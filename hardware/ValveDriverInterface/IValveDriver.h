#ifndef IVALVEDRIVER_H
#define IVALVEDRIVER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cgs {
namespace hardware {

// Single nozzle command: open valve N at absolute time T for D milliseconds.
// Production stack uses Beckhoff EL2828 via EtherCAT ADS; timing bounded by
// EtherCAT cycle (~1 ms). Real machine: 136 channels (QNum=136).
struct NozzleCommand {
    int nozzleId = 0;                    // 1..N (N = channelCount, typically 136)
    uint64_t fireAtNs = 0;               // absolute steady-clock timestamp (nanoseconds)
    uint32_t durationMs = 30;
};

struct ValveDriverStatus {
    bool connected = false;
    bool armed = false;
    int channelCount = 136;              // real machine: 136 (QNum=136)
    uint64_t commandsAccepted = 0;
    uint64_t commandsRejected = 0;
    std::string lastError;
};

class IValveDriver {
public:
    virtual ~IValveDriver() = default;

    virtual bool open(const std::string& portOrConfig) = 0;
    virtual void close() = 0;

    virtual bool arm() = 0;
    virtual bool disarm() = 0;

    // Schedule a batch of nozzle fires. Driver guarantees timing per spec.
    virtual bool schedule(const std::vector<NozzleCommand>& batch) = 0;

    // Synchronous self-test: fire each nozzle briefly in sequence, return false on fault.
    virtual bool selfTest(uint32_t pulseMs = 50) = 0;

    virtual ValveDriverStatus pollStatus() = 0;

    using FaultCallback = std::function<void(const std::string& reason)>;
    virtual void setFaultCallback(FaultCallback cb) = 0;
};

std::unique_ptr<IValveDriver> createValveDriver(const std::string& type);

}}

#endif
