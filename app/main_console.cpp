// Console entry point — runs the full Mock pipeline without Qt.
// Useful as a CI smoke test and for early development before UI exists.

#include "../core/Classifier/Classifier.h"
#include "../core/Fusion/FusionPolicy.h"
#include "../core/NozzleMapping/NozzleMapper.h"
#include "../core/Timing/TimingCalculator.h"
#include "../hardware/CameraInterface/CameraMock.h"
#include "../hardware/DetectorInterface/DetectorMock.h"
#include "../hardware/PLCInterface/PLCMock.h"
#include "../hardware/ValveDriverInterface/ValveDriverMock.h"
#include "../hardware/XRayInterface/XRayMock.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

using namespace cgs;


int main(int argc, char** argv) {
    bool runForever = false;
    int durationSeconds = 5;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mock") { /* default already mock */ }
        else if (arg == "--forever") runForever = true;
        else if (arg == "--seconds" && i + 1 < argc) durationSeconds = std::atoi(argv[++i]);
    }

    std::printf("==> coal-gangue-sorter (mock pipeline)\n");

    auto xray     = hardware::createXRaySource("mock");
    auto detector = hardware::createDetector("mock");
    auto camera   = hardware::createCamera("mock");
    auto valves   = hardware::createValveDriver("mock");
    auto plc      = hardware::createPLC("mock");

    xray->open("");
    xray->startup();

    hardware::DetectorConfig dcfg;
    dcfg.width = 1024;
    dcfg.lineRateHz = 100;            // slow down for console readability
    detector->open(dcfg);

    hardware::CameraConfig ccfg;
    ccfg.width = 1024;
    ccfg.height = 64;
    ccfg.frameRateHz = 10;
    camera->open(ccfg);

    valves->open("mock");
    valves->arm();
    plc->connect("mock://localhost");
    plc->setConveyor(hardware::ConveyorId::DetectionBelt, true);
    plc->setBeltSpeedHz(40.0);

    core::Classifier classifier;
    core::NozzleMapper mapper;          // default: 1024 px / 64 nozzles = 16 px/nozzle
    core::TimingCalculator timing;
    core::FusionPolicy fusion;

    std::atomic<uint64_t> frameCount{0};
    std::atomic<uint64_t> rejectCount{0};

    const uint64_t startNs = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now().time_since_epoch()).count();
    auto* mockValves = static_cast<hardware::ValveDriverMock*>(valves.get());
    mockValves->setFireLog([startNs](const hardware::NozzleCommand& c) {
        const double tMs = (c.fireAtNs > startNs)
                              ? (double)(c.fireAtNs - startNs) / 1.0e6 : 0.0;
        std::printf("    fire nozzle %02d  T+%.1f ms  duration %u ms\n",
                    c.nozzleId, tMs, c.durationMs);
    });

    detector->setFrameCallback([&](const hardware::DetectorFrame& f) {
        ++frameCount;
        const auto plcStatus = plc->pollStatus();
        const double belt = std::max(0.5, plcStatus.detectionBeltSpeedMps);

        const auto segs = classifier.classifyXRayRow(f.data.data(), f.width);
        bool printedHeader = false;
        for (const auto& s : segs) {
            const core::Material finalLabel =
                fusion.combine(s.label, core::Material::Unknown);
            if (finalLabel != core::Material::Gangue) continue;
            if (!printedHeader) {
                std::printf("[frame %lu] gangue detected -> scheduling fires:\n",
                            (unsigned long)f.frameId);
                printedHeader = true;
            }
            const auto noz = mapper.nozzlesForRange(s.startPx, s.endPx, f.width);
            const double delayMs = timing.computeFireDelayMs(belt);
            const double burstMs = timing.computeBurstDurationMs(
                (s.endPx - s.startPx) * (1024.0 / 1000.0 / f.width * 0.0 + 1.0), belt);
            std::vector<hardware::NozzleCommand> batch;
            for (int n : noz) {
                hardware::NozzleCommand c;
                c.nozzleId = n;
                c.fireAtNs = core::TimingCalculator::fireAtNs(f.timestampNs, std::max(0.0, delayMs));
                c.durationMs = (uint32_t)std::max(20.0, burstMs);
                batch.push_back(c);
            }
            valves->schedule(batch);
            ++rejectCount;
        }
    });

    detector->start();
    camera->startStreaming();

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto vs = valves->pollStatus();
        std::printf("== status frames=%lu rejects=%lu cmds_ok=%lu cmds_rej=%lu ==\n",
                    (unsigned long)frameCount.load(),
                    (unsigned long)rejectCount.load(),
                    (unsigned long)vs.commandsAccepted,
                    (unsigned long)vs.commandsRejected);
        if (!runForever && std::chrono::duration_cast<std::chrono::seconds>(
                clock::now() - t0).count() >= durationSeconds) break;
    }

    camera->stopStreaming();
    detector->stop();
    valves->disarm();
    valves->close();
    plc->disconnect();
    xray->shutdown();
    xray->close();

    std::printf("==> done.\n");
    return 0;
}
