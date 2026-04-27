#include "test_harness.h"

#include "../core/Pipeline/PipelineEngine.h"
#include "../hardware/ValveDriverInterface/ValveDriverMock.h"

#include <atomic>
#include <vector>

using namespace cgs;

static hardware::DetectorFrame makeFrame(uint64_t id, int width, uint16_t fill) {
    hardware::DetectorFrame f;
    f.frameId = id;
    f.timestampNs = id * 1'000'000ULL;
    f.width = width;
    f.height = 1;
    f.bitsPerPixel = 16;
    f.data.assign((size_t)width, fill);
    return f;
}

TEST_CASE("pipeline_processes_empty_frame_no_fires") {
    core::PipelineEngine eng;
    core::PipelineConfig cfg;
    eng.setConfig(cfg);
    auto f = makeFrame(1, 1024, 60000); // empty belt
    eng.processFrame(f);
    auto s = eng.stats();
    EXPECT_EQ(s.framesProcessed, 1u);
    EXPECT_EQ(s.gangueRejected, 0u);
    EXPECT_EQ(s.commandsScheduled, 0u);
}

TEST_CASE("pipeline_detects_gangue_and_schedules_when_armed") {
    core::PipelineEngine eng;
    core::PipelineConfig cfg;
    cfg.nozzles.pixelsPerNozzle = 16.0;
    eng.setConfig(cfg);

    hardware::ValveDriverMock valves;
    valves.open("test");
    valves.arm();

    std::atomic<int> captured{0};
    valves.setFireLog([&](const hardware::NozzleCommand&){ ++captured; });

    eng.attach(nullptr, nullptr, &valves, nullptr, nullptr);
    eng.arm();

    auto f = makeFrame(1, 1024, 60000);
    for (int i = 100; i < 200; ++i) f.data[i] = 15000; // gangue band
    eng.processFrame(f);

    EXPECT_TRUE(captured > 0);
    auto s = eng.stats();
    EXPECT_TRUE(s.gangueRejected > 0u);
    EXPECT_TRUE(s.commandsScheduled > 0u);
}

TEST_CASE("pipeline_disarmed_does_not_schedule") {
    core::PipelineEngine eng;
    eng.setConfig({});
    hardware::ValveDriverMock valves;
    valves.open("test");
    valves.arm();
    std::atomic<int> captured{0};
    valves.setFireLog([&](const hardware::NozzleCommand&){ ++captured; });
    eng.attach(nullptr, nullptr, &valves, nullptr, nullptr);
    // engine NOT armed

    auto f = makeFrame(1, 1024, 60000);
    for (int i = 100; i < 200; ++i) f.data[i] = 15000;
    eng.processFrame(f);

    EXPECT_EQ(captured.load(), 0);
    auto s = eng.stats();
    EXPECT_TRUE(s.gangueRejected > 0u);          // still detected
    EXPECT_EQ(s.commandsScheduled, 0u);          // but not fired
}

TEST_CASE("pipeline_snapshot_callback_fires") {
    core::PipelineEngine eng;
    eng.setConfig({});
    std::atomic<int> calls{0};
    eng.setSnapshotCallback([&](const core::PipelineFrameSnapshot&){ ++calls; });
    auto f = makeFrame(1, 256, 60000);
    eng.processFrame(f);
    EXPECT_EQ(calls.load(), 1);
}
