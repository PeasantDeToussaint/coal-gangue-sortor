#ifndef CAMERAMOCK_H
#define CAMERAMOCK_H

#include "ICamera.h"

#include <atomic>
#include <thread>

namespace cgs {
namespace hardware {

class CameraMock : public ICamera {
public:
    CameraMock();
    ~CameraMock() override;

    bool open(const CameraConfig& cfg) override;
    void close() override;

    bool startStreaming() override;
    bool stopStreaming() override;
    bool triggerSoft() override;

    void setFrameCallback(FrameCallback cb) override;
    bool isStreaming() const override { return m_streaming; }

private:
    void run();
    void emitFrame();

    CameraConfig m_cfg;
    std::atomic<bool> m_streaming{false};
    std::thread m_thread;
    FrameCallback m_cb;
    uint64_t m_frameId = 0;
};

}}

#endif
