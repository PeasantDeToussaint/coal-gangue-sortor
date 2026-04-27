#include "DetectorMock.h"

#ifdef CGS_HAS_AURORA_SDK
#include "DetectorAurora.h"
#endif

#include <chrono>
#include <random>

namespace cgs {
namespace hardware {

DetectorMock::DetectorMock() = default;
DetectorMock::~DetectorMock() { close(); }

bool DetectorMock::open(const DetectorConfig& cfg) {
    if (m_running) return false;
    m_cfg = cfg;
    if (m_cfg.width <= 0) m_cfg.width = 1024;
    if (m_cfg.lineRateHz <= 0) m_cfg.lineRateHz = 1000;
    return true;
}

void DetectorMock::close() {
    stop();
}

bool DetectorMock::start() {
    if (m_running.exchange(true)) return false;
    m_thread = std::thread([this]{ run(); });
    return true;
}

bool DetectorMock::stop() {
    if (!m_running.exchange(false)) return false;
    if (m_thread.joinable()) m_thread.join();
    return true;
}

void DetectorMock::setFrameCallback(FrameCallback cb) { m_cb = std::move(cb); }

void DetectorMock::run() {
    using clock = std::chrono::steady_clock;
    const auto period = std::chrono::nanoseconds(1'000'000'000LL / m_cfg.lineRateHz);
    auto next = clock::now();

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> objWidth(20, 80);
    std::uniform_int_distribution<int> objStart(0, std::max(1, m_cfg.width - 100));
    std::uniform_int_distribution<int> objKind(0, 2); // 0=none, 1=coal, 2=gangue

    while (m_running) {
        DetectorFrame f;
        f.frameId = ++m_frameId;
        f.timestampNs = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                            clock::now().time_since_epoch()).count();
        f.width = m_cfg.width;
        f.height = 1;
        f.bitsPerPixel = 16;
        f.data.assign((size_t)m_cfg.width, 60000); // bright = empty belt

        if (objKind(rng) > 0) {
            const int start = objStart(rng);
            const int len = objWidth(rng);
            const uint16_t baseAtten = (objKind(rng) == 1) ? 35000 : 18000;
            for (int i = 0; i < len && start + i < m_cfg.width; ++i) {
                f.data[start + i] = (uint16_t)(baseAtten + (rng() % 4000));
            }
        }

        if (m_cb) m_cb(f);

        next += period;
        std::this_thread::sleep_until(next);
    }
}

std::unique_ptr<IDetector> createDetector(const std::string& type) {
    if (type == "mock" || type.empty()) return std::make_unique<DetectorMock>();
#ifdef CGS_HAS_AURORA_SDK
    if (type == "aurora") return std::make_unique<DetectorAurora>();
#endif
    return nullptr;
}

}}
