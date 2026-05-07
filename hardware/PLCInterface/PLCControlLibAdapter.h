#ifndef CGS_PLCCONTROLLIBADAPTER_H
#define CGS_PLCCONTROLLIBADAPTER_H

// PLCControlLibAdapter — wraps PLCControlLib.dll (袁工's S7 TCP adapter).
//
// Uses PLCControlLib's signalReadInt136Value to receive DB15 data in one batch.
// Belt speed is at index (beltSpeedOffset - batchStartOffset) in that array.
//
// Enabled with: cmake -DCGS_HAS_PLCCONTROLLIB=ON
//
// Advantage over snap7: no external library needed; PLCControlLib.dll is already
// deployed on the production machine alongside Gangue.exe.

#include "IPLC.h"

#ifdef CGS_HAS_PLCCONTROLLIB

#include <QObject>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>

class PLCControlLib;

namespace cgs {
namespace hardware {

class PLCControlLibAdapter : public QObject, public IPLC {
    Q_OBJECT
public:
    PLCControlLibAdapter();
    ~PLCControlLibAdapter() override;

    bool connect(const std::string& endpoint) override;
    void disconnect() override;
    bool setConveyor(ConveyorId id, bool on) override;
    bool setBeltSpeedHz(double hz) override;
    bool setLamp(LampColor color, LampState state) override;
    PLCStatus pollStatus() override;
    void subscribeMaterialTrigger(TriggerCallback cb) override;
    void subscribeEmergencyStop(EmergencyCallback cb) override;

    // Configure which index in the 136-int batch is belt speed.
    // Default: index 146 (matches PLCState panel item 146 = 皮带速度).
    void setBeltSpeedIndex(int idx) { m_beltSpeedIndex = idx; }

private slots:
    void onReadInt136Value(int* values, int count);
    void onReadValue(int itemIndex, float value, QString name);
    void onSocketError(QString error);

private:
    PLCControlLib*      m_plc{nullptr};
    std::atomic<bool>   m_connected{false};
    mutable std::mutex  m_statusMtx;
    PLCStatus           m_status;
    TriggerCallback     m_triggerCb;
    EmergencyCallback   m_estopCb;
    std::mutex          m_cbMtx;
    // Belt speed is at DB15 byte offset 146 (INT16, 2 bytes).
    // In the 136-element INT16 batch array (signalReadInt136Value), each element is 2 bytes:
    //   array index = byte_offset / 2 = 146 / 2 = 73
    // In signalReadValue(itemIndex, ...), itemIndex IS the byte offset = 146.
    int                 m_beltSpeedIndex{73};   // array index in 136-int batch (byte 146 / 2)

    std::string         m_ip;
    int                 m_port{102};
};

}} // cgs::hardware

#endif // CGS_HAS_PLCCONTROLLIB
#endif // CGS_PLCCONTROLLIBADAPTER_H
