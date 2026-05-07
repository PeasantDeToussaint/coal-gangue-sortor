// Console entry point — runs the full pipeline through PipelineEngine without Qt.
// CI smoke test + early dev mode.
//
// Startup sequence mirrors the GUI entry point:
//   1. Init logger → data/gangue_sys.log
//   2. Load config
//   3. Construct hardware (real or mock)
//   4. Load inference engine (TRT / ONNX / none)
//   5. Arm and run for --seconds N (default 5) or --forever

#include "../core/Config/ConfigLoader.h"
#include "../core/Inference/IInferenceEngine.h"
#include "../core/Logging/Logger.h"
#include "../core/Pipeline/PipelineEngine.h"
#include "../hardware/CameraInterface/ICamera.h"
#include "../hardware/DetectorInterface/IDetector.h"
#include "../hardware/PLCInterface/IPLC.h"
#include "../hardware/PLCInterface/ACSMotionClient.h"
#include "../hardware/ValveDriverInterface/IValveDriver.h"
#include "../hardware/XRayInterface/IXRaySource.h"
#include "../hardware/XRayInterface/BeiduAdapter.h"

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
    int  durationSeconds = 5;
    bool wantReal = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--forever") runForever = true;
        else if (arg == "--real") wantReal = true;
        else if (arg == "--seconds" && i + 1 < argc)
            durationSeconds = std::atoi(argv[++i]);
    }

    // 1. Logger
    core::Logger::init("data/gangue_sys.log");
    CGS_LOG_INFO("-----coal-gangue-sorter console starting");
    std::printf("==> coal-gangue-sorter console (PipelineEngine)\n");

    // 2. Config
    const std::string cfgPath = core::resolveConfigPath(argc, argv);
    core::PipelineConfig pcfg;
    core::HardwareConfig hw;
    const bool loaded = core::loadConfigFromFile(cfgPath, pcfg, hw);
    if (loaded) {
        std::printf("[config] loaded: %s\n", cfgPath.c_str());
        CGS_LOG_INFO("config loaded: {}", cfgPath);
    } else {
        core::applyBuiltinDefaults(wantReal, pcfg, hw);
        std::printf("[config] using built-in %s defaults. Tried: %s\n",
                    wantReal ? "real-hardware" : "mock", cfgPath.c_str());
        CGS_LOG_WARN("config not found — using {} defaults", wantReal ? "real" : "mock");
    }

    // 3. Hardware
    auto xray     = orFallback(hardware::createXRaySource(hw.xrayType),   "xray",
                               [] { return hardware::createXRaySource("mock"); });
    auto detector = orFallback(hardware::createDetector(hw.detectorType), "detector",
                               [] { return hardware::createDetector("mock"); });
    auto camera   = orFallback(hardware::createCamera(hw.cameraType),     "camera",
                               [] { return hardware::createCamera("mock"); });
    auto valves   = orFallback(hardware::createValveDriver(hw.valveType), "valves",
                               [] { return hardware::createValveDriver("mock"); });
    auto plc      = orFallback(hardware::createPLC(hw.plcType),           "plc",
                               [] { return hardware::createPLC("mock"); });

    // Beifu ADS interlock — must enable BEFORE X-ray startup
    hardware::BeiduAdapter beifu;
    if (hw.beifuEnabled && !hw.beifuNetId.empty()) {
        const std::string routerIp = hw.plcEndpoint.substr(
            hw.plcEndpoint.find("://") != std::string::npos
            ? hw.plcEndpoint.find("://") + 3 : 0,
            hw.plcEndpoint.rfind(':'));
        if (beifu.open(hw.beifuNetId, routerIp)) {
            beifu.enable(true);
            CGS_LOG_INFO("Beifu interlock enabled (NetID {})", hw.beifuNetId);
        } else {
            CGS_LOG_WARN("Beifu adapter open failed — beam may not enable");
        }
    }

    // ACS belt controller
    hardware::ACSMotionClient acs;
    if (!hw.acsIp.empty() && hw.acsPort > 0) {
        if (acs.connect(hw.acsIp, hw.acsPort)) {
            acs.enableBelt(true);
            CGS_LOG_INFO("ACS belt enabled ({}:{})", hw.acsIp, hw.acsPort);
        } else {
            CGS_LOG_WARN("ACS connect failed — belt may be controlled by PLC only");
        }
    }

    if (xray) {
        const std::string xopen = (hw.xrayType == "mock" || hw.xrayType.empty())
                                  ? "" : serialUri(hw.xrayPort);
        xray->open(xopen);
        if (hw.xrayType != "mock" && !hw.xrayType.empty()) {
            xray->setKv(hw.xrayKv);
            xray->setMa(hw.xrayMa);
        }
        xray->startup();
    }

    hardware::DetectorConfig dcfg;
    dcfg.width      = hw.detectorWidth;
    dcfg.lineRateHz = hw.detectorLineRateHz;
    if (detector) detector->open(dcfg);

    hardware::CameraConfig ccfg;
    ccfg.ipOrSerial      = hw.cameraIp;
    ccfg.ccfPath         = hw.cameraCcfPath;
    ccfg.width           = hw.cameraWidth;
    ccfg.height          = hw.cameraHeight;
    ccfg.exposureUs      = hw.cameraExposureUs;
    ccfg.gainDb          = hw.cameraGainDb;
    ccfg.frameRateHz     = hw.cameraFrameRateHz;
    ccfg.hardwareTrigger = hw.cameraHardwareTrigger;
    if (camera) camera->open(ccfg);

    if (valves) { valves->open(valveOpenArg(hw)); valves->arm(); }
    if (plc)    { plc->connect(hw.plcEndpoint);
                  plc->setConveyor(hardware::ConveyorId::DetectionBelt, true);
                  plc->setBeltSpeedHz(hw.plcInitialBeltHz); }

    // 4. Pipeline engine + inference
    core::PipelineEngine engine;
    engine.setConfig(pcfg);
    engine.attach(detector.get(), camera.get(), valves.get(), plc.get(), xray.get());

    if (hw.xrayType != "mock") {
        auto inferEngine = core::createBestInferenceEngine(pcfg.inference);
        if (inferEngine) {
            std::printf("[inference] engine: %s\n", inferEngine->engineName().c_str());
            CGS_LOG_INFO("inference engine: {}", inferEngine->engineName());
            engine.setInferenceEngine(std::move(inferEngine));
        } else {
            std::printf("[inference] no engine — using threshold classifier fallback\n");
            CGS_LOG_WARN("no inference engine; threshold classifier active");
        }
    }

    engine.setSnapshotCallback([](const core::PipelineFrameSnapshot& snap) {
        if (!snap.firedNozzleIds.empty()) {
            std::string ids;
            for (int n : snap.firedNozzleIds) {
                if (!ids.empty()) ids += ",";
                ids += std::to_string(n);
            }
            CGS_LOG_DEBUG("-----finish process: frame={} gangue nozzles=[{}]",
                          snap.frameId, ids);
            std::printf("[frame %llu] gangue → nozzles: %s  (%.1f ms inference)\n",
                        (unsigned long long)snap.frameId,
                        ids.c_str(),
                        snap.inferenceMs);
        }
    });

    engine.arm();
    if (detector) detector->start();
    if (camera)   camera->startStreaming();

    // 5. Run loop
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    std::printf("Running for %d s (--forever to run indefinitely)...\n", durationSeconds);
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const auto s = engine.stats();
        std::printf("== frames=%llu  detected=%llu  rejected=%llu  fired=%llu  "
                    "belt=%.2f m/s  inference=%.1f ms ==\n",
                    (unsigned long long)s.framesProcessed,
                    (unsigned long long)s.segmentsDetected,
                    (unsigned long long)s.gangueRejected,
                    (unsigned long long)s.commandsScheduled,
                    s.currentBeltSpeedMps,
                    s.lastInferenceMs);
        if (!runForever && std::chrono::duration_cast<std::chrono::seconds>(
                clock::now() - t0).count() >= durationSeconds)
            break;
    }

    // 6. Teardown
    if (camera)  camera->stopStreaming();
    if (detector) detector->stop();
    engine.disarm();
    if (valves) { valves->disarm(); valves->close(); }
    if (plc)    plc->disconnect();
    if (xray)   { xray->shutdown(); xray->close(); }
    if (beifu.isOpen()) { beifu.enable(false); beifu.close(); }
    if (acs.isConnected()) { acs.enableBelt(false); acs.disconnect(); }

    CGS_LOG_INFO("-----coal-gangue-sorter console done");
    core::Logger::flush();
    std::printf("==> done.\n");
    return 0;
}
