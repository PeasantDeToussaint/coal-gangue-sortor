#ifndef VALVEDRIVERMOCK_H
#define VALVEDRIVERMOCK_H

#include "IValveDriver.h"

#include <atomic>
#include <mutex>

namespace cgs {
namespace hardware {

// Logs every scheduled command to stdout / callback. Useful for verifying
// that the upper layer (Classifier -> NozzleMapper -> ValveDriver) makes
// sensible decisions before connecting real hardware.
class ValveDriverMock : public IValveDriver {
public:
    ValveDriverMock();
    ~ValveDriverMock() override;

    bool open(const std::string& portOrConfig) override;
    void close() override;

    bool arm() override;
    bool disarm() override;

    bool schedule(const std::vector<NozzleCommand>& batch) override;
    bool selfTest(uint32_t pulseMs) override;

    ValveDriverStatus pollStatus() override;

    void setFaultCallback(FaultCallback cb) override;

    using FireLog = std::function<void(const NozzleCommand&)>;
    void setFireLog(FireLog cb) { m_log = std::move(cb); }

private:
    std::atomic<bool> m_open{false};
    std::atomic<bool> m_armed{false};
    std::atomic<uint64_t> m_accepted{0};
    std::atomic<uint64_t> m_rejected{0};
    FaultCallback m_fault;
    FireLog m_log;
    std::mutex m_mtx;
};

}}

#endif
