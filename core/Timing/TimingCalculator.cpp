#include "TimingCalculator.h"

#include <algorithm>

namespace cgs {
namespace core {

double TimingCalculator::computeFireDelayMs(double beltSpeedMps) const {
    if (beltSpeedMps <= 0.0) return -1.0;
    const double travelMs = (m_cfg.sensorToNozzleMm / 1000.0 / beltSpeedMps) * 1000.0;
    const double delay = travelMs
                       - m_cfg.valveOpenLatencyMs
                       - m_cfg.pneumaticTravelMs
                       - m_cfg.safetyMarginMs;
    return delay;
}

double TimingCalculator::computeBurstDurationMs(double particleLengthMm, double beltSpeedMps) const {
    const double clampedLen = std::max(0.0, particleLengthMm);
    if (beltSpeedMps <= 0.0) return 30.0;
    const double passMs = (clampedLen / 1000.0 / beltSpeedMps) * 1000.0;
    return std::max(20.0, passMs + m_cfg.valveCloseLatencyMs);
}

uint64_t TimingCalculator::fireAtNs(uint64_t nowNs, double delayMs) {
    if (delayMs < 0) delayMs = 0;
    return nowNs + (uint64_t)(delayMs * 1.0e6);
}

}}
