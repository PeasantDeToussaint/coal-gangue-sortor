#include "test_harness.h"

#include "../core/Pipeline/PipelineEngine.h"
#include "../hardware/ValveDriverInterface/ValveDriverMock.h"

#include <atomic>
#include <vector>

using namespace cgs;

// Helper: build a single-line DetectorFrame with a uniform fill value.
static hardware::DetectorFrame makeFrame(uint64_t id, int width, uint16_t fill) {
    hardware::DetectorFrame f;
    f.frameId      = id;
    f.timestampNs  = id * 1'000'000ULL;
    f.width        = width;
    f.height       = 1;
    f.bitsPerPixel = 16;
    f.data.assign((size_t)width, fill);
    return f;
}

// Convenience: build a config that fires after exactly 1 line so tests
// do not have to push 1150 lines to trigger the FrameAccumulator.
static core::PipelineConfig singleLineConfig() {
    core::PipelineConfig cfg;
    cfg.lineCount = 1;   // accumulate only 1 line before running inference
    cfg.defaultBeltSpeedMps = 2.035;
    return cfg;
}

TEST_CASE("pipeline_processes_empty_frame_no_fires") {
    core::PipelineEngine eng;
    auto cfg = singleLineConfig();
    eng.setConfig(cfg);

    auto f = makeFrame(1, 1024, 60000);  // 60000 >> xrayEmptyMin(50000) → empty belt
    eng.processFrame(f);

    auto s = eng.stats();
    EXPECT_EQ(s.framesProcessed, 1u);
    EXPECT_EQ(s.gangueRejected,   0u);
    EXPECT_EQ(s.commandsScheduled, 0u);
}

TEST_CASE("pipeline_detects_gangue_and_schedules_when_armed") {
    core::PipelineEngine eng;
    auto cfg = singleLineConfig();
    // Ensure threshold classifier can distinguish gangue (no model loaded in tests).
    // Default xrayGangueMax=25000; pixel value 15000 < 25000 → gangue.
    cfg.nozzles.pixelsPerNozzle = 16.0;
    cfg.nozzles.totalNozzles    = 64;
    cfg.nozzles.beltLeftPaddingPx  = 0;
    cfg.nozzles.beltRightPaddingPx = 0;
    eng.setConfig(cfg);

    hardware::ValveDriverMock valves;
    valves.open("test");
    valves.arm();

    std::atomic<int> captured{0};
    valves.setFireLog([&](const hardware::NozzleCommand&){ ++captured; });

    eng.attach(nullptr, nullptr, &valves, nullptr, nullptr);
    eng.arm();

    // Build frame with a gangue band at columns 100..199.
    auto f = makeFrame(1, 1024, 60000);
    for (int i = 100; i < 200; ++i) f.data[i] = 15000;
    eng.processFrame(f);

    // ValveDriverMock executes fire immediately in its schedule().
    EXPECT_TRUE(captured > 0);
    auto s = eng.stats();
    EXPECT_TRUE(s.gangueRejected    > 0u);
    EXPECT_TRUE(s.commandsScheduled > 0u);
}

TEST_CASE("pipeline_disarmed_does_not_schedule") {
    core::PipelineEngine eng;
    auto cfg = singleLineConfig();
    eng.setConfig(cfg);

    hardware::ValveDriverMock valves;
    valves.open("test");
    valves.arm();
    std::atomic<int> captured{0};
    valves.setFireLog([&](const hardware::NozzleCommand&){ ++captured; });
    eng.attach(nullptr, nullptr, &valves, nullptr, nullptr);
    // engine NOT armed — detections should occur but no valves fire.

    auto f = makeFrame(1, 1024, 60000);
    for (int i = 100; i < 200; ++i) f.data[i] = 15000;
    eng.processFrame(f);

    EXPECT_EQ(captured.load(), 0);
    auto s = eng.stats();
    EXPECT_TRUE(s.gangueRejected    > 0u);   // classifier still detects
    EXPECT_EQ(s.commandsScheduled,   0u);    // but nothing fired
}

TEST_CASE("pipeline_snapshot_callback_fires") {
    core::PipelineEngine eng;
    auto cfg = singleLineConfig();
    eng.setConfig(cfg);

    std::atomic<int> calls{0};
    eng.setSnapshotCallback([&](const core::PipelineFrameSnapshot&){ ++calls; });

    auto f = makeFrame(1, 256, 60000);
    eng.processFrame(f);

    EXPECT_EQ(calls.load(), 1);
}

TEST_CASE("pipeline_accumulates_lines_before_firing") {
    core::PipelineEngine eng;
    core::PipelineConfig cfg;
    cfg.lineCount = 3;  // need 3 lines before inference runs
    cfg.defaultBeltSpeedMps = 2.035;
    eng.setConfig(cfg);

    std::atomic<int> snapshots{0};
    eng.setSnapshotCallback([&](const core::PipelineFrameSnapshot&){ ++snapshots; });

    // Push 2 lines — snapshot should NOT have fired yet.
    eng.processFrame(makeFrame(1, 128, 60000));
    eng.processFrame(makeFrame(2, 128, 60000));
    EXPECT_EQ(snapshots.load(), 0);

    // Push the 3rd line — now a complete frame is available.
    eng.processFrame(makeFrame(3, 128, 60000));
    EXPECT_EQ(snapshots.load(), 1);
    EXPECT_EQ(eng.stats().framesProcessed, 1u);
}
