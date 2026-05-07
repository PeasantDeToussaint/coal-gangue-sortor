#include "ValveDriverBeckHoffLib.h"

#ifdef CGS_HAS_BECKOFFLIB

#include "../../third_party/beckofflib/BeckHoffLibInterface.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sstream>

namespace cgs {
namespace hardware {

ValveDriverBeckHoffLib::ValveDriverBeckHoffLib() = default;

ValveDriverBeckHoffLib::~ValveDriverBeckHoffLib() {
    close();
}

bool ValveDriverBeckHoffLib::open(const std::string& endpoint) {
    // Parse optional "?ch=N&ig=0x..." from endpoint string
    m_channelCount = 136;
    m_indexGroup   = 0x3040030;
    if (!endpoint.empty() && endpoint != "beckofflib") {
        const auto chPos = endpoint.find("ch=");
        if (chPos != std::string::npos)
            m_channelCount = std::stoi(endpoint.substr(chPos + 3));
        const auto igPos = endpoint.find("ig=");
        if (igPos != std::string::npos)
            m_indexGroup = (uint32_t)std::stoul(endpoint.substr(igPos + 3), nullptr, 0);
    }

    BeckHoffLib* bhl = BeckHoffLib::getInstance();
    if (!bhl) {
        std::fprintf(stderr, "[ValveDriverBeckHoffLib] BeckHoffLib::getInstance() null\n");
        return false;
    }
    bhl->Init();

    // Probe: write all-zeros
    std::vector<bool> zero(m_channelCount, false);
    int err = bhl->writeRequest(m_indexGroup, zero.data());
    if (err != 0) {
        std::fprintf(stderr, "[ValveDriverBeckHoffLib] probe write failed: %d\n", err);
        return false;
    }
    m_open = true;
    return true;
}

void ValveDriverBeckHoffLib::close() {
    disarm();
    if (m_open) {
        std::vector<bool> zero(m_channelCount, false);
        if (auto* bhl = BeckHoffLib::getInstance())
            bhl->writeRequest(m_indexGroup, zero.data());
    }
    m_open = false;
}

bool ValveDriverBeckHoffLib::arm() {
    if (!m_open || m_armed.exchange(true)) return false;
    m_schedulerRun = true;
    m_scheduler = std::thread([this]{ schedulerLoop(); });
    return true;
}

bool ValveDriverBeckHoffLib::disarm() {
    if (!m_armed.exchange(false)) return false;
    m_schedulerRun = false;
    if (m_scheduler.joinable()) m_scheduler.join();
    std::lock_guard<std::mutex> lk(m_pendingMtx);
    m_pending.clear();
    return true;
}

bool ValveDriverBeckHoffLib::bulkWrite(const std::vector<uint8_t>& states) {
    auto* bhl = BeckHoffLib::getInstance();
    if (!bhl) return false;
    // Convert uint8 → bool array
    std::vector<bool> bools(states.size());
    for (size_t i = 0; i < states.size(); ++i) bools[i] = (states[i] != 0);
    int err = bhl->writeRequest(m_indexGroup, bools.data());
    if (err != 0 && m_fault)
        m_fault("BeckHoffLib::writeRequest failed: " + std::to_string(err));
    return err == 0;
}

bool ValveDriverBeckHoffLib::schedule(const std::vector<NozzleCommand>& batch) {
    if (!m_armed) { m_rejected += batch.size(); return false; }
    std::lock_guard<std::mutex> lk(m_pendingMtx);
    for (const auto& c : batch) {
        if (c.nozzleId < 1 || c.nozzleId > m_channelCount) { ++m_rejected; continue; }
        m_pending.push_back(c);
        ++m_accepted;
    }
    std::sort(m_pending.begin(), m_pending.end(),
              [](const NozzleCommand& a, const NozzleCommand& b){ return a.fireAtNs < b.fireAtNs; });
    return true;
}

ValveDriverStatus ValveDriverBeckHoffLib::pollStatus() {
    ValveDriverStatus s;
    s.connected        = m_open;
    s.armed            = m_armed;
    s.channelCount     = m_channelCount;
    s.commandsAccepted = m_accepted;
    s.commandsRejected = m_rejected;
    return s;
}

void ValveDriverBeckHoffLib::setFaultCallback(FaultCallback cb) { m_fault = std::move(cb); }

void ValveDriverBeckHoffLib::schedulerLoop() {
    using clock = std::chrono::steady_clock;
    struct ActiveChannel { int nozzleId; uint64_t offAtNs; };
    std::vector<ActiveChannel> active;

    while (m_schedulerRun) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const uint64_t now = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 clock::now().time_since_epoch()).count();
        {
            std::lock_guard<std::mutex> lk(m_pendingMtx);
            while (!m_pending.empty() && m_pending.front().fireAtNs <= now) {
                const auto& cmd = m_pending.front();
                active.push_back({cmd.nozzleId,
                                  now + (uint64_t)cmd.durationMs * 1'000'000ULL});
                m_pending.erase(m_pending.begin());
            }
        }
        active.erase(std::remove_if(active.begin(), active.end(),
                         [now](const ActiveChannel& a){ return a.offAtNs <= now; }),
                     active.end());

        std::vector<uint8_t> out(m_channelCount, 0);
        for (const auto& a : active) {
            const int idx = a.nozzleId - 1;
            if (idx >= 0 && idx < m_channelCount) out[idx] = 1;
        }
        bulkWrite(out);
    }
    std::vector<uint8_t> zero(m_channelCount, 0);
    bulkWrite(zero);
}

}} // cgs::hardware

#endif // CGS_HAS_BECKOFFLIB
