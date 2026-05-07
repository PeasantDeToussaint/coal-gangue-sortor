#include "BeiduAdapter.h"

#ifdef CGS_HAS_BECKHOFF_ADS

#include <AdsLib.h>

#include <cstdio>
#include <cstring>

namespace cgs {
namespace hardware {

struct BeiduAdapter::AdsConn {
    AmsAddr addr{};
    long    port{0};
};

BeiduAdapter::BeiduAdapter() = default;

BeiduAdapter::~BeiduAdapter() {
    close();
}

bool BeiduAdapter::open(const std::string& netId,
                        const std::string& routerIp,
                        uint32_t indexGroup,
                        uint32_t indexOffset) {
    if (m_open) return false;

    m_indexGroup  = indexGroup;   // 0x3040030
    m_indexOffset = indexOffset;  // 0x81000000 (Beifu secondary, NOT 0x81000006)

    // Parse "n1.n2.n3.n4.n5.n6"
    AmsNetId amsNetId{};
    int v[6] = {0};
    if (std::sscanf(netId.c_str(), "%d.%d.%d.%d.%d.%d",
                    &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        std::fprintf(stderr, "[BeiduAdapter] invalid AMS NetID: %s\n", netId.c_str());
        return false;
    }
    for (int i = 0; i < 6; ++i) amsNetId.b[i] = (uint8_t)v[i];

    AdsAddRoute(amsNetId, routerIp.c_str());

    m_conn = std::make_unique<AdsConn>();
    m_conn->port = AdsPortOpen();
    if (m_conn->port == 0) {
        m_conn.reset();
        return false;
    }
    m_conn->addr.netId = amsNetId;
    m_conn->addr.port  = 851;   // TwinCAT PLC runtime port

    // Probe: write disable byte to confirm ADS route is alive
    const uint8_t disableVal = 0;
    long err = AdsSyncWriteReq(&m_conn->addr,
                               m_indexGroup, m_indexOffset,
                               1, const_cast<uint8_t*>(&disableVal));
    if (err != 0) {
        std::fprintf(stderr, "[BeiduAdapter] probe write failed: %ld\n", err);
        AdsPortClose(m_conn->port);
        m_conn.reset();
        return false;
    }

    m_open = true;
    return true;
}

void BeiduAdapter::close() {
    if (m_open) enable(false);
    if (m_conn && m_conn->port != 0) {
        AdsPortClose(m_conn->port);
    }
    m_conn.reset();
    m_open = false;
}

bool BeiduAdapter::enable(bool on) {
    if (!m_conn || m_conn->port == 0) return false;
    const uint8_t val = on ? 1 : 0;
    long err = AdsSyncWriteReq(&m_conn->addr,
                               m_indexGroup, m_indexOffset,
                               1, const_cast<uint8_t*>(&val));
    if (err != 0) {
        std::fprintf(stderr, "[BeiduAdapter] enable(%d) failed: %ld\n", (int)on, err);
        return false;
    }
    return true;
}

}}

#else // !CGS_HAS_BECKHOFF_ADS

// Stub for non-Beckhoff builds.
namespace cgs { namespace hardware {
BeiduAdapter::BeiduAdapter() = default;
BeiduAdapter::~BeiduAdapter() = default;
bool BeiduAdapter::open(const std::string&, const std::string&, uint32_t, uint32_t) {
    return false;
}
void BeiduAdapter::close() {}
bool BeiduAdapter::enable(bool) { return false; }
}}

#endif // CGS_HAS_BECKHOFF_ADS
