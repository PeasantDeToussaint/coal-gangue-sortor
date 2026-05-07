#ifndef CGS_ACSMOTIONCLIENT_H
#define CGS_ACSMOTIONCLIENT_H

// ACS Motion Controller TCP client (minimal).
//
// The ACS controller at 10.0.0.100 (default port 7070) manages belt motion
// on some machine installations. The Type="CP" in the original config.xml
// refers to the ACS "Control Protocol" (OpenClaw / EtherCAT master TCP API).
//
// This adapter provides the minimum needed for the sorter:
//   - Connect / disconnect
//   - Start / stop belt (conveyor enable command)
//   - Query belt running state
//
// The ACS SPII/CPCL protocol commands sent over TCP are plain-text ASCII
// with a CR+LF terminator. Key commands observed in OpenClaw documentation:
//   "ENABLE <axis>"  / "DISABLE <axis>"  — axis motor enable/disable
//   "RUNA <buffer>"                        — run a named motion program
//   "HALT <axis>"                          — halt motion
//   "?MFLAGS(<axis>)"                      — query motion flags
//
// If the machine does NOT use ACS (belt driven purely by S7 + G120 VFD),
// this client will fail to connect and log a warning; everything else
// continues operating normally. Confirm with 刘工 whether ACS is wired
// for belt on this specific installation.
//
// From real machine config.xml:
//   <ACSInfo IP="10.0.0.100" Type="CP"/>
// From D:\ listing: OpenClaw Windows software confirmed installed on PC.

#include <atomic>
#include <functional>
#include <string>

namespace cgs {
namespace hardware {

class ACSMotionClient {
public:
    ACSMotionClient() = default;
    ~ACSMotionClient();

    // Connect to ACS controller.
    // ip: controller IP (default 10.0.0.100)
    // port: TCP port (default 7070 for ACS OpenClaw CP)
    // timeoutMs: connection timeout
    bool connect(const std::string& ip = "10.0.0.100",
                 int port = 7070,
                 int timeoutMs = 2000);
    void disconnect();
    bool isConnected() const { return m_connected; }

    // Belt (axis 0) control commands.
    // Returns false if not connected or command rejected.
    bool enableBelt(bool on);
    bool isBeltRunning();

    // Send a raw ASCII command and return the response (blocking, ≤500 ms).
    bool sendCommand(const std::string& cmd, std::string* response = nullptr);

private:
    int  m_sock{-1};
    std::atomic<bool> m_connected{false};
    std::string m_ip;
    int  m_port{7070};
};

}}

#endif
