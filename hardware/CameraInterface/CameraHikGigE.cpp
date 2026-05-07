#include "CameraHikGigE.h"

#ifdef CGS_HAS_HIKVISION

#include <chrono>
#include <cstring>

namespace cgs {
namespace hardware {

CameraHikGigE::CameraHikGigE() = default;

CameraHikGigE::~CameraHikGigE() {
    close();
}

// ---------------------------------------------------------------------------
// open — enumerate GigE devices, match by IP if provided, connect.
// ---------------------------------------------------------------------------
bool CameraHikGigE::open(const CameraConfig& cfg) {
    if (m_handle) return false;

    MV_CC_DEVICE_INFO_LIST deviceList{};
    if (MV_CC_EnumDevices(MV_GIGE_DEVICE, &deviceList) != MV_OK) return false;
    if (deviceList.nDeviceNum == 0) return false;

    MV_CC_DEVICE_INFO* target = nullptr;
    for (unsigned int i = 0; i < deviceList.nDeviceNum; ++i) {
        auto* info = deviceList.pDeviceInfo[i];
        if (info->nTLayerType != MV_GIGE_DEVICE) continue;
        if (cfg.ipOrSerial.empty()) {
            target = info;  // take first GigE camera if no IP specified
            break;
        }
        // Match by IP string
        char ipStr[32]{};
        const auto ip = info->SpecialInfo.stGigEInfo.nCurrentIp;
        std::snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
                      (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                      (ip >>  8) & 0xFF,  ip         & 0xFF);
        if (cfg.ipOrSerial == ipStr) { target = info; break; }
    }
    if (!target) return false;

    if (MV_CC_CreateHandle(&m_handle, target) != MV_OK) return false;
    if (MV_CC_OpenDevice(m_handle) != MV_OK) {
        MV_CC_DestroyHandle(m_handle); m_handle = nullptr; return false;
    }

    // Load feature file — prefer cfg.ccfPath, fall back to m_hikCfg.ccfPath
    const std::string& ccf = cfg.ccfPath.empty() ? m_hikCfg.ccfPath : cfg.ccfPath;
    if (!ccf.empty()) loadCcf(ccf);

    // Trigger mode: use cfg.hardwareTrigger (para0_0_4) as the canonical source
    const int trigMode = cfg.hardwareTrigger ? 1 : 0;
    MV_CC_SetEnumValue(m_handle, "TriggerMode", trigMode);

    // Frame rate (only meaningful in free-run mode)
    if (m_hikCfg.triggerMode == 0 && cfg.frameRateHz > 0) {
        MV_CC_SetBoolValue(m_handle, "AcquisitionFrameRateEnable", true);
        MV_CC_SetFloatValue(m_handle, "AcquisitionFrameRate",
                            static_cast<float>(cfg.frameRateHz));
    }

    // Register frame callback — delivers Mono8 data (8-bit greyscale line-scan)
    MV_CC_RegisterImageCallBackEx(m_handle, &CameraHikGigE::frameCallbackStatic, this);
    return true;
}

void CameraHikGigE::close() {
    stopStreaming();
    if (m_handle) {
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
    }
}

bool CameraHikGigE::startStreaming() {
    if (!m_handle) return false;
    if (m_streaming.exchange(true)) return false;
    return MV_CC_StartGrabbing(m_handle) == MV_OK;
}

bool CameraHikGigE::stopStreaming() {
    if (!m_streaming.exchange(false)) return false;
    if (m_handle) MV_CC_StopGrabbing(m_handle);
    return true;
}

bool CameraHikGigE::triggerSoft() {
    if (!m_handle || !m_streaming) return false;
    return MV_CC_SetCommandValue(m_handle, "TriggerSoftware") == MV_OK;
}

void CameraHikGigE::setFrameCallback(FrameCallback cb) {
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_cb = std::move(cb);
}

bool CameraHikGigE::loadCcf(const std::string& ccfPath) {
    if (!m_handle) return false;
    return MV_CC_FeatureLoad(m_handle, ccfPath.c_str()) == MV_OK;
}

// ---------------------------------------------------------------------------
// Frame callback — converts Hikvision frame to ICamera::CameraFrame (Mono8).
// The PipelineEngine's camera path uses 8-bit CameraFrame; the XCCR mapper
// converts camera pixel columns to detector pixel columns for fusion.
// ---------------------------------------------------------------------------
void MV_CAMCTRL_API CameraHikGigE::frameCallbackStatic(unsigned char* pData,
                                                         MV_FRAME_OUT_INFO_EX* pInfo,
                                                         void* pUser) {
    if (pUser) static_cast<CameraHikGigE*>(pUser)->onFrame(pData, pInfo);
}

void CameraHikGigE::onFrame(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pInfo) {
    if (!pData || !pInfo || !m_streaming) return;

    CameraFrame f;
    f.frameId    = ++m_frameId;
    f.timestampNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
    f.width      = static_cast<int>(pInfo->nWidth);
    f.height     = static_cast<int>(pInfo->nHeight);
    f.format     = PixelFormat::Mono8;

    const size_t total = static_cast<size_t>(f.width) * static_cast<size_t>(f.height);
    f.data.assign(pData, pData + total);

    FrameCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_cb;
    }
    if (cb) cb(f);
}

}}

#endif // CGS_HAS_HIKVISION
