#include "ACSMotionClient.h"

#include <cstring>
#include <cstdio>
#include <string>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   using SockLen = int;
#  define SOCK_INVALID INVALID_SOCKET
#  define SOCK_ERR     SOCKET_ERROR
#  define closeSocket  closesocket
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using SockLen = socklen_t;
#  define SOCK_INVALID (-1)
#  define SOCK_ERR     (-1)
#  define closeSocket  close
   using SOCKET = int;
#endif

namespace cgs {
namespace hardware {

ACSMotionClient::~ACSMotionClient() {
    disconnect();
}

bool ACSMotionClient::connect(const std::string& ip, int port, int /*timeoutMs*/) {
    if (m_connected) return true;

#ifdef _WIN32
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == SOCK_INVALID) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::connect(s, (sockaddr*)&addr, (SockLen)sizeof(addr)) == SOCK_ERR) {
        closeSocket(s);
        std::fprintf(stderr, "[ACSMotionClient] cannot connect to %s:%d\n",
                     ip.c_str(), port);
        return false;
    }

    m_sock = (int)s;
    m_ip   = ip;
    m_port = port;
    m_connected = true;
    std::fprintf(stderr, "[ACSMotionClient] connected to %s:%d\n", ip.c_str(), port);
    return true;
}

void ACSMotionClient::disconnect() {
    if (!m_connected) return;
    m_connected = false;
    if (m_sock != -1) {
        closeSocket((SOCKET)m_sock);
        m_sock = -1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

bool ACSMotionClient::sendCommand(const std::string& cmd, std::string* response) {
    if (!m_connected || m_sock == -1) return false;

    // ACS SPII protocol: ASCII command + CR LF
    const std::string frame = cmd + "\r\n";
    int sent = (int)send((SOCKET)m_sock, frame.c_str(), (int)frame.size(), 0);
    if (sent == SOCK_ERR) {
        m_connected = false;
        return false;
    }

    if (response) {
        char buf[512] = {};
        int n = (int)recv((SOCKET)m_sock, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            *response = buf;
        }
    }
    return true;
}

bool ACSMotionClient::enableBelt(bool on) {
    // ACS SPII: ENABLE 0 / DISABLE 0
    // Axis 0 is typically the conveyor axis; confirm with 刘工.
    const std::string cmd = on ? "ENABLE 0" : "DISABLE 0";
    return sendCommand(cmd);
}

bool ACSMotionClient::isBeltRunning() {
    std::string resp;
    if (!sendCommand("?MFLAGS(0)", &resp)) return false;
    // MFLAGS bit 0 = motor enabled; non-zero means running.
    // Minimal parse: any non-zero response indicates belt moving.
    for (char c : resp) {
        if (c != '0' && c != '\r' && c != '\n' && c != ' ') return true;
    }
    return false;
}

}}
