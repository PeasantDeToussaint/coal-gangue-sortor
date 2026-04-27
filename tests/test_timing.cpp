#include "test_harness.h"

#include "../core/Timing/TimingCalculator.h"

using cgs::core::TimingCalculator;
using cgs::core::TimingConfig;

TEST_CASE("timing_basic_calculation") {
    TimingConfig cfg;
    cfg.sensorToNozzleMm = 1000.0;
    cfg.valveOpenLatencyMs = 8.0;
    cfg.pneumaticTravelMs = 2.0;
    cfg.safetyMarginMs = 1.0;
    TimingCalculator t(cfg);
    // At 2 m/s, 1000 mm takes 500 ms. Net delay = 500 - 8 - 2 - 1 = 489 ms.
    EXPECT_NEAR(t.computeFireDelayMs(2.0), 489.0, 0.001);
}

TEST_CASE("timing_zero_speed_returns_negative") {
    TimingCalculator t;
    EXPECT_TRUE(t.computeFireDelayMs(0.0) < 0);
    EXPECT_TRUE(t.computeFireDelayMs(-1.0) < 0);
}

TEST_CASE("timing_too_fast_belt_negative") {
    TimingConfig cfg;
    cfg.sensorToNozzleMm = 50.0;
    cfg.valveOpenLatencyMs = 100.0;
    TimingCalculator t(cfg);
    // 50 mm @ 2 m/s = 25 ms travel; valve needs 100 ms => delay negative => alarm.
    EXPECT_TRUE(t.computeFireDelayMs(2.0) < 0);
}

TEST_CASE("timing_burst_duration_minimum") {
    TimingCalculator t;
    EXPECT_NEAR(t.computeBurstDurationMs(0.0, 2.0), 20.0, 0.001);
    EXPECT_NEAR(t.computeBurstDurationMs(-5.0, 2.0), 20.0, 0.001);
}

TEST_CASE("timing_burst_duration_scales_with_length") {
    TimingConfig cfg;
    cfg.valveCloseLatencyMs = 0.0;
    TimingCalculator t(cfg);
    // 100 mm @ 1 m/s = 100 ms pass time; result = max(20, 100) = 100.
    EXPECT_NEAR(t.computeBurstDurationMs(100.0, 1.0), 100.0, 0.001);
}

TEST_CASE("timing_fire_at_ns_conversion") {
    EXPECT_EQ(TimingCalculator::fireAtNs(1'000'000'000ULL, 10.0), 1'010'000'000ULL);
    EXPECT_EQ(TimingCalculator::fireAtNs(0, -5.0), 0u);
}
