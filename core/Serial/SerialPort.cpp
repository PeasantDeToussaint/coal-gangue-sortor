#include "SerialPort.h"

#include <chrono>
#include <cstring>
#include <thread>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <errno.h>
  #include <fcntl.h>
  #include <poll.h>
  #include <sys/ioctl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

namespace cgs {
namespace core {

#if defined(_WIN32)

struct SerialPort::Impl {
    HANDLE handle = INVALID_HANDLE_VALUE;
    SerialConfig cfg;
    std::string err;

    bool open(const SerialConfig& c) {
        cfg = c;
        std::string fullName = c.port;
        if (fullName.rfind("\\\\.\\", 0) != 0) fullName = std::string("\\\\.\\") + c.port;
        handle = CreateFileA(fullName.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            err = "CreateFile failed";
            return false;
        }
        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle, &dcb)) { err = "GetCommState"; close(); return false; }
        dcb.BaudRate = (DWORD)c.baud;
        dcb.ByteSize = (BYTE)c.dataBits;
        dcb.Parity   = (c.parity == Parity::Even) ? EVENPARITY :
                       (c.parity == Parity::Odd)  ? ODDPARITY  : NOPARITY;
        dcb.StopBits = (c.stopBits == StopBits::Two) ? TWOSTOPBITS : ONESTOPBIT;
        dcb.fBinary = TRUE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl  = DTR_CONTROL_DISABLE;
        dcb.fRtsControl  = RTS_CONTROL_DISABLE;
        dcb.fOutX = FALSE;
        dcb.fInX  = FALSE;
        if (!SetCommState(handle, &dcb)) { err = "SetCommState"; close(); return false; }
        COMMTIMEOUTS to{};
        to.ReadIntervalTimeout = MAXDWORD;
        to.ReadTotalTimeoutConstant = (DWORD)c.readTimeoutMs;
        to.WriteTotalTimeoutConstant = (DWORD)c.writeTimeoutMs;
        SetCommTimeouts(handle, &to);
        PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
        return true;
    }
    void close() {
        if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); handle = INVALID_HANDLE_VALUE; }
    }
    bool isOpen() const { return handle != INVALID_HANDLE_VALUE; }
    int write(const void* data, size_t len) {
        if (!isOpen()) return -1;
        DWORD wrote = 0;
        return WriteFile(handle, data, (DWORD)len, &wrote, nullptr) ? (int)wrote : -1;
    }
    int read(void* buf, size_t maxLen) {
        if (!isOpen()) return -1;
        DWORD got = 0;
        return ReadFile(handle, buf, (DWORD)maxLen, &got, nullptr) ? (int)got : -1;
    }
    void flushInput() { if (isOpen()) PurgeComm(handle, PURGE_RXCLEAR); }
};

#else  // POSIX

struct SerialPort::Impl {
    int fd = -1;
    SerialConfig cfg;
    std::string err;

    static speed_t baudConst(int baud) {
        switch (baud) {
            case 1200:   return B1200;
            case 2400:   return B2400;
            case 4800:   return B4800;
            case 9600:   return B9600;
            case 19200:  return B19200;
            case 38400:  return B38400;
            case 57600:  return B57600;
            case 115200: return B115200;
            default:     return B9600;
        }
    }

    bool open(const SerialConfig& c) {
        cfg = c;
        fd = ::open(c.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) { err = strerror(errno); return false; }
        struct termios tio{};
        if (tcgetattr(fd, &tio) != 0) { err = strerror(errno); close(); return false; }
        cfmakeraw(&tio);
        speed_t b = baudConst(c.baud);
        cfsetispeed(&tio, b);
        cfsetospeed(&tio, b);
        tio.c_cflag &= ~CSIZE;
        tio.c_cflag |= (c.dataBits == 7) ? CS7 : CS8;
        tio.c_cflag |= (CLOCAL | CREAD);
        if (c.parity == Parity::None) tio.c_cflag &= ~PARENB;
        else { tio.c_cflag |= PARENB; if (c.parity == Parity::Odd) tio.c_cflag |= PARODD; else tio.c_cflag &= ~PARODD; }
        if (c.stopBits == StopBits::Two) tio.c_cflag |= CSTOPB; else tio.c_cflag &= ~CSTOPB;
        tio.c_cflag &= ~CRTSCTS;
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 0;  // we manage timeouts via poll()
        if (tcsetattr(fd, TCSANOW, &tio) != 0) { err = strerror(errno); close(); return false; }
        tcflush(fd, TCIOFLUSH);
        return true;
    }
    void close() { if (fd >= 0) { ::close(fd); fd = -1; } }
    bool isOpen() const { return fd >= 0; }

    int write(const void* data, size_t len) {
        if (!isOpen()) return -1;
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::milliseconds(cfg.writeTimeoutMs);
        size_t total = 0;
        const uint8_t* p = (const uint8_t*)data;
        while (total < len) {
            ssize_t n = ::write(fd, p + total, len - total);
            if (n > 0) { total += (size_t)n; continue; }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (std::chrono::steady_clock::now() > deadline) return (int)total;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            err = strerror(errno);
            return -1;
        }
        return (int)total;
    }

    int read(void* buf, size_t maxLen) {
        if (!isOpen()) return -1;
        struct pollfd pfd{ fd, POLLIN, 0 };
        int pr = poll(&pfd, 1, cfg.readTimeoutMs);
        if (pr == 0) return 0;
        if (pr < 0) { err = strerror(errno); return -1; }
        ssize_t n = ::read(fd, buf, maxLen);
        if (n < 0) { err = strerror(errno); return -1; }
        return (int)n;
    }

    void flushInput() { if (isOpen()) tcflush(fd, TCIFLUSH); }
};

#endif

SerialPort::SerialPort() : m_impl(new Impl()) {}
SerialPort::~SerialPort() { close(); delete m_impl; }

bool SerialPort::open(const SerialConfig& cfg) { return m_impl->open(cfg); }
void SerialPort::close() { m_impl->close(); }
bool SerialPort::isOpen() const { return m_impl->isOpen(); }
int SerialPort::write(const void* data, size_t len) { return m_impl->write(data, len); }
int SerialPort::writeString(const std::string& s) { return write(s.data(), s.size()); }
int SerialPort::read(void* buf, size_t maxLen) { return m_impl->read(buf, maxLen); }
void SerialPort::flushInput() { m_impl->flushInput(); }
std::string SerialPort::lastError() const { return m_impl->err; }

std::vector<uint8_t> SerialPort::readUntil(uint8_t delim, int totalTimeoutMs) {
    std::vector<uint8_t> out;
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::milliseconds(totalTimeoutMs);
    uint8_t b;
    while (clock::now() < deadline) {
        int n = read(&b, 1);
        if (n > 0) {
            out.push_back(b);
            if (b == delim) return out;
        } else if (n == 0) {
            // timeout slice; loop back if total deadline still allows
        } else {
            return out;
        }
    }
    return out;
}

}}
