#ifndef ICAMERA_H
#define ICAMERA_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cgs {
namespace hardware {

enum class PixelFormat {
    Mono8,
    Mono16,
    BGR8,
    BayerRG8
};

struct CameraFrame {
    uint64_t frameId = 0;
    uint64_t timestampNs = 0;
    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::Mono8;
    std::vector<uint8_t> data;
};

struct CameraConfig {
    std::string ipOrSerial;              // GigE IP address or USB serial
    int width = 2048;
    int height = 1024;
    double exposureUs = 1000.0;
    double gainDb = 0.0;
    bool hardwareTrigger = false;        // true = external trigger (para0_0_4=1)
    int frameRateHz = 30;
    // Optional: Hikvision .ccf device feature file (from MVS software).
    // If non-empty, CameraHikGigE::open() calls MV_CC_FeatureLoad() with this path.
    std::string ccfPath;
};

class ICamera {
public:
    virtual ~ICamera() = default;

    virtual bool open(const CameraConfig& cfg) = 0;
    virtual void close() = 0;

    virtual bool startStreaming() = 0;
    virtual bool stopStreaming() = 0;

    virtual bool triggerSoft() = 0;

    using FrameCallback = std::function<void(const CameraFrame&)>;
    virtual void setFrameCallback(FrameCallback cb) = 0;

    virtual bool isStreaming() const = 0;
};

std::unique_ptr<ICamera> createCamera(const std::string& type);

}}

#endif
