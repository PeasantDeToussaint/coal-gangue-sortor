#include "ValveDriverMock.h"

#ifdef CGS_HAS_BECKHOFF_ADS
#include "ValveDriverEL2828.h"
#endif

#include <chrono>
#include <thread>

namespace cgs {
namespace hardware {

ValveDriverMock::ValveDriverMock() = default;
ValveDriverMock::~ValveDriverMock() { close(); }

bool ValveDriverMock::open(const std::string& /*portOrConfig*/) {
    m_open = true;
    return true;
}

void ValveDriverMock::close() {
    disarm();
    m_open = false;
}

bool ValveDriverMock::arm() {
    if (!m_open) return false;
    m_armed = true;
    return true;
}

bool ValveDriverMock::disarm() {
    m_armed = false;
    return true;
}

bool ValveDriverMock::schedule(const std::vector<NozzleCommand>& batch) {
    if (!m_armed) {
        m_rejected += batch.size();
        if (m_fault) m_fault("not armed");
        return false;
    }
    std::lock_guard<std::mutex> lk(m_mtx);
    for (const auto& cmd : batch) {
        if (cmd.nozzleId < 1 || cmd.nozzleId > 64) {
            ++m_rejected;
            if (m_fault) m_fault("invalid nozzle id");
            continue;
        }
        ++m_accepted;
        if (m_log) m_log(cmd);
    }
    return true;
}

bool ValveDriverMock::selfTest(uint32_t pulseMs) {
    if (!m_armed) return false;
    using clock = std::chrono::steady_clock;
    const auto base = clock::now();
    std::vector<NozzleCommand> batch;
    batch.reserve(64);
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

ValveDriverStatus ValveDriverMock::pollStatus() {
    ValveDriverStatus s;
    s.connected = m_open;
    s.armed = m_armed;
    s.channelCount = 64;
    s.commandsAccepted = m_accepted;
    s.commandsRejected = m_rejected;
    return s;
}

void ValveDriverMock::setFaultCallback(FaultCallback cb) { m_fault = std::move(cb); }

std::unique_ptr<IValveDriver> createValveDriver(const std::string& type) {
    if (type == "mock" || type.empty()) return std::make_unique<ValveDriverMock>();
#ifdef CGS_HAS_BECKHOFF_ADS
    if (type == "el2828" || type == "beckhoff") return std::make_unique<ValveDriverEL2828>();
#endif
    return nullptr;
}

}}
