// Console entry point — runs the full Mock pipeline through PipelineEngine
// without Qt. CI smoke test + early dev mode.

#include "../core/Pipeline/PipelineEngine.h"
#include "../hardware/CameraInterface/CameraMock.h"
#include "../hardware/DetectorInterface/DetectorMock.h"
#include "../hardware/PLCInterface/PLCMock.h"
#include "../hardware/ValveDriverInterface/ValveDriverMock.h"
#include "../hardware/XRayInterface/XRayMock.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using namespace cgs;

int main(int argc, char** argv) {
    bool runForever = false;
    int durationSeconds = 5;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mock") { /* default */ }
        else if (arg == "--forever") runForever = true;
        else if (arg == "--seconds" && i + 1 < argc) durationSeconds = std::atoi(argv[++i]);
    }

    std::printf("==> coal-gangue-sorter (mock pipeline via PipelineEngine)\n");

    auto xray     = hardware::createXRaySource("mock");
    auto detector = hardware::createDetector("mock");
    auto camera   = hardware::createCamera("mock");
    auto valves   = hardware::createValveDriver("mock");
    auto plc      = hardware::createPLC("mock");

    xray->open("");
    xray->startup();

    hardware::DetectorConfig dcfg;
    dcfg.width = 1024;
    dcfg.lineRateHz = 100;
    detector->open(dcfg);

    hardware::CameraConfig ccfg;
    ccfg.width = 1024; ccfg.height = 64; ccfg.frameRateHz = 10;
    camera->open(ccfg);

    valves->open("mock");
    valves->arm();
    plc->connect("mock://localhost");
    plc->setConveyor(hardware::ConveyorId::DetectionBelt, true);
    plc->setBeltSpeedHz(40.0);

    core::PipelineEngine engine;
    core::PipelineConfig pcfg;
    pcfg.nozzles.totalNozzles = 64;
    pcfg.nozzles.firstNozzleId = 1;
    pcfg.nozzles.pixelsPerNozzle = 1024.0 / 64.0;
    engine.setConfig(pcfg);
    engine.attach(detector.get(), camera.get(), valves.get(), plc.get(), xray.get());
    engine.arm();

    engine.setSnapshotCallback([&](const core::PipelineFrameSnapshot& snap){
        if (!snap.firedNozzleIds.empty()) {
            std::string ids;
            for (int n : snap.firedNozzleIds) {
                if (!ids.empty()) ids += ",";
                ids += std::to_string(n);
            }
            std::printf("[frame %lu] gangue → fire nozzles: %s\n",
                        (unsigned long)snap.frameId, ids.c_str());
        }
    });

    detector->start();
    camera->startStreaming();

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const auto s = engine.stats();
        std::printf("== status frames=%lu segments=%lu rejects=%lu fired=%lu belt=%.2f m/s ==\n",
                    (unsigned long)s.framesProcessed,
                    (unsigned long)s.segmentsDetected,
                    (unsigned long)s.gangueRejected,
                    (unsigned long)s.commandsScheduled,
                    s.currentBeltSpeedMps);
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
