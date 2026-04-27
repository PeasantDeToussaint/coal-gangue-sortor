#include "ValveDriverEL2828.h"

#ifdef CGS_HAS_BECKHOFF_ADS

#include <AdsLib.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace cgs {
namespace hardware {

ValveDriverEL2828::ValveDriverEL2828() = default;
ValveDriverEL2828::~ValveDriverEL2828() { close(); }

static bool parseEndpoint(const std::string& s, std::string& ip,
                          AmsNetId& netId, uint16_t& port) {
    const std::string scheme = "ads://";
    if (s.rfind(scheme, 0) != 0) return false;
    std::string rest = s.substr(scheme.size());
    auto p1 = rest.find(':');
    if (p1 == std::string::npos) return false;
    ip = rest.substr(0, p1);
    rest = rest.substr(p1 + 1);
    auto p2 = rest.rfind(':');
    if (p2 == std::string::npos) return false;
    const std::string netIdStr = rest.substr(0, p2);
    port = (uint16_t)std::stoi(rest.substr(p2 + 1));
    int v[6] = {0};
    if (std::sscanf(netIdStr.c_str(), "%d.%d.%d.%d.%d.%d",
                    &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]) != 6) return false;
    for (int i = 0; i < 6; ++i) netId.b[i] = (uint8_t)v[i];
    return true;
}

bool ValveDriverEL2828::open(const std::string& endpoint) {
    if (m_open) return false;
    std::string ip;
    AmsNetId net;
    uint16_t port = 851;
    if (!parseEndpoint(endpoint, ip, net, port)) return false;
    AdsAddRoute(net, ip.c_str());
    try {
        m_dev = std::make_unique<AdsDevice>(ip, net, port);
    } catch (...) {
        m_dev.reset();
        return false;
    }
    m_open = true;
    return true;
}

void ValveDriverEL2828::close() {
    disarm();
    m_dev.reset();
    m_handleCache.clear();
    m_open = false;
}

bool ValveDriverEL2828::arm() {
    if (!m_open) return false;
    if (m_armed.exchange(true)) return false;
    m_schedulerRun = true;
    m_scheduler = std::thread([this]{ schedulerLoop(); });
    return true;
}

bool ValveDriverEL2828::disarm() {
    if (!m_armed.exchange(false)) return false;
    m_schedulerRun = false;
    if (m_scheduler.joinable()) m_scheduler.join();
    {
        std::lock_guard<std::mutex> lk(m_pendingMtx);
        for (const auto& c : m_pending) writeChannel(c.nozzleId, false);
        m_pending.clear();
    }
    return true;
}

bool ValveDriverEL2828::writeChannel(int nozzleId, bool on) {
    if (!m_dev) return false;
    char varName[64];
    std::snprintf(varName, sizeof(varName), m_chTemplate.c_str(), nozzleId);
    try {
        auto h = m_dev->GetHandle(varName);
        uint8_t v = on ? 1 : 0;
        m_dev->WriteReqEx(0xF005, *h, sizeof(v), &v);
        return true;
    } catch (...) { return false; }
}

bool ValveDriverEL2828::schedule(const std::vector<NozzleCommand>& batch) {
    if (!m_armed) {
        m_rejected += batch.size();
        if (m_fault) m_fault("not armed");
        return false;
    }
    std::lock_guard<std::mutex> lk(m_pendingMtx);
    for (const auto& c : batch) {
        if (c.nozzleId < 1 || c.nozzleId > 64) {
            ++m_rejected;
            if (m_fault) m_fault("invalid nozzle id");
            continue;
        }
        m_pending.push_back(c);
        ++m_accepted;
    }
    std::sort(m_pending.begin(), m_pending.end(),
              [](const NozzleCommand& a, const NozzleCommand& b){
                  return a.fireAtNs < b.fireAtNs;
              });
    return true;
}

bool ValveDriverEL2828::selfTest(uint32_t pulseMs) {
    if (!m_armed) return false;
    using clock = std::chrono::steady_clock;
    auto base = clock::now();
    std::vector<NozzleCommand> batch;
    for (int i = 1; i <= 64; ++i) {
        NozzleCommand c;
        c.nozzleId = i;
        c.fireAtNs = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                         (base + std::chrono::milliseconds((i - 1) * pulseMs * 2)).time_since_epoch()).count();
        c.durationMs = pulseMs;
        batch.push_back(c);
    }
    return schedule(batch);
}

ValveDriverStatus ValveDriverEL2828::pollStatus() {
    ValveDriverStatus s;
    s.connected = m_open;
    s.armed = m_armed;
    s.channelCount = 64;
    s.commandsAccepted = m_accepted;
    s.commandsRejected = m_rejected;
    return s;
}

void ValveDriverEL2828::setFaultCallback(FaultCallback cb) { m_fault = std::move(cb); }

// 1ms scheduler — matches EtherCAT cycle. Walks pending list, fires what's due,
// then schedules the OFF write at the right moment.
void ValveDriverEL2828::schedulerLoop() {
    using clock = std::chrono::steady_clock;
    struct OffEvent { int nozzleId; uint64_t atNs; };
    std::vector<OffEvent> offQueue;

    while (m_schedulerRun) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const uint64_t now = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 clock::now().time_since_epoch()).count();
        // 1) Fire any pending whose time has come.
        {
            std::lock_guard<std::mutex> lk(m_pendingMtx);
            while (!m_pending.empty() && m_pending.front().fireAtNs <= now) {
                const auto cmd = m_pending.front();
                m_pending.erase(m_pending.begin());
                if (writeChannel(cmd.nozzleId, true)) {
                    offQueue.push_back({cmd.nozzleId, now + (uint64_t)cmd.durationMs * 1'000'000ULL});
                } else {
                    ++m_rejected;
                    if (m_fault) m_fault("ADS write failed");
                }
            }
        }
        // 2) Turn off any channel whose burst has ended.
        offQueue.erase(
            std::remove_if(offQueue.begin(), offQueue.end(),
                           [this, now](const OffEvent& e) {
                               if (e.atNs <= now) {
                                   writeChannel(e.nozzleId, false);
                                   return true;
                               }
                               return false;
                           }),
            offQueue.end());
    }
}

}}

#endif // CGS_HAS_BECKHOFF_ADS
