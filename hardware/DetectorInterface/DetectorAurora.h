#ifndef CGS_DETECTORAURORA_H
#define CGS_DETECTORAURORA_H

// Detection Technology X-LIB / Aurora SDK adapter.
// Compiled only when CGS_HAS_AURORA_SDK is defined and the SDK headers are
// reachable via -I third_party/aurora-sdk/include and the linker has access
// to xlib.lib (Windows) / libxlib.so (Linux).
//
// Protocol details and example flow: docs/hardware/protocols/detection-tech-aurora.md
// Hardware: 17x X-Card DA21506414C + 1x X-GCU GT, GigE Ethernet (UDP).

#include "IDetector.h"

#ifdef CGS_HAS_AURORA_SDK

#include <atomic>
#include <mutex>
#include <string>

class XSystem;
class XCommand;
class XAcquisition;
class XFrameTransfer;
class XOffCorrect;
class XGigFactory;
class XImage;

namespace cgs {
namespace hardware {

struct AuroraConfig {
    std::string localIp = "192.168.1.100";   // PC NIC IP on detector subnet
    int lineCount = 512;                     // lines per frame
    int integrationTimeUs = 800;
    int gainLow = 4;
    int gainHigh = 4;
    bool enableLineTrigger = true;           // external trigger from PLC/photo-eye
    bool enableLineInfo = true;              // 8 byte trailer per line
    std::string offsetGainModelPath;         // pre-calibrated mode_up.txt
};

class DetectorAurora : public IDetector {
public:
    DetectorAurora();
    ~DetectorAurora() override;

    bool open(const DetectorConfig& cfg) override;
    void close() override;
    bool start() override;
    bool stop() override;
    void setFrameCallback(FrameCallback cb) override;
    bool isRunning() const override;

    void setAuroraConfig(const AuroraConfig& a) { m_auroraCfg = a; }

    // Called from internal SDK callback (IXImgSink::OnFrameReady)
    void onFrameReadyInternal(XImage* image);

private:
    bool initSdkObjects();
    void releaseSdkObjects();

    XSystem*        m_sys = nullptr;
    XCommand*       m_cmd = nullptr;
    XAcquisition*   m_acq = nullptr;
    XFrameTransfer* m_xfer = nullptr;
    XOffCorrect*    m_off = nullptr;
    XGigFactory*    m_factory = nullptr;

    DetectorConfig m_cfg;
    AuroraConfig   m_auroraCfg;
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_frameId{0};
    FrameCallback m_cb;
    std::mutex m_mtx;
};

}}

#endif // CGS_HAS_AURORA_SDK
#endif
