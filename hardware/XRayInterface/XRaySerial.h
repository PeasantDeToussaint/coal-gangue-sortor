#ifndef CGS_XRAYSERIAL_H
#define CGS_XRAYSERIAL_H

// VJ Technologies X-ray generator driver (RS232).
// Supports the IXS200BP500P479 family. Protocol fully decoded in
// docs/hardware/protocols/vj-xray-rs232.md, ported from legacy
// thickness-meter project XRayLib.cpp.

#include "IXRaySource.h"
#include "../../core/Serial/SerialPort.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace cgs {
namespace hardware {

struct XRayFault {
    bool regulation = false;
    bool interlockOpen = false;
    bool overVoltageCathode = false;
    bool overVoltageAnode = false;
    bool overTemperature = false;
    bool arcDetect = false;
    bool overCurrent = false;
    bool powerLimit = false;
    bool overVoltage = false;
    bool any() const {
        return regulation || interlockOpen || overVoltageCathode || overVoltageAnode
             || overTemperature || arcDetect || overCurrent || powerLimit || overVoltage;
    }
};

class XRaySerial : public IXRaySource {
public:
    XRaySerial();
    ~XRaySerial() override;

    // configPath format: "serial:///dev/cu.usbserial-XX" or "serial://COM3"
    bool open(const std::string& configPath) override;
    void close() override;

    bool startup() override;
    bool shutdown() override;

    bool setKv(double kv) override;
    bool setMa(double ma) override;

    XRayStatus pollStatus() override;
    void setStatusCallback(StatusCallback cb) override;

    // VJ-specific extras
    bool setWatchDog(bool enabled);
    bool setPreheatSeconds(int seconds);
    bool clearFaults();
    XRayFault lastFaults() const;

private:
    void monitorLoop();
    bool sendCommand(const std::string& asciiCmd);
    std::string readResponseLine(int timeoutMs);

    static std::string formatNumber(double value, int width);

    cgs::core::SerialPort m_port;
    std::atomic<bool> m_open{false};
    std::atomic<bool> m_hvOn{false};
    std::atomic<double> m_kvSet{0.0};
    std::atomic<double> m_uaSet{0.0};
    std::atomic<double> m_kvActual{0.0};
    std::atomic<double> m_uaActual{0.0};
    std::atomic<double> m_tempC{0.0};

    mutable std::mutex m_faultMtx;
    XRayFault m_lastFaults;

    std::atomic<bool> m_monitorRun{false};
    std::thread m_monitor;
    StatusCallback m_cb;
    std::mutex m_ioMtx;
};

}}

#endif
