#ifndef CGS_TIMINGCALCULATOR_H
#define CGS_TIMINGCALCULATOR_H

#include <cstdint>

namespace cgs {
namespace core {

// All distances in millimetres, time in milliseconds, speed in metres/sec.
struct TimingConfig {
    // DQ: detector face to air-nozzle centre, mm.
    // Real machine: 2498 mm (from config.xml DQ="2498").
    double sensorToNozzleMm = 2498.0;

    // TimerGap: pre-calibrated fire delay in ms (TimerGap="2456" in real config.xml).
    // Confirmed from Qt_OpenCV_Image_Processing.dll decompilation: the original software
    // does NOT compute delay dynamically from DQ/speed. It uses a fixed TimerGap value
    // set during commissioning (disk experiment). When > 0, this overrides the
    // dynamic DQ/speed formula.
    double timerGapMs = 2456.0;

    double valveOpenLatencyMs = 8.0;     // EL2828 + solenoid open response
    double valveCloseLatencyMs = 6.0;    // EL2828 + solenoid close response
    double pneumaticTravelMs = 2.0;      // air-burst time-of-flight to belt
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
