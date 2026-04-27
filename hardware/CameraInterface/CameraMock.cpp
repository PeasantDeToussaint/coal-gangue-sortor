#include "CameraMock.h"

#include <chrono>
#include <random>

namespace cgs {
namespace hardware {

CameraMock::CameraMock() = default;
CameraMock::~CameraMock() { close(); }

bool CameraMock::open(const CameraConfig& cfg) {
    if (m_streaming) return false;
    m_cfg = cfg;
    if (m_cfg.width <= 0) m_cfg.width = 1024;
    if (m_cfg.height <= 0) m_cfg.height = 512;
    if (m_cfg.frameRateHz <= 0) m_cfg.frameRateHz = 30;
    return true;
}

void CameraMock::close() {
    stopStreaming();
}

bool CameraMock::startStreaming() {
    if (m_cfg.hardwareTrigger) {
        m_streaming = true;
        return true;
    }
    if (m_streaming.exchange(true)) return false;
    m_thread = std::thread([this]{ run(); });
    return true;
}

bool CameraMock::stopStreaming() {
    if (!m_streaming.exchange(false)) return false;
    if (m_thread.joinable()) m_thread.join();
    return true;
}

bool CameraMock::triggerSoft() {
    if (!m_streaming) return false;
    if (m_cfg.hardwareTrigger) {
        emitFrame();
    }
    return true;
}

void CameraMock::setFrameCallback(FrameCallback cb) { m_cb = std::move(cb); }

void CameraMock::run() {
    using clock = std::chrono::steady_clock;
    const auto period = std::chrono::nanoseconds(1'000'000'000LL / m_cfg.frameRateHz);
    auto next = clock::now();

    while (m_streaming) {
        emitFrame();
        next += period;
        std::this_thread::sleep_until(next);
    }
}

void CameraMock::emitFrame() {
    static thread_local std::mt19937 rng(123);
    static thread_local std::uniform_int_distribution<int> noise(0, 30);

    CameraFrame f;
    f.frameId = ++m_frameId;
    f.timestampNs = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
    f.width = m_cfg.width;
    f.height = m_cfg.height;
    f.format = PixelFormat::Mono8;
    f.data.resize((size_t)m_cfg.width * (size_t)m_cfg.height);

    const int objCenter = (int)(m_frameId * 7 % m_cfg.width);
    const int objHalf = 60;
    for (int y = 0; y < m_cfg.height; ++y) {
        for (int x = 0; x < m_cfg.width; ++x) {
            uint8_t v = 200 - (uint8_t)noise(rng);
            const int dx = std::abs(x - objCenter);
            if (dx < objHalf) {
                v = (uint8_t)((m_frameId & 1) ? 60 : 110);
            }
            f.data[(size_t)y * m_cfg.width + (size_t)x] = v;
        }
    }

    if (m_cb) m_cb(f);
}

std::unique_ptr<ICamera> createCamera(const std::string& type) {
    if (type == "mock" || type.empty()) return std::make_unique<CameraMock>();
    return nullptr;
}

}}
