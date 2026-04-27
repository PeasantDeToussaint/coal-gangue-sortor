// Qt GUI entry point. Wires Mock hardware -> PipelineEngine -> MainWindow.
// Real hardware swap-in happens by replacing the createXxx("mock") factory calls.

#include "MainWindow.h"

#include "../../core/Pipeline/PipelineEngine.h"
#include "../../hardware/CameraInterface/CameraMock.h"
#include "../../hardware/DetectorInterface/DetectorMock.h"
#include "../../hardware/PLCInterface/PLCMock.h"
#include "../../hardware/ValveDriverInterface/ValveDriverMock.h"
#include "../../hardware/XRayInterface/XRayMock.h"

#include <QApplication>
#include <QCommandLineParser>

#include <memory>

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Coal-Gangue Sorter Qt GUI");
    parser.addHelpOption();
    QCommandLineOption mockOpt("mock", "Use mock hardware (default)");
    QCommandLineOption realOpt("real", "Use real hardware adapters where available");
    parser.addOption(mockOpt);
    parser.addOption(realOpt);
    parser.process(app);
    const bool useMock = !parser.isSet(realOpt);

    auto xray     = cgs::hardware::createXRaySource(useMock ? "mock" : "vj-serial");
    auto detector = cgs::hardware::createDetector  (useMock ? "mock" : "aurora");
    auto camera   = cgs::hardware::createCamera    ("mock");
    auto valves   = cgs::hardware::createValveDriver(useMock ? "mock" : "el2828");
    auto plc      = cgs::hardware::createPLC       (useMock ? "mock" : "beckhoff");

    if (xray)     xray->open("");
    if (xray)     xray->startup();
    cgs::hardware::DetectorConfig dcfg;
    dcfg.width = 2180;
    dcfg.lineRateHz = 100;
    if (detector) detector->open(dcfg);
    cgs::hardware::CameraConfig ccfg;
    if (camera) camera->open(ccfg);
    if (valves)   valves->open("mock");
    if (valves)   valves->arm();
    if (plc)      plc->connect("mock://localhost");
    if (plc)      plc->setConveyor(cgs::hardware::ConveyorId::DetectionBelt, true);
    if (plc)      plc->setBeltSpeedHz(40.0);

    cgs::core::PipelineEngine engine;
    cgs::core::PipelineConfig pcfg;
    pcfg.nozzles.totalNozzles = 64;
    pcfg.nozzles.firstNozzleId = 1;
    pcfg.nozzles.pixelsPerNozzle = 2180.0 / 64.0;
    engine.setConfig(pcfg);
    engine.attach(detector.get(), camera.get(), valves.get(), plc.get(), xray.get());

    if (detector) detector->start();
    if (camera)   camera->startStreaming();

    cgs::gui::MainWindow win(&engine);
    win.show();

    int rc = app.exec();

    if (camera)   camera->stopStreaming();
    if (detector) detector->stop();
    if (valves)   valves->disarm();
    if (valves)   valves->close();
    if (plc)      plc->disconnect();
    if (xray)     xray->shutdown();
    if (xray)     xray->close();
    return rc;
}
