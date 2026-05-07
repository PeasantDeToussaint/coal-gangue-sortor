#ifndef CGS_PLCS7_H
#define CGS_PLCS7_H

// Siemens S7-1500 PLC adapter using the snap7 library.
//
// Enabled with: cmake -DCGS_HAS_SNAP7=ON
//
// The real machine has a Siemens S7-1500 PLC at 192.168.2.100:102 that:
//   - Controls belt motor start/stop via G120 VFD
//   - Reports actual belt speed back (used for ejection timing)
//   - Monitors safety interlocks (e-stop, air pressure, X-ray interlock)
//
// This is the PRIMARY belt-speed source for TimingCalculator.
// DO NOT conflate with PLCBeckhoff which connects to the EtherCAT controller
// (Beckhoff handles nozzle firing, S7-1500 handles belt/safety logic).
//
// Endpoint format: s7://<ip>:<port>
// Example: s7://192.168.2.100:102
//
// Variable address configuration: set DB number and byte offsets in config.xml
// <plc> section so the software can be adapted to the customer's PLC program.

#include "IPLC.h"

#ifdef CGS_HAS_SNAP7

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace cgs {
namespace hardware {

struct S7PlcVarConfig {
    int beltSpeedDb{100};           // DB number for belt speed REAL
    int beltSpeedOffset{0};         // byte offset
    int emergencyStopDb{100};       // DB number for E-stop BOOL
    int emergencyStopOffset{10};
    int airPressureOkDb{100};
    int airPressureOkOffset{11};
    int xrayInterlockDb{100};
    int xrayInterlockOffset{12};
    double vfdHzToMps{0.05};        // 50 Hz → 2.5 m/s; adjust via belt calibration
};

class PLCS7 : public IPLC {
public:
    PLCS7();
    ~PLCS7() override;

    // endpoint: "s7://192.168.2.100:102"
    bool connect(const std::string& endpoint) override;
    void disconnect() override;

    bool setConveyor(ConveyorId id, bool on) override;
    bool setBeltSpeedHz(double hz) override;
    bool setLamp(LampColor color, LampState state) override;

    PLCStatus pollStatus() override;

    void subscribeMaterialTrigger(TriggerCallback cb) override;
    void subscribeEmergencyStop(EmergencyCallback cb) override;

    // Fine-tune variable address mapping after construction.
    void setVarConfig(const S7PlcVarConfig& cfg) { m_vars = cfg; }
    S7PlcVarConfig varConfig() const { return m_vars; }

private:
    bool readReal(int db, int offset, float& out);
    bool readBool(int db, int byteOffset, int bitOffset, bool& out);
    void notifyLoop();

    void* m_client{nullptr};   // opaque S7Object (snap7 handle)
    std::atomic<bool>  m_connected{false};
    std::atomic<bool>  m_polling{false};
    std::thread        m_pollThread;
    std::mutex         m_mtx;
    TriggerCallback    m_triggerCb;
    EmergencyCallback  m_estopCb;
    S7PlcVarConfig     m_vars;
    std::string        m_ip;
    int                m_port{102};
    int                m_rack{0};
    int                m_slot{1};   // S7-1500: slot 1
};

}}

#endif // CGS_HAS_SNAP7
#endif
