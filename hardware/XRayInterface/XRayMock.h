#ifndef XRAYMOCK_H
#define XRAYMOCK_H

#include "IXRaySource.h"

#include <atomic>

namespace cgs {
namespace hardware {

class XRayMock : public IXRaySource {
public:
    XRayMock();
    ~XRayMock() override;

    bool open(const std::string& configPath) override;
    void close() override;

    bool startup() override;
    bool shutdown() override;

    bool setKv(double kv) override;
    bool setMa(double ma) override;

    XRayStatus pollStatus() override;

    void setStatusCallback(StatusCallback cb) override;

private:
    std::atomic<bool> m_open{false};
    std::atomic<bool> m_on{false};
    double m_kv = 80.0;
    double m_ma = 5.0;
    StatusCallback m_cb;
};

}}

#endif
