#ifndef IXRAYSOURCE_H
#define IXRAYSOURCE_H

#include <functional>
#include <memory>
#include <string>

namespace cgs {
namespace hardware {

struct XRayStatus {
    bool ready = false;
    bool highVoltageOn = false;
    double kvActual = 0.0;
    double maActual = 0.0;
    double tubeTemperatureC = 0.0;
    std::string lastError;
};

class IXRaySource {
public:
    virtual ~IXRaySource() = default;

    virtual bool open(const std::string& configPath) = 0;
    virtual void close() = 0;

    // X 射线源属于"慢启动"设备，需要预热数秒。调用后通过 onStatusChanged 回调通知就绪。
    virtual bool startup() = 0;
    virtual bool shutdown() = 0;

    virtual bool setKv(double kv) = 0;
    virtual bool setMa(double ma) = 0;

    virtual XRayStatus pollStatus() = 0;

    using StatusCallback = std::function<void(const XRayStatus&)>;
    virtual void setStatusCallback(StatusCallback cb) = 0;
};

std::unique_ptr<IXRaySource> createXRaySource(const std::string& type);

}}

#endif
