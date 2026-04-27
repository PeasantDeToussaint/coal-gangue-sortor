#ifndef DETECTORMOCK_H
#define DETECTORMOCK_H

#include "IDetector.h"

#include <atomic>
#include <thread>

namespace cgs {
namespace hardware {

// Generates synthetic line-scan frames at the configured line rate.
// Each frame simulates a row with a random-width "object" of lower transmission
// (lower pixel value = thicker / denser material). Perfect for testing
// classification thresholds and timing without real X-ray hardware.
class DetectorMock : public IDetector {
public:
    DetectorMock();
    ~DetectorMock() override;

    bool open(const DetectorConfig& cfg) override;
    void close() override;

    bool start() override;
    bool stop() override;

    void setFrameCallback(FrameCallback cb) override;

    bool isRunning() const override { return m_running; }

private:
    void run();

    DetectorConfig m_cfg;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    FrameCallback m_cb;
    uint64_t m_frameId = 0;
};

}}

#endif
