#pragma once
// XRayLibInterface.h — RECONSTRUCTED from XRayLib.lib + XRayLibd.lib symbol tables.
//
// Original source: E:\0_work\0_HBJH\code\Gangue_home_2\XRayLib\
// Every class, method, and signal was decoded from MSVC-mangled names.
//
// XRayLib is 袁工's Qt wrapper around the VJ/LN RS-232 X-ray source protocol.
// It is functionally equivalent to our XRaySerial.cpp but packaged as a DLL.
//
// Two implementations exist in the same DLL:
//   XRayLib   — production class (used by Gangue.exe)
//   XRayLIO   — alternate LIO-protocol variant
//
// Both implement XRayLibInterface (abstract Qt QObject base).
//
// Factory pattern:
//   auto* factory = XRayLibFactory::getInstance();
//   XRayLibInterface* xray = factory->create("XRay");  // or ""
//   connect(xray, &XRayLibInterface::signalXRayInfo, this, &MyClass::onXRayInfo);
//   xray->XRay_connect("COM1", &ok);
//   xray->XRay_openXRay();
//
// Key signals:
//   signalXRayInfo(double kv, int statusCode, double ma)   — live readings ~1Hz
//   signalXRayFLTState(QVariant faultBits)                 — packed fault flags
//   signalXRayState(QVariant state)                        — overall state

#ifndef XRAYLIBINTERFACE_H
#define XRAYLIBINTERFACE_H

#include <QObject>
#include <QString>
#include <QVariant>

// ---------------------------------------------------------------------------
// XRayLibInterface — abstract Qt QObject base for both XRayLib and XRayLIO.
// ---------------------------------------------------------------------------
class XRayLibInterface : public QObject {
    Q_OBJECT
public:
    virtual ~XRayLibInterface() = default;

    // Connection
    virtual bool XRay_connect(QString portName, bool* result = nullptr) = 0;
    virtual bool XRay_disconnect(bool* result = nullptr) = 0;

    // Beam control
    virtual void XRay_startup() = 0;
    virtual void XRay_openXRay() = 0;    // alias for startup (enable HV)
    virtual void XRay_closeXRay() = 0;   // shutdown HV

    // Parameter set
    virtual void XRay_setVoltage(double kv) = 0;
    virtual void XRay_setCurrent(double ma) = 0;
    virtual void XRay_setPower(double w) = 0;

    // Preheat
    virtual void XRay_setPreWarmStatus(int seconds) = 0;
    virtual void XRay_getPreWarmStatus(int& seconds) = 0;
    virtual void XRay_warmup() = 0;           // warmup with default duration
    virtual void XRay_warmup(int seconds) = 0; // warmup for N seconds

    // Range query (hardware limits)
    virtual void XRay_getVoltageRange(double& minKv, double& maxKv) = 0;
    virtual void XRay_getCurrentRange(double& minMa, double& maxMa) = 0;
    virtual void XRay_getPowerRange(double& minW, double& maxW) = 0;

    // Fault
    virtual void XRay_clearFault() = 0;

    // Status / diagnostics
    virtual int     getStatusCode() = 0;
    virtual QString getStatusStr() = 0;
    virtual int     getErrorCode() = 0;
    virtual QString getErrorStr() = 0;
    virtual bool    getIsConnected() = 0;
    virtual long long getRunTime() = 0;    // total tube ON-time in seconds

signals:
    // Emitted ~1 Hz with live kV, status code, and mA readings.
    void signalXRayInfo(double kv, int statusCode, double ma);

    // Fault state — QVariant wraps a bit-packed integer matching XRayFault fields.
    // Bit 0: regulation, bit 2: interlockOpen, bit 4: overVoltageCathode,
    // bit 6: overVoltageAnode, bit 8: overTemperature, bit 10: arcDetect,
    // bit 12: overCurrent, bit 14: powerLimit, bit 16: overVoltage.
    void signalXRayFLTState(QVariant faultBits);

    // Overall machine state — QVariant wraps an integer state code.
    void signalXRayState(QVariant state);

    // Operation event signals (emitted when async slot commands complete)
    void signalXRay_connect(QString portName, bool* result);
    void signalXRay_disconnect(bool* result);
    void signalXRay_openXRay();
    void signalXRay_closeXRay();
    void signalXRay_setVoltage(double kv);
    void signalXRay_setCurrent(double ma);
    void signalXRay_setPower(double w);
    void signalXRay_clearFault();
    void signalXRay_warmup1();       // warmup() no-arg version
    void signalXRay_warmup2(int seconds);
    void signalTest(QString msg);
    void signalTimerStoped();
    void signalThreadFinished();
    void signalStopStatusThread();
};

// ---------------------------------------------------------------------------
// XRayLibFactory — singleton that creates XRayLibInterface instances.
// ---------------------------------------------------------------------------
class XRayLibFactory {
public:
    // Singleton accessor.
    static XRayLibFactory* getInstance();

    // Create a new XRayLibInterface.
    // name: pass "" or "XRay" — the exact string is unverified but "" is safest.
    XRayLibInterface* create(QString name);

    // Get the most recently created instance.
    XRayLibInterface* get();

    XRayLibFactory(const XRayLibFactory&) = delete;
    XRayLibFactory& operator=(const XRayLibFactory&) = delete;

private:
    XRayLibFactory() = default;
};

#endif // XRAYLIBINTERFACE_H
