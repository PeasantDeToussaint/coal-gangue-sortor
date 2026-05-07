#ifndef CGS_CAMERAHIKGIGE_H
#define CGS_CAMERAHIKGIGE_H

// Hikvision GigE Vision / GenICam line-scan camera adapter.
//
// Enabled with: cmake -DCGS_HAS_HIKVISION=ON -DCGS_HIKVISION_DIR=<SDK>
//
// Wraps the Hikvision MvCameraControl SDK. Implements ICamera so it can be
// substituted for the mock camera in any pipeline configuration.
//
// To use the camera in X-ray+camera fusion mode (para0_0_2=2):
//   - Set cameraType="hik-gige" in config.xml
//   - Provide ccfPath in CameraHikConfig
//   - PipelineEngine reads CameraFrame (Mono8) and converts for the XCCR mapper

#include "ICamera.h"

#ifdef CGS_HAS_HIKVISION

#include <MvCameraControl.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace cgs {
namespace hardware {

// Extra Hikvision-specific options not in the generic CameraConfig.
struct CameraHikConfig {
    std::string ccfPath;          // .ccf device feature file from MVS software
    int         triggerMode{0};   // 0=internal (free-run), 1=external hardware trigger
};

class CameraHikGigE : public ICamera {
public:
    CameraHikGigE();
    ~CameraHikGigE() override;

    // ICamera interface
    bool open(const CameraConfig& cfg) override;
    void close() override;

    bool startStreaming() override;
    bool stopStreaming() override;
    bool triggerSoft() override;
    bool isStreaming() const override { return m_streaming; }

    void setFrameCallback(FrameCallback cb) override;

    // Load a Hikvision .ccf device feature file (call before startStreaming).
    bool loadCcf(const std::string& ccfPath);

    // Set extra Hikvision-specific config (call before open()).
    void setHikConfig(const CameraHikConfig& cfg) { m_hikCfg = cfg; }

private:
    static void MV_CAMCTRL_API frameCallbackStatic(unsigned char* pData,
                                                    MV_FRAME_OUT_INFO_EX* pFrameInfo,
                                                    void* pUser);
    void onFrame(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo);

    void*           m_handle{nullptr};
    CameraHikConfig m_hikCfg;

    std::atomic<bool>     m_streaming{false};
    std::atomic<uint64_t> m_frameId{0};
    FrameCallback          m_cb;
    std::mutex             m_cbMtx;
};

}}

#endif // CGS_HAS_HIKVISION
#endif
