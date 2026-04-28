// Qt GUI entry point. Wires hardware -> PipelineEngine -> MainWindow.
// Connection strings and algorithm parameters come from config.xml when present.

#include "MainWindow.h"

#include "../../core/Config/ConfigLoader.h"
#include "../../core/Pipeline/PipelineEngine.h"
#include "../../hardware/CameraInterface/ICamera.h"
#include "../../hardware/DetectorInterface/IDetector.h"
#include "../../hardware/PLCInterface/IPLC.h"
#include "../../hardware/ValveDriverInterface/IValveDriver.h"
#include "../../hardware/XRayInterface/IXRaySource.h"

#include <QApplication>
#include <QCommandLineParser>

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

    QCommandLineParser parser;
    parser.setApplicationDescription("Coal-Gangue Sorter Qt GUI");
    parser.addHelpOption();
    QCommandLineOption mockOpt("mock", "Use mock hardware when no config file loads");
    QCommandLineOption realOpt("real", "Use built-in real-hardware preset when no config file loads");
    QCommandLineOption configOpt(QStringList() << "c" << "config",
                                 "Path to config.xml (see also CGS_CONFIG_PATH)",
                                 "file");
    parser.addOption(mockOpt);
    parser.addOption(realOpt);
    parser.addOption(configOpt);
    parser.process(app);

    std::string cfgPath = parser.value(configOpt).toStdString();
    if (cfgPath.empty()) cfgPath = cgs::core::resolveConfigPath(argc, argv);

    cgs::core::PipelineConfig pcfg;
    cgs::core::HardwareConfig hw;
    const bool loaded = cgs::core::loadConfigFromFile(cfgPath, pcfg, hw);
    if (!loaded) {
        const bool wantReal = parser.isSet(realOpt);
        cgs::core::applyBuiltinDefaults(wantReal, pcfg, hw);
    }

    auto xray = orFallback(cgs::hardware::createXRaySource(hw.xrayType), "xray",
                           [] { return cgs::hardware::createXRaySource("mock"); });
    auto detector = orFallback(cgs::hardware::createDetector(hw.detectorType), "detector",
                               [] { return cgs::hardware::createDetector("mock"); });
    auto camera = orFallback(cgs::hardware::createCamera(hw.cameraType), "camera",
                             [] { return cgs::hardware::createCamera("mock"); });
    auto valves = orFallback(cgs::hardware::createValveDriver(hw.valveType), "valves",
                             [] { return cgs::hardware::createValveDriver("mock"); });
    auto plc = orFallback(cgs::hardware::createPLC(hw.plcType), "plc",
                          [] { return cgs::hardware::createPLC("mock"); });

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

    cgs::hardware::DetectorConfig dcfg;
    dcfg.width = hw.detectorWidth;
    dcfg.lineRateHz = hw.detectorLineRateHz;
    if (detector) detector->open(dcfg);

    cgs::hardware::CameraConfig ccfg;
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
        plc->setConveyor(cgs::hardware::ConveyorId::DetectionBelt, true);
        plc->setBeltSpeedHz(hw.plcInitialBeltHz);
    }

    cgs::core::PipelineEngine engine;
    engine.setConfig(pcfg);
    engine.attach(detector.get(), camera.get(), valves.get(), plc.get(), xray.get());

    if (detector) detector->start();
    if (camera) camera->startStreaming();

    cgs::gui::MainWindow win(&engine);
    win.show();

    const int rc = app.exec();

    if (camera) camera->stopStreaming();
    if (detector) detector->stop();
    if (valves) {
        valves->disarm();
        valves->close();
    }
    if (plc) plc->disconnect();
    if (xray) {
        xray->shutdown();
        xray->close();
    }
    return rc;
}
