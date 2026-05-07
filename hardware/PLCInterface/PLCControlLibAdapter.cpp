#include "PLCControlLibAdapter.h"

#ifdef CGS_HAS_PLCCONTROLLIB

#include "../../third_party/plccontrollib/PLCControlLibInterface.h"

#include <cstdio>
#include <cstring>

namespace cgs {
namespace hardware {

PLCControlLibAdapter::PLCControlLibAdapter() {
    m_plc = new PLCControlLib(this);

    QObject::connect(m_plc, &PLCControlLib::signalReadInt136Value,
                     this,  &PLCControlLibAdapter::onReadInt136Value,
                     Qt::QueuedConnection);
    // signalReadValue delivers individual item reads (item=DB byte offset, value=float).
    // Belt speed at DB15 byte offset 146 may arrive here rather than in the 136-int batch.
    // Confirmed from PLCControlLib.dll.c: individual PLC_DBReadInt calls → signalReadValue.
    QObject::connect(m_plc, &PLCControlLib::signalReadValue,
                     this,  &PLCControlLibAdapter::onReadValue,
                     Qt::QueuedConnection);
    QObject::connect(m_plc, &PLCControlLib::signalSocketError,
                     this,  &PLCControlLibAdapter::onSocketError,
                     Qt::QueuedConnection);
}

PLCControlLibAdapter::~PLCControlLibAdapter() {
    disconnect();
}

bool PLCControlLibAdapter::connect(const std::string& endpoint) {
    // endpoint: "s7://192.168.2.100:102"
    std::string ip = "192.168.2.100";
    int port = 102;
    const std::string scheme = "s7://";
    if (endpoint.rfind(scheme, 0) == 0) {
        const std::string rest = endpoint.substr(scheme.size());
        const auto col = rest.rfind(':');
        if (col != std::string::npos) {
            ip   = rest.substr(0, col);
            port = std::stoi(rest.substr(col + 1));
        } else {
            ip = rest;
        }
    }
    m_ip   = ip;
    m_port = port;

    // PLCControlLib::PLC_Connect(ip, port, serialport0)
    // serialport0 is the COM port for PLC auxiliary serial (e.g. COM5 from config)
    // Third parameter is PLC model number, NOT a serial port.
    // Confirmed from PLCControlLib.dll.c getPLCType():
    //   splits on '-', takes number after dash → determines COTP TSAP
    //   "S7-1500" → splits to "1500" → TSAP 0x0101 (same as 200, 1200)
    //   "S7-300"  → "300" → TSAP 0x0201
    //   "S7-400"  → "400" → TSAP 0x0301
    m_plc->PLC_Connect(QString::fromStdString(ip),
                       (unsigned int)port,
                       "S7-1500");  // PLC model → selects correct COTP TSAP
    m_connected = true;

    // Read belt speed individually at byte offset 146 in DB15.
    // PLC_DBReadInt(db=15, len=2, offset=146) → response via signalReadValue(146, value, name)
    // Also request the status booleans individually.
    m_plc->PLC_DBReadInt(15, 2, 146);   // belt speed (皮带速度) at byte 146
    m_plc->PLC_DBReadInt(15, 2, 89);    // emergency stop (急停状态) at byte 89
    m_plc->PLC_DBReadInt(15, 2, 101);   // X-ray interlock at byte 101
    return true;
}

void PLCControlLibAdapter::disconnect() {
    if (!m_connected.exchange(false)) return;
    if (m_plc) m_plc->PLC_DisConnect();
}

bool PLCControlLibAdapter::setConveyor(ConveyorId /*id*/, bool /*on*/) {
    return false;  // Belt start/stop via HMI / S7 program
}

bool PLCControlLibAdapter::setBeltSpeedHz(double /*hz*/) {
    return false;
}

bool PLCControlLibAdapter::setLamp(LampColor /*color*/, LampState /*state*/) {
    return false;
}

PLCStatus PLCControlLibAdapter::pollStatus() {
    std::lock_guard<std::mutex> lk(m_statusMtx);
    return m_status;
}

void PLCControlLibAdapter::subscribeMaterialTrigger(TriggerCallback cb) {
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_triggerCb = std::move(cb);
}

void PLCControlLibAdapter::subscribeEmergencyStop(EmergencyCallback cb) {
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_estopCb = std::move(cb);
}

// ---------------------------------------------------------------------------
// Slot — called when PLCControlLib delivers the 136-int DB15 batch
// ---------------------------------------------------------------------------
void PLCControlLibAdapter::onReadInt136Value(int* values, int count) {
    if (!values || count < 1) return;

    PLCStatus s;
    s.connected = m_connected;

    // Belt speed at DB15 byte offset 146.
    // In the 136-INT16 batch, each element is 2 bytes → array index = 146/2 = 73.
    // Confirmed from PLCControlLib.dll.c: INT16 big-endian, 2 bytes per element.
    if (m_beltSpeedIndex < count) {
        const float raw = (float)values[m_beltSpeedIndex];  // index 73 = byte offset 146
        s.detectionBeltSpeedMps = (double)(raw / 1000.0f);  // raw unit TBD on-site
        s.detectionBeltSpeedHz  = 0.0;
    }

    // Array indices = byte_offset / 2 (each INT16 element is 2 bytes):
    // Emergency stop: byte 89 → index 44
    const int estopIdx = 89 / 2;  // = 44
    if (estopIdx < count) {
        const bool estop = (values[estopIdx] != 0);
        s.emergencyStop = estop;
        if (estop) {
            EmergencyCallback cb;
            { std::lock_guard<std::mutex> lk(m_cbMtx); cb = m_estopCb; }
            if (cb) cb();
        }
    }

    // X-ray interlock: byte 101 → index 50
    const int xrayIdx = 101 / 2;  // = 50
    if (xrayIdx < count) s.xrayInterlockOk = (values[xrayIdx] != 0);

    // Air pressure: byte 142 → index 71
    const int pressIdx = 142 / 2;  // = 71
    if (pressIdx < count) s.airPressureOk = (values[pressIdx] > 0);

    {
        std::lock_guard<std::mutex> lk(m_statusMtx);
        m_status = s;
    }

    // Re-schedule next read (continuous polling)
    if (m_connected && m_plc) {
        m_plc->PLC_DBReadInt(15, 136 * 4, 0);
    }
}

// ---------------------------------------------------------------------------
// Slot — individual item read result: signalReadValue(byteOffset, value, name)
// Confirmed from PLCControlLib.dll.c: PLC_DBReadInt(db, 2, offset) delivers here.
// ---------------------------------------------------------------------------
void PLCControlLibAdapter::onReadValue(int itemIndex, float value, QString /*name*/) {
    std::lock_guard<std::mutex> lk(m_statusMtx);
    m_status.connected = m_connected;

    if (itemIndex == 146) {
        // Belt speed at byte offset 146 in DB15.
        // Unit: raw integer — likely mm/s or scaled. Scale factor TBD on-site.
        // At nominal Speed=2035 mm/s, expect value ≈ 2035 (if raw mm/s) or similar.
        m_status.detectionBeltSpeedMps = (double)(value / 1000.0f);  // assume mm/s
    } else if (itemIndex == 89) {
        m_status.emergencyStop = (value != 0);
    } else if (itemIndex == 101) {
        m_status.xrayInterlockOk = (value != 0);
    } else if (itemIndex == 142) {
        m_status.airPressureOk = (value > 0);
    }

    // Re-schedule reads for continuous polling
    if (m_connected && m_plc) {
        m_plc->PLC_DBReadInt(15, 2, 146);
        m_plc->PLC_DBReadInt(15, 2, 89);
        m_plc->PLC_DBReadInt(15, 2, 101);
    }
}

void PLCControlLibAdapter::onSocketError(QString error) {
    std::fprintf(stderr, "[PLCControlLibAdapter] socket error: %s\n",
                 error.toStdString().c_str());
    m_connected = false;
    std::lock_guard<std::mutex> lk(m_statusMtx);
    m_status.connected = false;
}

}} // cgs::hardware

#endif // CGS_HAS_PLCCONTROLLIB
