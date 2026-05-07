#include "XRayLibAdapter.h"

#ifdef CGS_HAS_XRAYLIB

#include "../../third_party/xraylib/XRayLibInterface.h"

#include <cstdio>

namespace cgs {
namespace hardware {

XRayLibAdapter::XRayLibAdapter() = default;

XRayLibAdapter::~XRayLibAdapter() {
    close();
}

bool XRayLibAdapter::open(const std::string& configPath) {
    // Strip "serial://" prefix
    std::string port = configPath;
    const std::string scheme = "serial://";
    if (port.rfind(scheme, 0) == 0) port = port.substr(scheme.size());
    if (port.empty()) return false;

    XRayLibFactory* factory = XRayLibFactory::getInstance();
    if (!factory) {
        std::fprintf(stderr, "[XRayLibAdapter] factory getInstance() failed\n");
        return false;
    }

    m_xray = factory->create("");
    if (!m_xray) m_xray = factory->create("XRay");
    if (!m_xray) {
        std::fprintf(stderr, "[XRayLibAdapter] factory->create() returned null\n");
        return false;
    }

    // Connect live-status signals (re13: XRayLib live signals)
    QObject::connect(m_xray, &XRayLibInterface::signalXRayInfo,
                     this,   &XRayLibAdapter::onXRayInfo,
                     Qt::QueuedConnection);
    QObject::connect(m_xray, &XRayLibInterface::signalXRayFLTState,
                     this,   &XRayLibAdapter::onXRayFLTState,
                     Qt::QueuedConnection);
    QObject::connect(m_xray, &XRayLibInterface::signalXRayState,
                     this,   &XRayLibAdapter::onXRayState,
                     Qt::QueuedConnection);

    bool ok = false;
    m_xray->XRay_connect(QString::fromStdString(port), &ok);
    m_lastPort = port;
    return ok;
}

void XRayLibAdapter::close() {
    if (!m_xray) return;
    if (m_hvOn) shutdown();
    bool ok = false;
    m_xray->XRay_disconnect(&ok);
    QObject::disconnect(m_xray, nullptr, this, nullptr);
    m_xray = nullptr;
}

bool XRayLibAdapter::startup() {
    if (!m_xray) return false;
    m_xray->XRay_openXRay();
    m_hvOn = true;
    return true;
}

bool XRayLibAdapter::shutdown() {
    if (!m_xray) return false;
    m_xray->XRay_closeXRay();
    m_hvOn = false;
    return true;
}

bool XRayLibAdapter::setKv(double kv) {
    if (!m_xray) return false;
    m_xray->XRay_setVoltage(kv);
    return true;
}

bool XRayLibAdapter::setMa(double ma) {
    if (!m_xray) return false;
    m_xray->XRay_setCurrent(ma);
    return true;
}

bool XRayLibAdapter::setPreheatSeconds(int seconds) {
    if (!m_xray) return false;
    m_xray->XRay_setPreWarmStatus(seconds);
    return true;
}

bool XRayLibAdapter::clearFaults() {
    if (!m_xray) return false;
    m_xray->XRay_clearFault();
    return true;
}

bool XRayLibAdapter::setWatchDog(bool /*enabled*/) {
    // XRayLib handles watchdog internally; no direct API.
    return true;
}

XRayStatus XRayLibAdapter::pollStatus() {
    XRayStatus s;
    s.ready         = (m_xray != nullptr);
    s.highVoltageOn = m_hvOn;
    s.kvActual      = m_kvActual;
    s.maActual      = m_maActual;
    if (m_faultBits != 0) s.lastError = "fault (code " + std::to_string(m_statusCode) + ")";
    return s;
}

void XRayLibAdapter::setStatusCallback(StatusCallback cb) {
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_cb = std::move(cb);
}

// ---------------------------------------------------------------------------
// Slots — receive live status from XRayLib at ~1 Hz
// ---------------------------------------------------------------------------
void XRayLibAdapter::onXRayInfo(double kv, int statusCode, double ma) {
    m_kvActual    = kv;
    m_maActual    = ma;
    m_statusCode  = statusCode;

    StatusCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_cb;
    }
    if (cb) cb(pollStatus());
}

void XRayLibAdapter::onXRayFLTState(QVariant faultBits) {
    m_faultBits = (uint32_t)faultBits.toUInt();
}

void XRayLibAdapter::onXRayState(QVariant /*state*/) {
    // State changes handled through pollStatus() / statusCallback.
}

}} // cgs::hardware

#endif // CGS_HAS_XRAYLIB
