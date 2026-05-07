#ifndef CGS_VALVEDRIVERBECKOFFLIB_H
#define CGS_VALVEDRIVERBECKOFFLIB_H

// ValveDriverBeckHoffLib — valve driver wrapping BeckHoffLib.dll.
//
// Simpler alternative to ValveDriverEL2828 (which uses the open-source AdsLib).
// BeckHoffLib.dll is already deployed alongside Gangue.exe and requires no
// extra SDK setup — it is the DLL 袁工 used directly.
//
// BeckHoffLib.dll differences from our open-source ADS approach:
//   - Reads ADS addresses from config.xml via ParseConfigXml (not from URI)
//   - Uses the official TcAdsDll.DLL (not open-source AdsLib)
//   - The Init() call handles ADS routing automatically
//
// Enabled with: cmake -DCGS_HAS_BECKOFFLIB=ON
//
// The 1 ms scheduler loop and nozzle command queue are identical to
// ValveDriverEL2828 — only the bulk write call changes.

#include "IValveDriver.h"

#ifdef CGS_HAS_BECKOFFLIB

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cgs {
namespace hardware {

class ValveDriverBeckHoffLib : public IValveDriver {
public:
    ValveDriverBeckHoffLib();
    ~ValveDriverBeckHoffLib() override;

    bool open(const std::string& endpoint) override;
    void close() override;
    bool arm() override;
    bool disarm() override;
    bool schedule(const std::vector<NozzleCommand>& batch) override;
    ValveDriverStatus pollStatus() override;
    void setFaultCallback(FaultCallback cb) override;
    int channelCount() const override { return m_channelCount; }

private:
    void schedulerLoop();
    bool bulkWrite(const std::vector<uint8_t>& states);

    int      m_channelCount{136};
    uint32_t m_indexGroup{0x3040030};
    bool     m_open{false};
    std::atomic<bool> m_armed{false};
    std::atomic<bool> m_schedulerRun{false};
    std::thread       m_scheduler;
    std::mutex        m_pendingMtx;
    std::vector<NozzleCommand> m_pending;
    FaultCallback m_fault;
    std::atomic<uint64_t> m_accepted{0};
    std::atomic<uint64_t> m_rejected{0};
};

}} // cgs::hardware

#endif // CGS_HAS_BECKOFFLIB
#endif // CGS_VALVEDRIVERBECKOFFLIB_H
