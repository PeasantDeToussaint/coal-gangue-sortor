#include "XRayMock.h"

namespace cgs {
namespace hardware {

XRayMock::XRayMock() = default;
XRayMock::~XRayMock() { close(); }

bool XRayMock::open(const std::string& /*configPath*/) {
    m_open = true;
    return true;
}

void XRayMock::close() {
    shutdown();
    m_open = false;
}

bool XRayMock::startup() {
    if (!m_open) return false;
    m_on = true;
    if (m_cb) m_cb(pollStatus());
    return true;
}

bool XRayMock::shutdown() {
    m_on = false;
    if (m_cb) m_cb(pollStatus());
    return true;
}

bool XRayMock::setKv(double kv) { m_kv = kv; return true; }
bool XRayMock::setMa(double ma) { m_ma = ma; return true; }

XRayStatus XRayMock::pollStatus() {
    XRayStatus s;
    s.ready = m_open;
    s.highVoltageOn = m_on;
    s.kvActual = m_on ? m_kv : 0.0;
    s.maActual = m_on ? m_ma : 0.0;
    s.tubeTemperatureC = 35.0;
    return s;
}

void XRayMock::setStatusCallback(StatusCallback cb) { m_cb = std::move(cb); }

std::unique_ptr<IXRaySource> createXRaySource(const std::string& type) {
    if (type == "mock" || type.empty()) return std::make_unique<XRayMock>();
    return nullptr;
}

}}
