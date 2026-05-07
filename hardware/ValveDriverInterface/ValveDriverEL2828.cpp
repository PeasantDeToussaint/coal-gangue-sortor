#include "ValveDriverEL2828.h"

#ifdef CGS_HAS_BECKHOFF_ADS

#include <AdsLib.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace cgs {
namespace hardware {

// Internal connection bundle — keeps AmsAddr / ADS port out of the public header.
struct ValveDriverEL2828::AdsConn {
    AmsAddr addr{};
    long    port{0};
};

ValveDriverEL2828::ValveDriverEL2828() = default;

ValveDriverEL2828::~ValveDriverEL2828() {
    close();
}

// ---------------------------------------------------------------------------
// Endpoint parser
// Format: ads://<routerIp>:<n1.n2.n3.n4.n5.n6>:<port>[?ig=0x...&io=0x...&ch=N]
// ---------------------------------------------------------------------------
bool ValveDriverEL2828::parseEndpoint(const std::string& s,
                                      std::string& ip,
                                      std::string& netIdStr,
                                      uint16_t& port,
                                      uint32_t& indexGroup,
                                      uint32_t& indexOffset,
                                      int& channelCount) {
    // Defaults
    indexGroup   = 0x3040030;
    indexOffset  = 0x81000006;
    channelCount = 136;
    port         = 851;

    const std::string scheme = "ads://";
    if (s.rfind(scheme, 0) != 0) return false;

    // Split off query string first
    std::string main = s.substr(scheme.size());
    std::string query;
    auto qpos = main.find('?');
    if (qpos != std::string::npos) {
        query = main.substr(qpos + 1);
        main  = main.substr(0, qpos);
    }

    // main = "<ip>:<netid>:<port>"
    auto p1 = main.find(':');
    if (p1 == std::string::npos) return false;
    ip = main.substr(0, p1);
    std::string rest = main.substr(p1 + 1);

    auto p2 = rest.rfind(':');
    if (p2 == std::string::npos) return false;
    netIdStr = rest.substr(0, p2);
    port = (uint16_t)std::stoi(rest.substr(p2 + 1));

    // Parse query params
    std::istringstream qs(query);
    std::string token;
    while (std::getline(qs, token, '&')) {
        auto eq = token.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = token.substr(0, eq);
        const std::string val = token.substr(eq + 1);
        if (key == "ig")  indexGroup   = (uint32_t)std::stoul(val, nullptr, 0);
        if (key == "io")  indexOffset  = (uint32_t)std::stoul(val, nullptr, 0);
        if (key == "ch")  channelCount = std::stoi(val);
    }
    return true;
}

// ---------------------------------------------------------------------------
// open / close
// ---------------------------------------------------------------------------
bool ValveDriverEL2828::open(const std::string& endpoint) {
    if (m_open) return false;

    std::string ip, netIdStr;
    uint16_t port = 851;
    uint32_t ig, io;
    int ch;

    if (!parseEndpoint(endpoint, ip, netIdStr, port, ig, io, ch))
        return false;

    m_indexGroup   = ig;
    m_indexOffset  = io;
    m_channelCount = ch;

    // Parse net ID "n1.n2.n3.n4.n5.n6"
    AmsNetId netId{};
    int v[6] = {0};
    if (std::sscanf(netIdStr.c_str(), "%d.%d.%d.%d.%d.%d",
                    &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
        return false;
    for (int i = 0; i < 6; ++i) netId.b[i] = (uint8_t)v[i];

    AdsAddRoute(netId, ip.c_str());

    m_conn = std::make_unique<AdsConn>();
    m_conn->port = AdsPortOpen();
    if (m_conn->port == 0) {
        m_conn.reset();
        return false;
    }

    m_conn->addr.netId = netId;
    m_conn->addr.port  = (uint16_t)port;

    // Verify connectivity: write all-zeros as a probe
    std::vector<uint8_t> zero(m_channelCount, 0);
    long err = AdsSyncWriteReq(&m_conn->addr,
                               m_indexGroup, m_indexOffset,
                               (uint32_t)zero.size(), zero.data());
    if (err != 0) {
        AdsPortClose(m_conn->port);
        m_conn.reset();
        return false;
    }

    m_open = true;
    return true;
}

void ValveDriverEL2828::close() {
    disarm();
    if (m_conn && m_conn->port != 0) {
        // Zero all channels on close
        std::vector<uint8_t> zero(m_channelCount, 0);
        AdsSyncWriteReq(&m_conn->addr,
                        m_indexGroup, m_indexOffset,
                        (uint32_t)zero.size(), zero.data());
        AdsPortClose(m_conn->port);
    }
    m_conn.reset();
    m_open = false;
}

// ---------------------------------------------------------------------------
// arm / disarm
// ---------------------------------------------------------------------------
bool ValveDriverEL2828::arm() {
    if (!m_open) return false;
    if (m_armed.exchange(true)) return false;  // already armed
    m_schedulerRun = true;
    m_scheduler = std::thread([this]{ schedulerLoop(); });
    return true;
}

bool ValveDriverEL2828::disarm() {
    if (!m_armed.exchange(false)) return false;
    m_schedulerRun = false;
    if (m_scheduler.joinable()) m_scheduler.join();

    // Flush and zero everything
    {
        std::lock_guard<std::mutex> lk(m_pendingMtx);
        m_pending.clear();
    }
    if (m_open) {
        std::vector<uint8_t> zero(m_channelCount, 0);
        bulkWrite(zero);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Bulk write — THE critical function.
// All channels written in a single ADS call (< 1 ms on EtherCAT 1 ms cycle).
// ---------------------------------------------------------------------------
bool ValveDriverEL2828::bulkWrite(const std::vector<uint8_t>& outputStates) {
    if (!m_conn) return false;
    long err = AdsSyncWriteReq(&m_conn->addr,
                               m_indexGroup, m_indexOffset,
                               (uint32_t)outputStates.size(),
                               const_cast<uint8_t*>(outputStates.data()));
    if (err != 0) {
        if (m_fault) m_fault("AdsSyncWriteReq failed: " + std::to_string(err));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// schedule
// ---------------------------------------------------------------------------
bool ValveDriverEL2828::schedule(const std::vector<NozzleCommand>& batch) {
    if (!m_armed) {
        m_rejected += batch.size();
        if (m_fault) m_fault("schedule called but not armed");
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(m_pendingMtx);
        for (const auto& c : batch) {
            if (c.nozzleId < 1 || c.nozzleId > m_channelCount) {
                ++m_rejected;
                if (m_fault) m_fault("nozzle id out of range: " + std::to_string(c.nozzleId));
                continue;
            }
            m_pending.push_back(c);
            ++m_accepted;
        }
        std::sort(m_pending.begin(), m_pending.end(),
                  [](const NozzleCommand& a, const NozzleCommand& b){
                      return a.fireAtNs < b.fireAtNs;
                  });
    }
    return true;
}

// ---------------------------------------------------------------------------
// selfTest — fires each nozzle individually in sequence (commissioning tool).
// Matches original BeckHoff.cpp test loop exactly.
// ---------------------------------------------------------------------------
bool ValveDriverEL2828::selfTest(uint32_t pulseMs) {
    if (!m_armed) return false;
    using clock = std::chrono::steady_clock;
    auto base = clock::now();

    std::vector<NozzleCommand> batch;
    batch.reserve(m_channelCount);
    for (int i = 1; i <= m_channelCount; ++i) {
        NozzleCommand c;
        c.nozzleId  = i;
        c.fireAtNs  = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                          (base + std::chrono::milliseconds((long long)(i - 1) * pulseMs * 2))
                          .time_since_epoch()).count();
        c.durationMs = pulseMs;
        batch.push_back(c);
    }
    return schedule(batch);
}

// ---------------------------------------------------------------------------
// pollStatus
// ---------------------------------------------------------------------------
ValveDriverStatus ValveDriverEL2828::pollStatus() {
    ValveDriverStatus s;
    s.connected         = m_open;
    s.armed             = m_armed;
    s.channelCount      = m_channelCount;
    s.commandsAccepted  = m_accepted;
    s.commandsRejected  = m_rejected;
    return s;
}

void ValveDriverEL2828::setFaultCallback(FaultCallback cb) {
    m_fault = std::move(cb);
}

// ---------------------------------------------------------------------------
// Scheduler loop — 1 ms tick matching EtherCAT cycle.
//
// Key algorithm:
//   Each tick, build the full output array (all channels false by default),
//   set channels whose ON window encompasses [now, now+1ms] to true,
//   then issue ONE bulk write for the entire bar.
//
// This means we never fire a "writeChannel per nozzle" round-trip — we always
// send one AdsSyncWriteReq regardless of how many nozzles are active.
// ---------------------------------------------------------------------------
void ValveDriverEL2828::schedulerLoop() {
    using clock = std::chrono::steady_clock;

    struct ActiveChannel {
        int      nozzleId;
        uint64_t offAtNs;
    };
    std::vector<ActiveChannel> active;

    while (m_schedulerRun) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        const uint64_t now = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  clock::now().time_since_epoch()).count();

        // 1) Promote commands whose fire time has arrived into active list.
        {
            std::lock_guard<std::mutex> lk(m_pendingMtx);
            while (!m_pending.empty() && m_pending.front().fireAtNs <= now) {
                const auto& cmd = m_pending.front();
                const uint64_t offAt = now + (uint64_t)cmd.durationMs * 1'000'000ULL;
                active.push_back({cmd.nozzleId, offAt});
                m_pending.erase(m_pending.begin());
            }
        }

        // 2) Remove channels whose burst has ended.
        active.erase(
            std::remove_if(active.begin(), active.end(),
                           [now](const ActiveChannel& a){ return a.offAtNs <= now; }),
            active.end());

        // 3) Build the output array and issue ONE bulk write.
        std::vector<uint8_t> out(m_channelCount, 0);
        for (const auto& a : active) {
            const int idx = a.nozzleId - 1;  // nozzleId is 1-based
            if (idx >= 0 && idx < m_channelCount) out[idx] = 1;
        }
        bulkWrite(out);
    }

    // Zero all channels when scheduler exits
    std::vector<uint8_t> zero(m_channelCount, 0);
    bulkWrite(zero);
}

}}

#endif // CGS_HAS_BECKHOFF_ADS
