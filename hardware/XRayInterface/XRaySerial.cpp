#include "XRaySerial.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace cgs {
namespace hardware {

namespace {
constexpr char STX = 0x02;
constexpr char CR  = 0x0D;
}

XRaySerial::XRaySerial() = default;
XRaySerial::~XRaySerial() { close(); }

std::string XRaySerial::formatNumber(double value, int width) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%0*.0f", width, value);
    return buf;
}

bool XRaySerial::open(const std::string& configPath) {
    if (m_open) return false;
    std::string portName = configPath;
    const std::string scheme = "serial://";
    if (portName.rfind(scheme, 0) == 0) portName = portName.substr(scheme.size());
    if (portName.empty()) return false;

    cgs::core::SerialConfig cfg;
    cfg.port = portName;
    cfg.baud = 9600;
    cfg.dataBits = 8;
    cfg.parity = cgs::core::Parity::None;
    cfg.stopBits = cgs::core::StopBits::One;
    cfg.readTimeoutMs = 200;
    cfg.writeTimeoutMs = 1000;

    if (!m_port.open(cfg)) return false;
    m_open = true;

    setWatchDog(false);   // matches legacy XRayLib::slotXRay_connect

    m_monitorRun = true;
    m_monitor = std::thread([this]{ monitorLoop(); });
    return true;
}

void XRaySerial::close() {
    if (!m_open.exchange(false)) return;
    m_monitorRun = false;
    if (m_monitor.joinable()) m_monitor.join();
    if (m_hvOn) shutdown();
    m_port.close();
}

bool XRaySerial::startup() {
    if (!m_open) return false;
    if (!sendCommand("ENBL1")) return false;
    m_hvOn = true;
    if (m_cb) m_cb(pollStatus());
    return true;
}

bool XRaySerial::shutdown() {
    if (!m_open) return false;
    sendCommand("ENBL0");
    m_hvOn = false;
    if (m_cb) m_cb(pollStatus());
    return true;
}

bool XRaySerial::setKv(double kv) {
    if (kv < 0 || kv > 250.0) return false;
    m_kvSet = kv;
    return sendCommand("VP" + formatNumber(kv * 10.0, 4));
}

bool XRaySerial::setMa(double ma) {
    // VJ accepts μA; our public IXRaySource API talks mA for cleanliness.
    const double ua = ma * 1000.0;
    if (ua < 0 || ua > 9999) return false;
    m_uaSet = ua;
    return sendCommand("CP" + formatNumber(ua, 4));
}

bool XRaySerial::setWatchDog(bool enabled) {
    return sendCommand(enabled ? "WDOG1" : "WDOG0");
}

bool XRaySerial::setPreheatSeconds(int seconds) {
    if (seconds < 0 || seconds > 9999) return false;
    return sendCommand("PTM" + formatNumber(seconds, 4));
}

bool XRaySerial::clearFaults() { return sendCommand("CLR"); }

XRayFault XRaySerial::lastFaults() const {
    std::lock_guard<std::mutex> lk(m_faultMtx);
    return m_lastFaults;
}

XRayStatus XRaySerial::pollStatus() {
    XRayStatus s;
    s.ready = m_open;
    s.highVoltageOn = m_hvOn;
    s.kvActual = m_kvActual;
    s.maActual = m_uaActual / 1000.0;
    s.tubeTemperatureC = m_tempC;
    const auto faults = lastFaults();
    if (faults.interlockOpen) s.lastError = "interlock open";
    else if (faults.overTemperature) s.lastError = "over temperature";
    else if (faults.arcDetect) s.lastError = "arc detected";
    else if (faults.any()) s.lastError = "fault";
    return s;
}

void XRaySerial::setStatusCallback(StatusCallback cb) { m_cb = std::move(cb); }

bool XRaySerial::sendCommand(const std::string& asciiCmd) {
    if (!m_open) return false;
    std::lock_guard<std::mutex> lk(m_ioMtx);
    std::string framed;
    framed.reserve(asciiCmd.size() + 2);
    framed += STX;
    framed += asciiCmd;
    framed += CR;
    int n = m_port.write(framed.data(), framed.size());
    return n == (int)framed.size();
}

std::string XRaySerial::readResponseLine(int timeoutMs) {
    auto bytes = m_port.readUntil((uint8_t)CR, timeoutMs);
    return std::string(bytes.begin(), bytes.end());
}

void XRaySerial::monitorLoop() {
    using clock = std::chrono::steady_clock;
    auto next = clock::now();

    while (m_monitorRun) {
        next += std::chrono::seconds(1);
        std::this_thread::sleep_until(next);
        if (!m_open) continue;

        // 1) Fault query
        if (sendCommand("FLT")) {
            const std::string resp = readResponseLine(500);
            if (resp.size() >= 19 && resp.front() == STX && resp.back() == CR) {
                XRayFault f;
                auto bit = [&](size_t pos){ return pos < resp.size() && resp[pos] == '1'; };
                f.regulation         = bit(1);
                f.interlockOpen      = bit(3);
                f.overVoltageCathode = bit(5);
                f.overVoltageAnode   = bit(7);
                f.overTemperature    = bit(9);
                f.arcDetect          = bit(11);
                f.overCurrent        = bit(13);
                f.powerLimit         = bit(15);
                f.overVoltage        = bit(17);
                std::lock_guard<std::mutex> lk(m_faultMtx);
                m_lastFaults = f;
            }
        }

        // 2) Monitor kV / uA / T
        if (sendCommand("MON")) {
            const std::string resp = readResponseLine(500);
            if (resp.size() >= 16 && resp.front() == STX) {
                // Format: STX kkkk uuuu tttt CR  (per legacy parser, with decimals at fixed positions)
                auto digit = [&](size_t i){ return resp.size() > i ? resp[i] : '0'; };
                char kvStr[8]={digit(1),digit(2),digit(3),'.',digit(4),0,0,0};
                char uaStr[8]={digit(6),digit(7),digit(8),digit(9),0,0,0,0};
                char tStr[8] ={digit(11),digit(12),digit(13),'.',digit(14),0,0,0};
                m_kvActual = std::atof(kvStr);
                m_uaActual = std::atof(uaStr);
                m_tempC    = std::atof(tStr);
            }
        }

        if (m_cb) m_cb(pollStatus());
    }
}

}}
