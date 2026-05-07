#include "PLCS7.h"

#ifdef CGS_HAS_SNAP7

#include <snap7.h>

#include <chrono>
#include <cstring>

namespace cgs {
namespace hardware {

PLCS7::PLCS7() {
    m_client = Cli_Create();
}

PLCS7::~PLCS7() {
    disconnect();
    if (m_client) Cli_Destroy(&m_client);
}

// ---------------------------------------------------------------------------
// parseEndpoint — "s7://192.168.2.100:102"
// ---------------------------------------------------------------------------
static bool parseS7Endpoint(const std::string& s, std::string& ip, int& port) {
    const std::string scheme = "s7://";
    if (s.rfind(scheme, 0) != 0) return false;
    const std::string rest = s.substr(scheme.size());
    const auto col = rest.rfind(':');
    if (col != std::string::npos) {
        ip   = rest.substr(0, col);
        port = std::stoi(rest.substr(col + 1));
    } else {
        ip   = rest;
        port = 102;
    }
    return !ip.empty();
}

// ---------------------------------------------------------------------------
// connect
// ---------------------------------------------------------------------------
bool PLCS7::connect(const std::string& endpoint) {
    if (m_connected) return false;

    int port = 102;
    if (!parseS7Endpoint(endpoint, m_ip, port)) return false;
    m_port = port;

    // S7-1500: rack=0, slot=1
    int res = Cli_ConnectTo(m_client,
                            m_ip.c_str(),
                            m_rack,
                            m_slot);
    if (res != 0) return false;

    m_connected = true;
    m_polling   = true;
    m_pollThread = std::thread([this]{ notifyLoop(); });
    return true;
}

void PLCS7::disconnect() {
    m_polling = false;
    if (m_pollThread.joinable()) m_pollThread.join();
    if (m_client) Cli_Disconnect(m_client);
    m_connected = false;
}

// ---------------------------------------------------------------------------
// read helpers
// ---------------------------------------------------------------------------
bool PLCS7::readReal(int db, int offset, float& out) {
    if (!m_connected) return false;
    uint8_t buf[4]{};
    int res = Cli_DBRead(m_client, db, offset, 4, buf);
    if (res != 0) return false;
    // S7 REAL is big-endian IEEE 754 float.
    uint32_t raw = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
                 | ((uint32_t)buf[2] <<  8) |  (uint32_t)buf[3];
    std::memcpy(&out, &raw, 4);
    return true;
}

bool PLCS7::readBool(int db, int byteOffset, int bitOffset, bool& out) {
    if (!m_connected) return false;
    uint8_t byte = 0;
    int res = Cli_DBRead(m_client, db, byteOffset, 1, &byte);
    if (res != 0) return false;
    out = ((byte >> bitOffset) & 0x01) != 0;
    return true;
}

// ---------------------------------------------------------------------------
// IPLC interface
// ---------------------------------------------------------------------------
bool PLCS7::setConveyor(ConveyorId /*id*/, bool /*on*/) {
    // Belt start/stop is handled by the S7 program via HMI or dedicated DI/DO.
    // Exposing this via software requires agreed-upon DB variable addresses.
    // Implement per customer PLC program.
    return false;
}

bool PLCS7::setBeltSpeedHz(double /*hz*/) {
    // Belt speed setpoint is written via G120 VFD → write to S7 DB if mapped.
    return false;
}

bool PLCS7::setLamp(LampColor /*color*/, LampState /*state*/) {
    return false;
}

PLCStatus PLCS7::pollStatus() {
    PLCStatus s;
    s.connected = m_connected;
    if (!m_connected) return s;

    // Belt speed (REAL from G120 speed feedback)
    float speedHz = 0.0f;
    if (readReal(m_vars.beltSpeedDb, m_vars.beltSpeedOffset, speedHz)) {
        s.detectionBeltSpeedHz = (double)speedHz;
        s.detectionBeltSpeedMps = (double)speedHz * m_vars.vfdHzToMps;
    }

    // Safety interlocks (BOOL, bit 0 of byte)
    bool b = false;
    if (readBool(m_vars.emergencyStopDb, m_vars.emergencyStopOffset, 0, b))
        s.emergencyStop = b;
    if (readBool(m_vars.airPressureOkDb, m_vars.airPressureOkOffset, 0, b))
        s.airPressureOk = b;
    if (readBool(m_vars.xrayInterlockDb, m_vars.xrayInterlockOffset, 0, b))
        s.xrayInterlockOk = b;

    return s;
}

void PLCS7::subscribeMaterialTrigger(TriggerCallback cb) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_triggerCb = std::move(cb);
}

void PLCS7::subscribeEmergencyStop(EmergencyCallback cb) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_estopCb = std::move(cb);
}

// ---------------------------------------------------------------------------
// notifyLoop — poll S7 at 20 Hz for e-stop events.
// ---------------------------------------------------------------------------
void PLCS7::notifyLoop() {
    bool prevEstop = false;
    while (m_polling) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!m_connected) continue;

        bool estop = false;
        readBool(m_vars.emergencyStopDb, m_vars.emergencyStopOffset, 0, estop);
        if (estop && !prevEstop) {
            std::lock_guard<std::mutex> lk(m_mtx);
            if (m_estopCb) m_estopCb();
        }
        prevEstop = estop;
    }
}

}}

#endif // CGS_HAS_SNAP7
