#ifndef CGS_SERIALPORT_H
#define CGS_SERIALPORT_H

// Tiny cross-platform RS232 wrapper. POSIX (macOS / Linux) via termios,
// Windows via Win32 CreateFile. Pulled in only by hardware/*Real* drivers
// to avoid dragging Qt or Boost into the algorithm core.

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace cgs {
namespace core {

enum class Parity   : uint8_t { None, Even, Odd };
enum class StopBits : uint8_t { One, Two };

struct SerialConfig {
    std::string port;        // POSIX: "/dev/cu.usbserial-XXX"  Win: "COM3"
    int baud = 9600;
    int dataBits = 8;
    Parity parity = Parity::None;
    StopBits stopBits = StopBits::One;
    int readTimeoutMs = 1000;
    int writeTimeoutMs = 1000;
};

class SerialPort {
public:
    SerialPort();
    ~SerialPort();
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open(const SerialConfig& cfg);
    void close();
    bool isOpen() const;

    // Returns bytes written (>=0 on success, <0 on error).
    int write(const void* data, size_t len);
    int writeString(const std::string& s);

    // Reads up to maxLen bytes. Blocks until at least 1 byte arrives or
    // readTimeoutMs elapses. Returns bytes read (0 on timeout, <0 on error).
    int read(void* buf, size_t maxLen);

    // Reads until delimiter byte appears, or timeout. Includes delimiter in result.
    std::vector<uint8_t> readUntil(uint8_t delim, int totalTimeoutMs);

    void flushInput();

    std::string lastError() const;

private:
    struct Impl;
    Impl* m_impl;
};

}}

#endif
