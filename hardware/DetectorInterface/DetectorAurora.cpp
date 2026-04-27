#include "DetectorAurora.h"

#ifdef CGS_HAS_AURORA_SDK

// Aurora X-LIB SDK headers — copy these from the legacy project's
// include/DetInclude/ folder into third_party/aurora-sdk/include/.
#include "xsystem.h"
#include "xcommand.h"
#include "xacquisition.h"
#include "xframe_transfer.h"
#include "xoff_correct.h"
#include "xgig_factory.h"
#include "xdevice.h"
#include "ximage.h"
#include "ixcmd_sink.h"
#include "iximg_sink.h"

#include <chrono>

namespace cgs {
namespace hardware {

namespace {
DetectorAurora* g_currentInstance = nullptr;

class CmdSinkBridge : public IXCmdSink {
public:
    void OnXError(uint32_t /*err_id*/, const char* /*err_msg*/) override {}
    void OnXEvent(uint32_t /*event_id*/, float /*data*/) override {}
};

class ImgSinkBridge : public IXImgSink {
public:
    void OnXError(uint32_t /*err_id*/, const char* /*err_msg*/) override {}
    void OnXEvent(uint32_t /*event_id*/, uint32_t /*data*/) override {}
    void OnFrameReady(XImage* image) override {
        if (g_currentInstance) g_currentInstance->onFrameReadyInternal(image);
    }
};

CmdSinkBridge g_cmdSink;
ImgSinkBridge g_imgSink;
}

DetectorAurora::DetectorAurora() {
    g_currentInstance = this;
}

DetectorAurora::~DetectorAurora() {
    close();
    if (g_currentInstance == this) g_currentInstance = nullptr;
}

bool DetectorAurora::initSdkObjects() {
    if (m_sys) return true;
    m_sys     = new XSystem();
    m_cmd     = new XCommand();
    m_acq     = new XAcquisition();
    m_xfer    = new XFrameTransfer();
    m_off     = new XOffCorrect();
    m_factory = new XGigFactory();
    m_sys->RegisterEventSink(&g_cmdSink);
    m_cmd->RegisterEventSink(&g_cmdSink);
    m_acq->RegisterEventSink(&g_imgSink);
    m_xfer->RegisterEventSink(&g_imgSink);
    return true;
}

void DetectorAurora::releaseSdkObjects() {
    if (m_acq) { m_acq->Stop(); }
    if (m_off) { delete m_off; m_off = nullptr; }
    if (m_xfer){ delete m_xfer; m_xfer = nullptr; }
    if (m_acq) { delete m_acq;  m_acq = nullptr; }
    if (m_cmd) { delete m_cmd;  m_cmd = nullptr; }
    if (m_factory) { delete m_factory; m_factory = nullptr; }
    if (m_sys) { m_sys->Close(); delete m_sys; m_sys = nullptr; }
}

bool DetectorAurora::open(const DetectorConfig& cfg) {
    if (m_running) return false;
    m_cfg = cfg;
    if (!initSdkObjects()) return false;

    m_sys->Close();
    m_sys->SetLocalIP(m_auroraCfg.localIp.c_str());
    if (!m_sys->Open()) return false;
    if (m_sys->FindDevice() <= 0) return false;
    XDevice* dev = m_sys->GetDevice(0);
    if (!dev) return false;

    m_cmd->SetFactory(m_factory);
    m_acq->SetFactory(m_factory);
    m_xfer->SetLineNum(m_auroraCfg.lineCount);
    m_acq->RegisterFrameTransfer(m_xfer);
    m_acq->EnableLineInfo(m_auroraCfg.enableLineInfo ? 1 : 0);
    if (!m_cmd->Open(dev)) return false;

    m_cmd->SetPara(XPARA_INT_TIME, (uint64_t)m_auroraCfg.integrationTimeUs);
    m_cmd->SetPara(XPARA_EN_SCAN, 1);
    if (m_auroraCfg.enableLineTrigger) {
        m_cmd->SetPara(XPARA_EN_LINE_TRIGGER, 1);
    }

    if (!m_acq->Open(dev, m_cmd)) return false;
    m_off->Open(dev);
    if (!m_auroraCfg.offsetGainModelPath.empty()) {
        m_off->LoadFile(m_auroraCfg.offsetGainModelPath.c_str());
    }
    return true;
}

void DetectorAurora::close() {
    stop();
    releaseSdkObjects();
}

bool DetectorAurora::start() {
    if (!m_acq) return false;
    if (m_running.exchange(true)) return false;
    m_acq->Grab(0); // 0 = continuous
    return true;
}

bool DetectorAurora::stop() {
    if (!m_running.exchange(false)) return false;
    if (m_acq) m_acq->Stop();
    return true;
}

void DetectorAurora::setFrameCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_cb = std::move(cb);
}

bool DetectorAurora::isRunning() const { return m_running; }

void DetectorAurora::onFrameReadyInternal(XImage* image) {
    if (!image || !m_running) return;
    if (m_off) m_off->DoCorrect(image);

    DetectorFrame f;
    f.frameId = ++m_frameId;
    f.timestampNs = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
    f.width = image->_width;
    f.height = image->_height;
    f.bitsPerPixel = 16;
    const size_t total = (size_t)image->_width * (size_t)image->_height;
    f.data.assign((const uint16_t*)image->_data_, (const uint16_t*)image->_data_ + total);

    FrameCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        cb = m_cb;
    }
    if (cb) cb(f);
}

}}

#endif // CGS_HAS_AURORA_SDK
