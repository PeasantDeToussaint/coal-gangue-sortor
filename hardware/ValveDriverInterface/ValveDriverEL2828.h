#ifndef CGS_VALVEDRIVEREL2828_H
#define CGS_VALVEDRIVEREL2828_H

// ValveDriver via Beckhoff EL2828 digital outputs over EtherCAT/ADS.
//
// Uses a SINGLE AdsSyncWriteReq call per ejection cycle to write all N channels
// as a contiguous boolean array to a fixed index-group/offset address — matching
// the original Gangue.exe / BeckHoff.cpp approach exactly:
//
//   AdsSyncWriteReq(pAddr, 0x3040030, 0x81000006, channelCount, &boolArray[0]);
//
// This is far faster than per-channel variable-name lookups (one ADS round-trip
// per channel) and is required to meet the <5ms dispatch latency needed to hit
// the 136-nozzle bar before material passes.

#include "IValveDriver.h"

#ifdef CGS_HAS_BECKHOFF_ADS

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct AmsAddr;

namespace cgs {
namespace hardware {

class ValveDriverEL2828 : public IValveDriver {
public:
    ValveDriverEL2828();
    ~ValveDriverEL2828() override;

    // Endpoint format (parsed at open()):
    //   ads://<routerIp>:<netId>:<port>?ig=<indexGroup>&io=<indexOffset>&ch=<channelCount>
    // Example:
    //   ads://192.168.1.10:2.192.168.0.102.1.1:851?ig=0x3040030&io=0x81000006&ch=136
    //
    // Defaults: ig=0x3040030, io=0x81000006, ch=136, port=851.
    bool open(const std::string& endpoint) override;
    void close() override;

    bool arm() override;
    bool disarm() override;
    bool schedule(const std::vector<NozzleCommand>& batch) override;
    bool selfTest(uint32_t pulseMs) override;
    ValveDriverStatus pollStatus() override;
    void setFaultCallback(FaultCallback cb) override;

    // Override channel count after construction (before open).
    void setChannelCount(int n) { m_channelCount = n; }

private:
    void schedulerLoop();

    // Writes the entire output array in ONE ADS call.
    // outputStates must have exactly m_channelCount elements.
    bool bulkWrite(const std::vector<uint8_t>& outputStates);

    // Parses the endpoint URI; returns false on malformed input.
    bool parseEndpoint(const std::string& s,
                       std::string& ip,
                       std::string& netIdStr,
                       uint16_t& port,
                       uint32_t& indexGroup,
                       uint32_t& indexOffset,
                       int& channelCount);

    // ADS connection state (raw handle to avoid pulling AdsLib headers into .h)
    struct AdsConn;
    std::unique_ptr<AdsConn> m_conn;

    uint32_t m_indexGroup{0x3040030};
    uint32_t m_indexOffset{0x81000006};
    int      m_channelCount{136};

    std::atomic<bool> m_open{false};
    std::atomic<bool> m_armed{false};
    std::atomic<uint64_t> m_accepted{0};
    std::atomic<uint64_t> m_rejected{0};

    std::vector<NozzleCommand> m_pending;
    std::mutex m_pendingMtx;
    std::atomic<bool> m_schedulerRun{false};
    std::thread m_scheduler;

    FaultCallback m_fault;
};

}}

#endif // CGS_HAS_BECKHOFF_ADS
#endif
