#pragma once
// BeckHoffLibInterface.h — RECONSTRUCTED from BeckHoffLib.lib symbol table.
//
// Original source: D:\0_work\0_HBJH\code\Gangue_home_2\bin\release\BeckHoffLib.pdb
// (confirmed from PDB path embedded in BeckHoffLib.dll)
//
// BeckHoffLib is 袁工's ADS wrapper around TcAdsDll.DLL. It reads indexGroup
// and indexOffset from ParseConfigXml::getDetectorParamsInfo() at runtime —
// the addresses are NOT hardcoded; they come from config.xml.
//
// API is intentionally minimal: just 4 public exports.
//   - getInstance()     → singleton
//   - Init()            → open ADS port, read config, establish route
//   - writeRequest()    → single-call bulk write (matches our ADS implementation)
//
// ADS calls used internally (from BeckHoffLib.dll imports):
//   AdsGetLocalAddress, AdsPortOpen, AdsSyncWriteReq
//
// Usage:
//   BeckHoffLib* bhl = BeckHoffLib::getInstance();
//   bhl->Init();
//   bool states[136] = {};
//   states[5] = true;
//   int err = bhl->writeRequest(0x3040030, states);  // err == 0 on success

#ifndef BECKHOFFLIBINTERFACE_H
#define BECKHOFFLIBINTERFACE_H

#include <QString>

class BeckHoffLib {
public:
    // Singleton — always use getInstance() to get the shared instance.
    static BeckHoffLib* getInstance();

    // Init: open ADS port and establish TwinCAT route.
    // Reads indexGroup and indexOffset from ParseConfigXml::getDetectorParamsInfo().
    // Must be called once before writeRequest().
    void Init();

    // Write a bool array to the EtherCAT output image in a single ADS call.
    //   indexGroup: ADS index group (e.g. 0x3040030 for nozzle bar)
    //   states:     pointer to bool array, one element per channel
    // Returns ADS error code (0 = success, non-zero = ADS error, logged as m_nErr).
    int writeRequest(unsigned long indexGroup, bool* states);

    BeckHoffLib(const BeckHoffLib&) = delete;
    BeckHoffLib& operator=(const BeckHoffLib&) = delete;

private:
    BeckHoffLib() = default;
};

#endif // BECKHOFFLIBINTERFACE_H
