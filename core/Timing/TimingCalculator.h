#ifndef CGS_TIMINGCALCULATOR_H
#define CGS_TIMINGCALCULATOR_H

#include <cstdint>

namespace cgs {
namespace core {

// All distances in millimetres, time in milliseconds, speed in metres/sec.
struct TimingConfig {
    double sensorToNozzleMm = 860.0;     // physical distance, X-ray detector centre to nozzle centre
    double valveOpenLatencyMs = 8.0;     // DF8 typical open response
    double valveCloseLatencyMs = 6.0;    // DF8 typical close response
    double pneumaticTravelMs = 2.0;      // air-burst time-of-flight from nozzle to belt level
    double safetyMarginMs = 1.0;
};

class TimingCalculator {
public:
    explicit TimingCalculator(TimingConfig cfg = {}) : m_cfg(cfg) {}

    void setConfig(const TimingConfig& cfg) { m_cfg = cfg; }
    const TimingConfig& config() const { return m_cfg; }

    // Returns the wall-clock delay (ms) from the moment the X-ray detector
    // observes a piece of material to the moment the corresponding nozzle
    // command must START its electrical pulse. Negative result means the
    // belt is too fast for the configured latencies (alarm condition).
    double computeFireDelayMs(double beltSpeedMps) const;

    // Burst duration recommended to reliably eject a particle of given on-belt length.
    double computeBurstDurationMs(double particleLengthMm, double beltSpeedMps) const;

    // Convenience: convert (delay ms, now ns) to absolute fire timestamp ns.
    static uint64_t fireAtNs(uint64_t nowNs, double delayMs);

private:
    TimingConfig m_cfg;
};

}}

#endif
