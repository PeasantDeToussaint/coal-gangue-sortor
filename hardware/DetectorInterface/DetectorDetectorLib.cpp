#include "DetectorDetectorLib.h"

#ifdef CGS_HAS_DETECTORLIB

#include "../../third_party/detectorlib/DetectorLibInterface.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace cgs {
namespace hardware {

DetectorDetectorLib::DetectorDetectorLib() = default;

DetectorDetectorLib::~DetectorDetectorLib() {
    close();
}

bool DetectorDetectorLib::open(const DetectorConfig& cfg) {
    m_width         = cfg.width > 0 ? cfg.width : 2180;
    m_integrationUs = cfg.integrationTimeUs > 0 ? cfg.integrationTimeUs : 540;
    m_dataPath      = "D:/data/";   // matches original: "D:/data/%1_IO_Mat_last.png"

    // Step 1: factory singleton → create DetectorLibInterface object.
    DetectorLibFactory* factory = DetectorLibFactory::getInstance();
    if (!factory) {
        std::fprintf(stderr, "[DetectorDetectorLib] factory getInstance() failed\n");
        return false;
    }
    // Confirmed from ThreadManager.dll.c DataAcquirer constructor:
    //   DetectorLibFactory::create("DetectorLib")  ← exact string from decompilation
    m_det = factory->create("DetectorLib");
    if (!m_det) m_det = factory->create("");        // fallback
    if (!m_det) {
        std::fprintf(stderr, "[DetectorDetectorLib] factory->create('DetectorLib') failed\n");
        return false;
    }

    // Step 2: connect frame signal BEFORE calling any slots.
    QObject::connect(m_det, &DetectorLibInterface::signalAcqDataArray,
                     this,  &DetectorDetectorLib::onAcqDataArray,
                     Qt::QueuedConnection);

    // Step 3: slotDet_init(localIp, ?, bool*)
    //
    // Confirmed from DetectorLib.dll.c decompilation (FUN_180003530):
    //   - param_2 (QString) → XSystem::SetLocalIP(ip)
    //     ip = LOCAL PC network interface IP on the detector subnet.
    //     The detector's IP/ports are auto-discovered by XSystem::FindDevice()
    //     via GigE Vision broadcast — NOT passed as parameters.
    //   - param_3 (int) → stored at this+0x2c; exact meaning TBD (try 0, lineCount, or intTime)
    //   - param_4 (bool*) → 1=success, 0=failure (device not found on subnet)
    bool initOk = false;
    const QString localIp = QString::fromStdString(cfg.ipOrSerial);
    QMetaObject::invokeMethod(m_det, "slotDet_init",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(QString, localIp),
                              Q_ARG(int, 0),       // try 0 first; if init fails try cfg.lineRateHz
                              Q_ARG(bool*, &initOk));
    if (!initOk) {
        std::fprintf(stderr, "[DetectorDetectorLib] slotDet_init failed "
                              "(ip=%s). Check PC NIC is on 192.168.1.x subnet.\n",
                     localIp.toStdString().c_str());
    }

    // Step 4: slotDet_connect — opens the GigE connection to the discovered device.
    bool connectOk = false;
    QMetaObject::invokeMethod(m_det, "slotDet_connect",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(bool*, &connectOk));
    if (!connectOk) {
        std::fprintf(stderr, "[DetectorDetectorLib] slotDet_connect failed.\n");
        // Don't abort — may still work (some versions return false but stream anyway).
    }

    // Step 5: slotDet_setParams(intTime, bool*)
    // Confirmed from FUN_180003920: XCommand::SetPara(cmd, 3, intTime, 0)
    // Parameter ID 3 = integration time. Match config intTime="540".
    bool paramsOk = false;
    QMetaObject::invokeMethod(m_det, "slotDet_setParams",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(int, m_integrationUs),
                              Q_ARG(bool*, &paramsOk));
    if (!paramsOk)
        std::fprintf(stderr, "[DetectorDetectorLib] slotDet_setParams(%d) failed.\n", m_integrationUs);

    // Step 6: slotDet_setExternal(false, bool*) — internal trigger (free-running).
    // Confirmed from FUN_1800038c0: external=false → SetPara(0x1e, 0) = internal trigger.
    bool trigOk = false;
    QMetaObject::invokeMethod(m_det, "slotDet_setExternal",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(bool, false),   // false = internal trigger
                              Q_ARG(bool*, &trigOk));
    if (!trigOk)
        std::fprintf(stderr, "[DetectorDetectorLib] slotDet_setExternal failed.\n");

    // Step 7: load calibration model (dark/bright field correction).
    // Confirmed from FUN_180003770: checks for "mode.txt" in the config path,
    // loads the model if present.
    bool modelOk = false;
    QMetaObject::invokeMethod(m_det, "slotDet_loadModel",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(bool*, &modelOk));
    if (!modelOk)
        std::fprintf(stderr, "[DetectorDetectorLib] slotDet_loadModel: no model file found "
                              "(calibration needed — run 暗场校正 / 亮场校正 first).\n");

    return true;   // connected even if some steps failed
}

void DetectorDetectorLib::close() {
    stop();
    if (m_det) {
        // Stop grab before disconnecting
        bool ok = false;
        QMetaObject::invokeMethod(m_det, "slotDet_stopGrab",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(bool*, &ok));
        bool disconnOk = false;
        QMetaObject::invokeMethod(m_det, "slotDet_disconnect",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(bool*, &disconnOk));
        QObject::disconnect(m_det, nullptr, this, nullptr);
        m_det = nullptr;
    }
}

bool DetectorDetectorLib::start() {
    if (!m_det) return false;

    // slotDet_acquireData(int count, QString type, QString path, bool*)
    // Confirmed from FUN_1800030e0:
    //   type="GrabImg" → continuous acquisition (count = lineNumber per frame)
    //   type="SnapImg" → single frame capture (count = 1)
    //   type="dark"    → dark field calibration
    //   type="bright"  → bright field calibration (target count = 50000)
    bool ok = false;
    QMetaObject::invokeMethod(m_det, "slotDet_acquireData",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(int,     m_width),                        // frame pixel count
                              Q_ARG(QString, QString("GrabImg")),             // continuous mode
                              Q_ARG(QString, QString::fromStdString(m_dataPath)),
                              Q_ARG(bool*,   &ok));
    if (!ok)
        std::fprintf(stderr, "[DetectorDetectorLib] slotDet_acquireData failed.\n");

    m_running = true;
    return true;
}

bool DetectorDetectorLib::stop() {
    m_running = false;
    if (!m_det) return false;
    bool ok = false;
    QMetaObject::invokeMethod(m_det, "slotDet_stopGrab",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(bool*, &ok));
    return ok;
}

void DetectorDetectorLib::setFrameCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_cb = std::move(cb);
}

// ---------------------------------------------------------------------------
// Slot — DetectorLib emits signalAcqDataArray for every acquired scan line.
// Confirmed from FUN_1800039c0:
//   signalAcqDataArray(uint16* pixelData, int width, int height, QString timestamp)
// ---------------------------------------------------------------------------
void DetectorDetectorLib::onAcqDataArray(unsigned short* data,
                                          int width, int height,
                                          QString /*timestamp*/) {
    if (!m_running || !data || width <= 0 || height <= 0) return;

    FrameCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_cb;
    }
    if (!cb) return;

    DetectorFrame frame;
    frame.frameId     = ++m_frameId;
    frame.timestampNs = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
    frame.width        = width;
    frame.height       = height;
    frame.bitsPerPixel = 16;
    frame.data.assign(data, data + (size_t)width * (size_t)height);
    cb(frame);
}

// ---------------------------------------------------------------------------
// Calibration — confirmed from FUN_1800030e0:
//   slotDet_acquireData(N, prefix, "dark",   result) → dark field calibration
//   slotDet_acquireData(N, prefix, "bright", result) → bright field (target 50000 cts)
// ---------------------------------------------------------------------------
bool DetectorDetectorLib::calibrateDark() {
    if (!m_det) return false;
    bool ok = false;
    QMetaObject::invokeMethod(m_det, "slotDet_acquireData",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(int,     100),                            // N frames to average
                              Q_ARG(QString, QString("dark_calib")),          // prefix for saved files
                              Q_ARG(QString, QString("dark")),                // type string → dark calibration
                              Q_ARG(bool*,   &ok));
    return ok;
}

bool DetectorDetectorLib::calibrateBright() {
    if (!m_det) return false;
    bool ok = false;
    QMetaObject::invokeMethod(m_det, "slotDet_acquireData",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(int,     50000),                          // target count (from decompilation)
                              Q_ARG(QString, QString("bright_calib")),        // prefix
                              Q_ARG(QString, QString("bright")),              // type string → bright calibration
                              Q_ARG(bool*,   &ok));
    return ok;
}

}} // cgs::hardware

#endif // CGS_HAS_DETECTORLIB
