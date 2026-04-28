// Console entry point — runs the full Mock pipeline through PipelineEngine
// without Qt. CI smoke test + early dev mode.

#include "../core/Config/ConfigLoader.h"
#include "../core/Pipeline/PipelineEngine.h"
#include "../hardware/CameraInterface/ICamera.h"
#include "../hardware/DetectorInterface/IDetector.h"
#include "../hardware/PLCInterface/IPLC.h"
#include "../hardware/ValveDriverInterface/IValveDriver.h"
#include "../hardware/XRayInterface/IXRaySource.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>

using namespace cgs;

namespace {

std::string serialUri(const std::string& port) {
    if (port.empty()) return {};
    if (port.find("://") != std::string::npos) return port;
    return "serial://" + port;
}

std::string valveOpenArg(const core::HardwareConfig& hw) {
    if (hw.valveType == "mock") return "mock";
    if (!hw.valveEndpoint.empty()) return hw.valveEndpoint;
    return "mock";
}

template <typename T>
std::unique_ptr<T> orFallback(std::unique_ptr<T> p, const char* name,
                              std::function<std::unique_ptr<T>()> fallback) {
    if (p) return p;
    std::fprintf(stderr, "[config] factory returned null for %s — using mock.\n", name);
    return fallback();
}

} // namespace

int main(int argc, char** argv) {
    bool runForever = false;
    int durationSeconds = 5;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mock") { /* default */ }
        else if (arg == "--forever") runForever = true;
        else if (arg == "--real") { /* handled after config */ }
        else if (arg == "--seconds" && i + 1 < argc) durationSeconds = std::atoi(argv[++i]);
    }

    std::printf("==> coal-gangue-sorter (mock pipeline via PipelineEngine)\n");

    const std::string cfgPath = core::resolveConfigPath(argc, argv);
    core::PipelineConfig pcfg;
    core::HardwareConfig hw;
    bool loaded = core::loadConfigFromFile(cfgPath, pcfg, hw);
    if (loaded) {
        std::printf("[config] loaded: %s\n", cfgPath.c_str());
    } else {
        bool wantReal = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--real") wantReal = true;
        }
        core::applyBuiltinDefaults(wantReal, pcfg, hw);
        std::printf("[config] using built-in defaults (%s). Tried: %s\n",
                    wantReal ? "real-hardware preset" : "mock",
                    cfgPath.c_str());
    }

    auto xray = orFallback(hardware::createXRaySource(hw.xrayType), "xray",
                           [] { return hardware::createXRaySource("mock"); });
    auto detector = orFallback(hardware::createDetector(hw.detectorType), "detector",
                               [] { return hardware::createDetector("mock"); });
    auto camera = orFallback(hardware::createCamera(hw.cameraType), "camera",
                             [] { return hardware::createCamera("mock"); });
    auto valves = orFallback(hardware::createValveDriver(hw.valveType), "valves",
                             [] { return hardware::createValveDriver("mock"); });
    auto plc = orFallback(hardware::createPLC(hw.plcType), "plc",
                          [] { return hardware::createPLC("mock"); });

    if (xray) {
        const std::string xopen = (hw.xrayType == "mock" || hw.xrayType.empty())
                                        ? ""
                                        : serialUri(hw.xrayPort);
        xray->open(xopen);
    }
    if (xray) {
        xray->startup();
        if (hw.xrayType != "mock" && !hw.xrayType.empty()) {
            xray->setKv(hw.xrayKv);
            xray->setMa(hw.xrayMa);
        }
    }

    hardware::DetectorConfig dcfg;
    dcfg.width = hw.detectorWidth;
    dcfg.lineRateHz = hw.detectorLineRateHz;
    if (detector) detector->open(dcfg);

    hardware::CameraConfig ccfg;
    ccfg.ipOrSerial = hw.cameraIp;
    ccfg.width = hw.cameraWidth;
    ccfg.height = hw.cameraHeight;
    ccfg.exposureUs = hw.cameraExposureUs;
    ccfg.gainDb = hw.cameraGainDb;
    ccfg.frameRateHz = hw.cameraFrameRateHz;
    ccfg.hardwareTrigger = hw.cameraHardwareTrigger;
    if (camera) camera->open(ccfg);

    if (valves) {
        valves->open(valveOpenArg(hw));
        valves->arm();
    }
    if (plc) {
        plc->connect(hw.plcEndpoint);
        plc->setConveyor(hardware::ConveyorId::DetectionBelt, true);
        plc->setBeltSpeedHz(hw.plcInitialBeltHz);
    }

    core::PipelineEngine engine;
    engine.setConfig(pcfg);
    engine.attach(detector.get(), camera.get(), valves.get(), plc.get(), xray.get());
    engine.arm();

    engine.setSnapshotCallback([&](const core::PipelineFrameSnapshot& snap) {
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
                clock::now() - t0).count() >= durationSeconds)
            break;
    }

    camera->stopStreaming();
    detector->stop();
    if (valves) {
        valves->disarm();
        valves->close();
    }
    if (plc) plc->disconnect();
    if (xray) {
        xray->shutdown();
        xray->close();
    }
    std::printf("==> done.\n");
    return 0;
}
