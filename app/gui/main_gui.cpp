// Qt GUI entry point. Wires hardware → PipelineEngine → MainWindow.
// Connection strings and algorithm parameters come from config.xml when present.
//
// Startup sequence (matches original Gangue.exe order):
//   1. Init logger
//   2. Load config
//   3. Construct hardware objects
//   4. Show PreheatDialog (X-ray source preheat)
//   5. Create inference engine (TRT preferred, ONNX fallback)
//   6. Arm PipelineEngine
//   7. Show MainWindow

#include "MainWindow.h"
#include "PreheatDialog.h"

#include "../../core/Config/ConfigLoader.h"
#include "../../core/Inference/IInferenceEngine.h"
#include "../../core/Logging/Logger.h"
#include "../../core/Pipeline/PipelineEngine.h"
#include "../../hardware/CameraInterface/ICamera.h"
#include "../../hardware/DetectorInterface/IDetector.h"
#include "../../hardware/PLCInterface/IPLC.h"
#include "../../hardware/ValveDriverInterface/IValveDriver.h"
#include "../../hardware/XRayInterface/IXRaySource.h"
#include "../../hardware/XRayInterface/BeiduAdapter.h"
#include "../../hardware/PLCInterface/ACSMotionClient.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QMessageBox>

#include <functional>
#include <memory>
#include <string>

namespace {

std::string serialUri(const std::string& port) {
    if (port.empty()) return {};
    if (port.find("://") != std::string::npos) return port;
    return "serial://" + port;
}

std::string valveOpenArg(const cgs::core::HardwareConfig& hw) {
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
    QApplication app(argc, argv);
    app.setApplicationName("coal-gangue-sorter");
    app.setOrganizationName("cgs");

    // Load original Gangue.exe stylesheet (style/style.qss next to executable).
    // Matches font (微软雅黑), colors, and layout of the original UI.
    {
        const QString qssPath = QCoreApplication::applicationDirPath() + "/style/style.qss";
        QFile qss(qssPath);
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
            app.setStyleSheet(qss.readAll());
            qss.close();
        }
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("Coal-Gangue Sorter Qt GUI (煤矸石分选机上位机)");
    parser.addHelpOption();
    QCommandLineOption mockOpt("mock", "Use mock hardware when no config file loads");
    QCommandLineOption realOpt("real", "Use real-hardware preset when no config file loads");
    QCommandLineOption configOpt(QStringList() << "c" << "config",
                                 "Path to config.xml (see also CGS_CONFIG_PATH)", "file");
    parser.addOption(mockOpt);
    parser.addOption(realOpt);
    parser.addOption(configOpt);
    parser.process(app);

    std::string cfgPath = parser.value(configOpt).toStdString();
    if (cfgPath.empty()) cfgPath = cgs::core::resolveConfigPath(argc, argv);

    // 1. Init logger — creates data/gangue_sys.log (rotating, matches original)
    cgs::core::Logger::init("data/gangue_sys.log");
    CGS_LOG_INFO("-----coal-gangue-sorter GUI starting");

    // 2. Load config
    cgs::core::PipelineConfig pcfg;
    cgs::core::HardwareConfig hw;
    const bool loaded = cgs::core::loadConfigFromFile(cfgPath, pcfg, hw);
    if (!loaded) {
        const bool wantReal = parser.isSet(realOpt);
        cgs::core::applyBuiltinDefaults(wantReal, pcfg, hw);
        CGS_LOG_WARN("config not found at {} — using {} defaults",
                     cfgPath, wantReal ? "real-hardware" : "mock");
    } else {
        CGS_LOG_INFO("config loaded: {}", cfgPath);
    }

    // 3. Construct hardware
    auto xray     = orFallback(cgs::hardware::createXRaySource(hw.xrayType),   "xray",
                               [] { return cgs::hardware::createXRaySource("mock"); });
    auto detector = orFallback(cgs::hardware::createDetector(hw.detectorType), "detector",
                               [] { return cgs::hardware::createDetector("mock"); });
    auto camera   = orFallback(cgs::hardware::createCamera(hw.cameraType),     "camera",
                               [] { return cgs::hardware::createCamera("mock"); });
    auto valves   = orFallback(cgs::hardware::createValveDriver(hw.valveType), "valves",
                               [] { return cgs::hardware::createValveDriver("mock"); });
    auto plc      = orFallback(cgs::hardware::createPLC(hw.plcType),           "plc",
                               [] { return cgs::hardware::createPLC("mock"); });

    if (xray) {
        const std::string xopen = (hw.xrayType == "mock" || hw.xrayType.empty())
                                  ? "" : serialUri(hw.xrayPort);
        xray->open(xopen);
    }

    cgs::hardware::DetectorConfig dcfg;
    dcfg.width      = hw.detectorWidth;
    dcfg.lineRateHz = hw.detectorLineRateHz;
    if (detector) detector->open(dcfg);

    cgs::hardware::CameraConfig ccfg;
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
    if (plc) {
        plc->connect(hw.plcEndpoint);
        plc->setConveyor(cgs::hardware::ConveyorId::DetectionBelt, true);
        plc->setBeltSpeedHz(hw.plcInitialBeltHz);
    }

    // Beifu ADS interlock (enable before X-ray preheat)
    cgs::hardware::BeiduAdapter beifu;
    if (hw.beifuEnabled && !hw.beifuNetId.empty()) {
        // Router IP = PLC/Beckhoff host (parse from valve endpoint or use default)
        std::string routerIp = "192.168.0.1";
        const auto ep = hw.valveEndpoint;
        const auto ads = ep.find("ads://");
        if (ads != std::string::npos) {
            const auto after = ep.substr(ads + 6);
            const auto colon = after.find(':');
            if (colon != std::string::npos) routerIp = after.substr(0, colon);
        }
        if (beifu.open(hw.beifuNetId, routerIp)) {
            beifu.enable(true);
            CGS_LOG_INFO("Beifu interlock enabled (NetID {})", hw.beifuNetId);
        } else {
            CGS_LOG_WARN("Beifu adapter open failed — beam may not enable");
        }
    }

    // ACS belt controller
    cgs::hardware::ACSMotionClient acs;
    if (!hw.acsIp.empty() && hw.acsPort > 0 &&
        hw.xrayType != "mock" && !hw.xrayType.empty()) {
        if (acs.connect(hw.acsIp, hw.acsPort)) {
            acs.enableBelt(true);
            CGS_LOG_INFO("ACS belt enabled ({}:{})", hw.acsIp, hw.acsPort);
        } else {
            CGS_LOG_WARN("ACS connect failed — belt may be controlled by PLC only");
        }
    }

    // 4. Build and arm PipelineEngine
    cgs::core::PipelineEngine engine;
    engine.setConfig(pcfg);
    engine.attach(detector.get(), camera.get(), valves.get(), plc.get(), xray.get());

    // 5. Load inference engine (TRT preferred, ONNX fallback; skip for mock mode)
    if (hw.xrayType != "mock") {
        auto inferEngine = cgs::core::createBestInferenceEngine(pcfg.inference);
        if (inferEngine) {
            CGS_LOG_INFO("inference engine: {}", inferEngine->engineName());
            engine.setInferenceEngine(std::move(inferEngine));
        } else {
            CGS_LOG_WARN("no inference engine loaded — using threshold classifier fallback. "
                         "Set trtPath/onnxPath in config.xml and build with "
                         "-DCGS_HAS_TENSORRT=ON or -DCGS_HAS_ONNX=ON.");
        }
    }

    // Register logger → LogCenter bridge (set after MainWindow is created below)
    // The bridge is wired in MainWindow after construction.

    if (detector) detector->start();
    if (camera)   camera->startStreaming();

    // 6. Preheat dialog (blocks until preheat complete or skipped)
    if (hw.xrayType != "mock" && !hw.xrayType.empty()) {
        cgs::gui::PreheatDialog preheat(xray.get(), hw.lastCloseTime, nullptr);
        if (preheat.exec() != QDialog::Accepted || !preheat.preheatCompleted()) {
            CGS_LOG_WARN("preheat skipped by operator");
        } else {
            CGS_LOG_INFO("preheat complete");
        }
        // Real hardware: push configured kV / mA after preheat flow (matches commissioning:
        // set parameters on the source before / as beam comes to operating point).
        if (xray) {
            xray->setKv(hw.xrayKv);
            xray->setMa(hw.xrayMa);
        }
    } else {
        // Mock mode: just start the X-ray source directly.
        if (xray) {
            xray->setKv(hw.xrayKv);
            xray->setMa(hw.xrayMa);
            xray->startup();
        }
    }

    // 7. Show main window
    cgs::gui::MainWindow win(&engine, cfgPath, nullptr);
    win.setHardwareConfig(hw);
    win.setHardwarePointers(xray.get(), detector.get());

    // Wire logger → LogCenter (thread-safe: Logger callback emits Qt signal)
    cgs::core::Logger::addCallbackSink([&win](const std::string& msg) {
        win.findChild<cgs::gui::LogCenter*>(); // ensure LogCenter is accessible
        // Direct call: LogCenter::appendMessage() is internally thread-safe.
        // We need to access logCenter from win — MainWindow exposes it via the
        // QObject child list. Use QMetaObject for cross-thread safety.
        Q_UNUSED(msg); // Wire-up deferred to avoid circular include
    });

    win.show();

    const int rc = app.exec();

    CGS_LOG_INFO("-----coal-gangue-sorter GUI shutting down");
    cgs::core::Logger::flush();

    if (camera)  camera->stopStreaming();
    if (detector) detector->stop();
    if (valves)  { valves->disarm(); valves->close(); }
    if (plc)     plc->disconnect();
    if (xray)    { xray->shutdown(); xray->close(); }
    if (beifu.isOpen()) { beifu.enable(false); beifu.close(); }
    if (acs.isConnected()) { acs.enableBelt(false); acs.disconnect(); }
    return rc;
}
