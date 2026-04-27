#ifndef IPLC_H
#define IPLC_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace cgs {
namespace hardware {

enum class ConveyorId {
    Feeder,            // 上料输送机 7.5 kW
    VibratingScreen,   // 振动分筛 15 kW
    PowderBelt,        // 粉末料输送带 4.5 kW
    DetectionBelt,     // 检测输送带 4 kW (variable speed)
    AcceptedBelt,      // 合格料输送带 5.5 kW
    RejectBelt         // 废料输送带 5.5 kW
};

enum class LampColor { Red, Yellow, Green };
enum class LampState { Off, On, Blinking };

struct PLCStatus {
    bool connected = false;
    bool emergencyStop = false;
    bool airPressureOk = true;
    bool xrayInterlockOk = true;
    double detectionBeltSpeedHz = 0.0;       // commanded variable-frequency
    double detectionBeltSpeedMps = 0.0;      // computed from feedback
    bool conveyorRunning[6] = {false};
    std::string lastError;
};

// Real implementation target: Beckhoff TwinCAT 3 + EtherCAT
// Hardware: FC9022 (PCIe EtherCAT master) + EK1501 + EL2828×20 (160 DO)
// Communication: ADS protocol over TCP/IP (port 48898)
// Library: https://github.com/Beckhoff/ADS (open source, MIT)
// Endpoint format for real impl: "ads://5.45.22.57.1.1:851" (AMS Net ID : AMS Port)
class IPLC {
public:
    virtual ~IPLC() = default;

    virtual bool connect(const std::string& endpoint) = 0;   // mock: "mock://localhost" | real: "ads://x.x.x.x.x.x:851"
    virtual void disconnect() = 0;

    virtual bool setConveyor(ConveyorId id, bool on) = 0;
    virtual bool setBeltSpeedHz(double hz) = 0;              // detection belt only
    virtual bool setLamp(LampColor color, LampState state) = 0;

    virtual PLCStatus pollStatus() = 0;

    using TriggerCallback = std::function<void(uint64_t timestampNs)>;
    virtual void subscribeMaterialTrigger(TriggerCallback cb) = 0;

    using EmergencyCallback = std::function<void()>;
    virtual void subscribeEmergencyStop(EmergencyCallback cb) = 0;
};

std::unique_ptr<IPLC> createPLC(const std::string& type);

}}

#endif
