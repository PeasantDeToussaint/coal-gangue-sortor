#ifndef IDETECTOR_H
#define IDETECTOR_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cgs {
namespace hardware {

struct DetectorFrame {
    uint64_t frameId = 0;
    uint64_t timestampNs = 0;
    int width = 0;                       // pixels per row
    int height = 1;                      // line-scan = 1; 2D detector = N
    int bitsPerPixel = 16;
    std::vector<uint16_t> data;          // size = width * height
};

struct DetectorConfig {
    std::string ipOrSerial{};            // detector IP (e.g. "192.168.1.199") or serial path
    int width = 1024;
    int lineRateHz = 1000;               // 1 kHz typical
    int gain = 0;
    int integrationTimeUs = 800;
};

class IDetector {
public:
    virtual ~IDetector() = default;

    virtual bool open(const DetectorConfig& cfg) = 0;
    virtual void close() = 0;

    virtual bool start() = 0;
    virtual bool stop() = 0;

    using FrameCallback = std::function<void(const DetectorFrame&)>;
    virtual void setFrameCallback(FrameCallback cb) = 0;

    virtual bool isRunning() const = 0;

    // Flat-field calibration.
    // calibrateDark():   X-ray OFF, acquire offset map  → saves calibrateImageXray.tif
    // calibrateBright(): X-ray ON, no material, acquire gain map → updates calibration
    // Returns false if not connected or calibration fails.
    // Default implementation returns false (unsupported); override in real adapters.
    virtual bool calibrateDark()   { return false; }
    virtual bool calibrateBright() { return false; }
};

std::unique_ptr<IDetector> createDetector(const std::string& type);

}}

#endif
