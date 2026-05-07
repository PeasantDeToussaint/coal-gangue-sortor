#ifndef CGS_BEIDUADAPTER_H
#define CGS_BEIDUADAPTER_H

// Beifu (Beiду / 倍福) X-ray controller adapter.
//
// The Beifu controller is an additional ADS-connected device on the Beckhoff
// EtherCAT network. Its AMS NetID is separate from the main Beckhoff controller
// (the valve bar). It acts as a hardware interlock for the X-ray tube: the PC
// must write an enable byte over ADS before the tube can fire.
//
// From real machine config.xml:
//   NotUseBeifu="0"            — Beifu IS used on this machine
//   BeifuNetID="2.192.168.0.102.1.1"
//
// Enabled with: cmake -DCGS_HAS_BECKHOFF_ADS=ON
// (Uses the same AdsLib as ValveDriverEL2828.)
//
// Usage:
//   BeiduAdapter beifu;
//   beifu.open("2.192.168.0.102.1.1", routerIp);
//   beifu.enable(true);   // before xray->startup()
//   beifu.enable(false);  // on shutdown

#include <string>
#include <memory>

namespace cgs {
namespace hardware {

class BeiduAdapter {
public:
    BeiduAdapter();
    ~BeiduAdapter();

    // Open ADS connection to the Beifu controller.
    // netId: AMS Net ID string "n1.n2.n3.n4.n5.n6" (BeifuNetID from config).
    // routerIp: IP of the Beckhoff TwinCAT router (same machine running TwinCAT).
    // indexGroup / indexOffset: ADS output variable address on the Beifu device.
    //   Defaults match the observed config; confirm with 刘工 if beam doesn't enable.
    // indexOffset = 0x81000000 confirmed from BeckHoff.cpp source code:
    //   AdsSyncWriteReq(pAddr, 0x3040030, 0x81000000, 80, &Data[0])
    // The Beifu secondary controller uses 0x81000000 (different from the main
    // nozzle valve bar which uses 0x81000006).
    bool open(const std::string& netId,
              const std::string& routerIp,
              uint32_t indexGroup  = 0x3040030,
              uint32_t indexOffset = 0x81000000);
    void close();

    // Write enable (1) or disable (0) to the Beifu interlock output.
    bool enable(bool on);

    bool isOpen() const { return m_open; }

private:
    struct AdsConn;
    std::unique_ptr<AdsConn> m_conn;
    uint32_t m_indexGroup{0x3040030};
    uint32_t m_indexOffset{0x81000006};
    bool m_open{false};
};

}}

#endif
