#include "PLCMock.h"

#include <chrono>

namespace cgs {
namespace hardware {

PLCMock::PLCMock() = default;
PLCMock::~PLCMock() { disconnect(); }

bool PLCMock::connect(const std::string& /*endpoint*/) {
    if (m_connected) return false;
    m_connected = true;
    m_running = true;
    m_thread = std::thread([this]{ run(); });
    return true;
}

void PLCMock::disconnect() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    m_connected = false;
}

bool PLCMock::setConveyor(ConveyorId id, bool on) {
    std::lock_guard<std::mutex> lk(m_mtx);
    const int idx = static_cast<int>(id);
    if (idx < 0 || idx >= 6) return false;
    m_status.conveyorRunning[idx] = on;
    return true;
}

bool PLCMock::setBeltSpeedHz(double hz) {
    if (hz < 0.0 || hz > 60.0) return false;
    m_beltHz = hz;
    return true;
}

bool PLCMock::setLamp(LampColor /*color*/, LampState /*state*/) {
    // Mock just acknowledges; UI plugin will draw based on PLCStatus.
    return true;
}

PLCStatus PLCMock::pollStatus() {
    std::lock_guard<std::mutex> lk(m_mtx);
    PLCStatus s = m_status;
    s.connected = m_connected;
    s.emergencyStop = m_estop;
    s.airPressureOk = !m_estop;
    s.xrayInterlockOk = !m_estop;
    const double hz = m_beltHz.load();
    s.detectionBeltSpeedHz = hz;
    // Crude conversion: assume 50 Hz = 2.5 m/s nominal.
    s.detectionBeltSpeedMps = hz * (2.5 / 50.0);
    return s;
}

void PLCMock::subscribeMaterialTrigger(TriggerCallback cb) { m_triggerCb = std::move(cb); }
void PLCMock::subscribeEmergencyStop(EmergencyCallback cb) { m_estopCb = std::move(cb); }

void PLCMock::injectEmergencyStop() {
    m_estop = true;
    if (m_estopCb) m_estopCb();
}

void PLCMock::run() {
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    int counter = 0;
    while (m_running) {
        next += std::chrono::milliseconds(50);
        std::this_thread::sleep_until(next);

        // Simulate a material-trigger every ~500 ms while detection belt is running.
        const bool detectionOn = [&]{
            std::lock_guard<std::mutex> lk(m_mtx);
            return m_status.conveyorRunning[(int)ConveyorId::DetectionBelt];
        }();
        if (detectionOn && ++counter % 10 == 0) {
            const uint64_t ts = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    clock::now().time_since_epoch()).count();
            if (m_triggerCb) m_triggerCb(ts);
        }
    }
}

std::unique_ptr<IPLC> createPLC(const std::string& type) {
    if (type == "mock" || type.empty()) return std::make_unique<PLCMock>();
    return nullptr;
}

}}
