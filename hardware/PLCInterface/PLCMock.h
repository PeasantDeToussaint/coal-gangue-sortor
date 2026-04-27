#ifndef PLCMOCK_H
#define PLCMOCK_H

#include "IPLC.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace cgs {
namespace hardware {

// Simulates a PLC controlling 6 conveyors, one variable-frequency belt,
// emergency stop, three-color tower lamp, and a periodic material trigger
// (so the rest of the pipeline has something to react to).
class PLCMock : public IPLC {
public:
    PLCMock();
    ~PLCMock() override;

    bool connect(const std::string& endpoint) override;
    void disconnect() override;

    bool setConveyor(ConveyorId id, bool on) override;
    bool setBeltSpeedHz(double hz) override;
    bool setLamp(LampColor color, LampState state) override;

    PLCStatus pollStatus() override;

    void subscribeMaterialTrigger(TriggerCallback cb) override;
    void subscribeEmergencyStop(EmergencyCallback cb) override;

    // Test hooks
    void injectEmergencyStop();

private:
    void run();

    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_estop{false};
    std::atomic<double> m_beltHz{0.0};
    std::thread m_thread;
    std::mutex m_mtx;
    PLCStatus m_status;
    TriggerCallback m_triggerCb;
    EmergencyCallback m_estopCb;
};

}}

#endif
