#ifndef CGS_PLCBECKHOFF_H
#define CGS_PLCBECKHOFF_H

// Beckhoff TwinCAT 3 ADS adapter for IPLC.
// Real implementation needs Beckhoff/ADS open-source library:
//   git clone https://github.com/Beckhoff/ADS third_party/ads
// Enable with -DCGS_HAS_BECKHOFF_ADS=ON.
//
// Protocol details and PLC variable contract:
//   docs/hardware/protocols/beckhoff-ads.md

#include "IPLC.h"

#ifdef CGS_HAS_BECKHOFF_ADS

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// Forward declarations from AdsLib so headers don't leak through cgs_hardware
// public API.
class AdsDevice;
struct AmsNetId;

namespace cgs {
namespace hardware {

// Mapping between cgs concepts and PLC GVL variable names. Loaded from
// config.xml so PLC engineer can rename variables without code changes.
struct BeckhoffPlcVariableMap {
    std::string conveyorFeeder      = "MAIN.bConveyorFeeder";
    std::string conveyorVibrator    = "MAIN.bConveyorVibrator";
    std::string conveyorPowder      = "MAIN.bConveyorPowder";
    std::string conveyorDetection   = "MAIN.bConveyorDetection";
    std::string conveyorAccepted    = "MAIN.bConveyorAccepted";
    std::string conveyorReject      = "MAIN.bConveyorReject";
    std::string detectionBeltHz     = "MAIN.rDetectionBeltSpeedHz";
    std::string detectionBeltActual = "MAIN.rDetectionBeltActualHz";
    std::string lampGreen           = "MAIN.bLampGreen";
    std::string lampYellow          = "MAIN.bLampYellow";
    std::string lampRed             = "MAIN.bLampRed";
    std::string emergencyStop       = "MAIN.bEmergencyStop";
    std::string airPressureOk       = "MAIN.bAirPressureOk";
    std::string xrayInterlock       = "MAIN.bXRayInterlockOk";
    std::string materialTrigger     = "MAIN.bMaterialTrigger";
};

class PLCBeckhoff : public IPLC {
public:
    PLCBeckhoff();
    ~PLCBeckhoff() override;

    // endpoint format: "ads://192.168.1.10:5.45.22.57.1.1:851"
    bool connect(const std::string& endpoint) override;
    void disconnect() override;

    bool setConveyor(ConveyorId id, bool on) override;
    bool setBeltSpeedHz(double hz) override;
    bool setLamp(LampColor color, LampState state) override;
    PLCStatus pollStatus() override;
    void subscribeMaterialTrigger(TriggerCallback cb) override;
    void subscribeEmergencyStop(EmergencyCallback cb) override;

    void setVariableMap(const BeckhoffPlcVariableMap& m) { m_vars = m; }

private:
    bool parseEndpoint(const std::string& s, std::string& ip, AmsNetId& netId, uint16_t& port);
    bool writeBool(const std::string& varName, bool v);
    bool writeReal(const std::string& varName, double v);
    bool readBool(const std::string& varName, bool& out);
    bool readReal(const std::string& varName, double& out);

    void notifyLoop();

    BeckhoffPlcVariableMap m_vars;
    std::unique_ptr<AdsDevice> m_dev;
    std::map<std::string, uint32_t> m_handleCache;
    std::mutex m_mtx;

    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_polling{false};
    std::thread m_pollThread;

    TriggerCallback m_triggerCb;
    EmergencyCallback m_estopCb;
};

}}

#endif // CGS_HAS_BECKHOFF_ADS
#endif
