#include "PLCBeckhoff.h"

#ifdef CGS_HAS_BECKHOFF_ADS

#include <AdsLib.h>           // from third_party/ads
#include <AdsNotificationOOI.h>

#include <chrono>
#include <cstring>
#include <sstream>

namespace cgs {
namespace hardware {

PLCBeckhoff::PLCBeckhoff() = default;
PLCBeckhoff::~PLCBeckhoff() { disconnect(); }

bool PLCBeckhoff::parseEndpoint(const std::string& s, std::string& ip,
                                 AmsNetId& netId, uint16_t& port) {
    // "ads://192.168.1.10:5.45.22.57.1.1:851"
    const std::string scheme = "ads://";
    if (s.rfind(scheme, 0) != 0) return false;
    std::string rest = s.substr(scheme.size());
    auto p1 = rest.find(':');
    if (p1 == std::string::npos) return false;
    ip = rest.substr(0, p1);
    rest = rest.substr(p1 + 1);
    auto p2 = rest.rfind(':');
    if (p2 == std::string::npos) return false;
    const std::string netIdStr = rest.substr(0, p2);
    port = (uint16_t)std::stoi(rest.substr(p2 + 1));
    int v[6] = {0};
    if (std::sscanf(netIdStr.c_str(), "%d.%d.%d.%d.%d.%d",
                    &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]) != 6) return false;
    for (int i = 0; i < 6; ++i) netId.b[i] = (uint8_t)v[i];
    return true;
}

bool PLCBeckhoff::connect(const std::string& endpoint) {
    if (m_connected) return false;
    std::string ip;
    AmsNetId net;
    uint16_t port = 851;
    if (!parseEndpoint(endpoint, ip, net, port)) return false;
    AdsAddRoute(net, ip.c_str());
    try {
        m_dev = std::make_unique<AdsDevice>(ip, net, port);
    } catch (...) {
        m_dev.reset();
        return false;
    }
    m_connected = true;
    m_polling = true;
    m_pollThread = std::thread([this]{ notifyLoop(); });
    return true;
}

void PLCBeckhoff::disconnect() {
    m_polling = false;
    if (m_pollThread.joinable()) m_pollThread.join();
    m_dev.reset();
    m_handleCache.clear();
    m_connected = false;
}

bool PLCBeckhoff::writeBool(const std::string& varName, bool v) {
    if (!m_dev) return false;
    try {
        auto h = m_dev->GetHandle(varName);
        uint8_t b = v ? 1 : 0;
        m_dev->WriteReqEx(0xF005 /*ADSIGRP_SYM_VALBYHND*/, *h, sizeof(b), &b);
        return true;
    } catch (...) { return false; }
}

bool PLCBeckhoff::writeReal(const std::string& varName, double v) {
    if (!m_dev) return false;
    try {
        auto h = m_dev->GetHandle(varName);
        float f = (float)v;
        m_dev->WriteReqEx(0xF005, *h, sizeof(f), &f);
        return true;
    } catch (...) { return false; }
}

bool PLCBeckhoff::readBool(const std::string& varName, bool& out) {
    if (!m_dev) return false;
    try {
        auto h = m_dev->GetHandle(varName);
        uint8_t b = 0; uint32_t got = 0;
        m_dev->ReadReqEx2(0xF005, *h, sizeof(b), &b, &got);
        out = (b != 0);
        return true;
    } catch (...) { return false; }
}

bool PLCBeckhoff::readReal(const std::string& varName, double& out) {
    if (!m_dev) return false;
    try {
        auto h = m_dev->GetHandle(varName);
        float f = 0.0f; uint32_t got = 0;
        m_dev->ReadReqEx2(0xF005, *h, sizeof(f), &f, &got);
        out = (double)f;
        return true;
    } catch (...) { return false; }
}

bool PLCBeckhoff::setConveyor(ConveyorId id, bool on) {
    switch (id) {
        case ConveyorId::Feeder:          return writeBool(m_vars.conveyorFeeder, on);
        case ConveyorId::VibratingScreen: return writeBool(m_vars.conveyorVibrator, on);
        case ConveyorId::PowderBelt:      return writeBool(m_vars.conveyorPowder, on);
        case ConveyorId::DetectionBelt:   return writeBool(m_vars.conveyorDetection, on);
        case ConveyorId::AcceptedBelt:    return writeBool(m_vars.conveyorAccepted, on);
        case ConveyorId::RejectBelt:      return writeBool(m_vars.conveyorReject, on);
    }
    return false;
}

bool PLCBeckhoff::setBeltSpeedHz(double hz) {
    if (hz < 0.0 || hz > 60.0) return false;
    return writeReal(m_vars.detectionBeltHz, hz);
}

bool PLCBeckhoff::setLamp(LampColor color, LampState state) {
    const std::string& var = (color == LampColor::Red)    ? m_vars.lampRed :
                              (color == LampColor::Yellow) ? m_vars.lampYellow :
                                                             m_vars.lampGreen;
    // Mock blinking server-side; here we only do on/off.
    return writeBool(var, state != LampState::Off);
}

PLCStatus PLCBeckhoff::pollStatus() {
    PLCStatus s;
    s.connected = m_connected;
    if (!m_connected) return s;
    bool b = false;
    readBool(m_vars.emergencyStop, b);  s.emergencyStop = b;
    readBool(m_vars.airPressureOk, b);  s.airPressureOk = b;
    readBool(m_vars.xrayInterlock, b);  s.xrayInterlockOk = b;
    double r = 0.0;
    readReal(m_vars.detectionBeltActual, r);
    s.detectionBeltSpeedHz = r;
    s.detectionBeltSpeedMps = r * (2.5 / 50.0);
    return s;
}

void PLCBeckhoff::subscribeMaterialTrigger(TriggerCallback cb) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_triggerCb = std::move(cb);
}

void PLCBeckhoff::subscribeEmergencyStop(EmergencyCallback cb) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_estopCb = std::move(cb);
}

// Polling-based notification fallback. For lower latency, switch to
// AdsDevice::AddDeviceNotification with an OnChange handler.
void PLCBeckhoff::notifyLoop() {
    using clock = std::chrono::steady_clock;
    bool prevTrigger = false;
    bool prevEstop = false;
    while (m_polling) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 50 Hz
        bool t = false, e = false;
        readBool(m_vars.materialTrigger, t);
        readBool(m_vars.emergencyStop, e);

        if (t && !prevTrigger) {
            const uint64_t ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    clock::now().time_since_epoch()).count();
            std::lock_guard<std::mutex> lk(m_mtx);
            if (m_triggerCb) m_triggerCb(ns);
        }
        if (e && !prevEstop) {
            std::lock_guard<std::mutex> lk(m_mtx);
            if (m_estopCb) m_estopCb();
        }
        prevTrigger = t;
        prevEstop = e;
    }
}

}}

#endif // CGS_HAS_BECKHOFF_ADS
