#ifndef CGS_XRAYLIBADAPTER_H
#define CGS_XRAYLIBADAPTER_H

// XRayLibAdapter — wraps XRayLib.dll (袁工's VJ RS-232 wrapper) as IXRaySource.
//
// This is the preferred adapter for the production machine — it uses the same
// XRayLib.dll that Gangue.exe uses, so serial port enumeration, preheat
// sequencing, and watchdog are handled identically.
//
// Requires Qt (XRayLibInterface is a QObject). Enabled with:
//   cmake -DCGS_HAS_XRAYLIB=ON -DCGS_XRAYLIB_DIR=third_party/xraylib
//
// Startup sequence:
//   xray->open("serial://COM1")   → XRay_connect("COM1")
//   xray->setKv(200.0)            → XRay_setVoltage(200.0)
//   xray->setMa(2.3)              → XRay_setCurrent(2.3)
//   xray->startup()               → XRay_openXRay()
//   xray->shutdown()              → XRay_closeXRay()
//
// Live status signals feed XRaySettingsDialog fault display at ~1 Hz.

#include "IXRaySource.h"

#ifdef CGS_HAS_XRAYLIB

#include <QObject>
#include <QVariant>
#include <atomic>
#include <mutex>
#include <string>

class XRayLibInterface;

namespace cgs {
namespace hardware {

class XRayLibAdapter : public QObject, public IXRaySource {
    Q_OBJECT
public:
    XRayLibAdapter();
    ~XRayLibAdapter() override;

    bool open(const std::string& configPath) override;
    void close() override;
    bool startup() override;
    bool shutdown() override;
    bool setKv(double kv) override;
    bool setMa(double ma) override;
    XRayStatus pollStatus() override;
    void setStatusCallback(StatusCallback cb) override;

    // VJ extras (forwarded to XRayLib)
    bool setPreheatSeconds(int seconds);
    bool clearFaults();
    bool setWatchDog(bool enabled);

private slots:
    // Connected to XRayLibInterface signals for live status updates
    void onXRayInfo(double kv, int statusCode, double ma);
    void onXRayFLTState(QVariant faultBits);
    void onXRayState(QVariant state);

private:
    XRayLibInterface*   m_xray{nullptr};
    StatusCallback      m_cb;
    mutable std::mutex  m_cbMtx;
    std::atomic<bool>   m_hvOn{false};
    double              m_kvActual{0.0};
    double              m_maActual{0.0};
    int                 m_statusCode{0};
    uint32_t            m_faultBits{0};
    std::string         m_lastPort;
};

}} // cgs::hardware

#endif // CGS_HAS_XRAYLIB
#endif // CGS_XRAYLIBADAPTER_H
