#ifndef CGS_VALVEDRIVEREL2828_H
#define CGS_VALVEDRIVEREL2828_H

// ValveDriver via Beckhoff EL2828 digital outputs (over ADS).
// Uses installed Beckhoff stack (FC9022 + EL2828 modules). Timing precision
// is bounded by the EtherCAT / TwinCAT task cycle (typically ~1 ms), which
// matches the DF8 valve mechanical response time of 5–15 ms.

#include "IValveDriver.h"

#ifdef CGS_HAS_BECKHOFF_ADS

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

class AdsDevice;

namespace cgs {
namespace hardware {

class ValveDriverEL2828 : public IValveDriver {
public:
    ValveDriverEL2828();
    ~ValveDriverEL2828() override;

    // endpoint same format as PLCBeckhoff: "ads://192.168.1.10:5.45.22.57.1.1:851"
    bool open(const std::string& endpoint) override;
    void close() override;

    bool arm() override;
    bool disarm() override;
    bool schedule(const std::vector<NozzleCommand>& batch) override;
    bool selfTest(uint32_t pulseMs) override;
    ValveDriverStatus pollStatus() override;
    void setFaultCallback(FaultCallback cb) override;

    // PLC variable name template (sprintf-style with %d for nozzle id 1..64).
    // Default expects PLC GVL: bValveCh01 .. bValveCh64
    void setChannelVarTemplate(const std::string& tmpl) { m_chTemplate = tmpl; }

private:
    void schedulerLoop();
    bool writeChannel(int nozzleId, bool on);

    std::unique_ptr<AdsDevice> m_dev;
    std::string m_chTemplate{"MAIN.bValveCh%02d"};
    std::map<int, uint32_t> m_handleCache;
    std::mutex m_mtx;

    std::atomic<bool> m_open{false};
    std::atomic<bool> m_armed{false};
    std::atomic<uint64_t> m_accepted{0};
    std::atomic<uint64_t> m_rejected{0};

    // Pending command queue (sorted by fire time).
    std::vector<NozzleCommand> m_pending;
    std::mutex m_pendingMtx;
    std::atomic<bool> m_schedulerRun{false};
    std::thread m_scheduler;

    FaultCallback m_fault;
};

}}

#endif // CGS_HAS_BECKHOFF_ADS
#endif
