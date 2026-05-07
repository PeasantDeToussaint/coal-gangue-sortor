#pragma once
// PLCControlLibInterface.h — RECONSTRUCTED from PLCControlLib.lib symbol table.
//
// PLCControlLib is 袁工's Siemens S7 PLC adapter using raw QTcpSocket with
// ISO-on-TCP (S7comm) frames. It does NOT use snap7.
//
// S7 connection frames visible in PLCControlLib.dll:
//   03 00 00 16 11 E0 00 00 00 01 00 C1 02 01 00 C2 02 01 01 C0 01 09  (slot 1)
//   03 00 00 16 11 E0 00 00 00 01 00 C1 02 01 00 C2 02 02 01 C0 01 09  (slot 2)
//   03 00 00 16 11 E0 00 00 00 01 00 C1 02 01 00 C2 02 03 01 C0 01 09  (slot 3)
//
// Internal arrays (matching signal batch sizes):
//   arrIntVar61[61]       → signalReadInt61Value
//   arrayFloatVar29[29]   → signalReadFloat29Value
//   arrayIntVar136[136]   → signalReadInt136Value  ← belt speed is inside this
//
// Key signals:
//   signalReadInt136Value(int* values, int count)  — 136-item batch from DB15
//   signalReadFloat29Value(float* values, int count) — 29 sensor floats
//   signalReadValue(int item, float val, QString name) — individual item
//
// Usage:
//   PLCControlLib* plc = new PLCControlLib();
//   plc->PLC_Connect("192.168.2.100", 102, 0);  // ip, port, type(0=S7-1500)
//   connect(plc, &PLCControlLib::signalReadInt136Value, this, &MyClass::onDB15);
//   plc->setDBLenOffset(15, 4, 146);  // DB15, 4-byte float, offset 146
//   plc->PLC_DBReadFloat(15, 4, 146); // async — result arrives via signal

#ifndef PLCCONTROLLIBINTERFACE_H
#define PLCCONTROLLIBINTERFACE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>

class PLCControlLib : public QObject {
    Q_OBJECT
public:
    explicit PLCControlLib(QObject* parent = nullptr);
    ~PLCControlLib() override;

    // Connection
    // type: 0 = S7-1500, 1 = S7-300/400 (Serialport0 field in config is the COM port)
    void PLC_Connect(QString ip, unsigned int port, QString serialport0);
    void PLC_DisConnect();
    bool PLC_isConnected();
    QString getPLCType(QString& typeOut);

    // Async DB read — results delivered via signal*Value signals
    void PLC_DBReadFloat(int db, int len, int offset);
    void PLC_DBReadInt(int db, int len, int offset);
    void PLC_DBReadStr(int db, int len, int offset);

    // Set the DB/len/offset for the next read (alternative to passing in call)
    void setDBLenOffset(int db, int len, int offset);

    // DB write
    void PLC_DBWriteFloat(int db, int len, int offset, QStringList values);
    void PLC_DBWriteInt(int db, int len, int offset, QStringList values);
    void PLC_DBWriteStr(int db, int len, int offset, QStringList values);

    // Bool read/write (X = inputs, Y = outputs in S7 nomenclature)
    void PLC_ReadBoolX();
    void PLC_ReadBoolY();
    void PLC_WriteBoolX();
    void PLC_WriteBoolY();

    // Internal: process raw received bytes
    void receiveData(QByteArray data);
    void BtnGene();

signals:
    // Batch reads (DB15 layout matches PLCState panel order)
    void signalReadInt136Value(int* values, int count);    // 136 ints from DB15
    void signalReadFloat29Value(float* values, int count); // 29 floats (sensors)
    void signalReadInt61Value(int* values, int count);     // 61 ints (subset)

    // Individual read result
    void signalReadValue(int itemIndex, float value, QString name);

    // Connection events
    void signalConnectSer(QString ip, unsigned int port, QString type);
    void signalConnected(QString info);
    void signalDisConnSer();
    void signalSocketError(QString error);

    // Raw data
    void signalReceiveData(QByteArray data);
    void signalSend(QString data);
    void signalTrans(QByteArray data);
    void signalStopThread();
};

#endif // PLCCONTROLLIBINTERFACE_H
