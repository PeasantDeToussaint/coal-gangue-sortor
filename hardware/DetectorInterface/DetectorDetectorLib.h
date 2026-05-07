#pragma once
#ifndef CGS_DETECTORDETECTORLIB_H
#define CGS_DETECTORDETECTORLIB_H

// DetectorDetectorLib — adapter wrapping DetectorLib.dll (袁工's Aurora wrapper).
//
// DetectorLib.dll is a Qt library: it emits Qt signals when scan-line data
// arrives. This adapter is therefore itself a QObject so it can connect to
// those signals and forward them to our IDetector callback.
//
// Enabled with: cmake -DCGS_HAS_DETECTORLIB=ON
//
// The DetectorLibInterface header is reconstructed from the .lib symbol table
// (DetectorLibInterface.h in third_party/detectorlib/).

#include "IDetector.h"

#ifdef CGS_HAS_DETECTORLIB

#include <QObject>
#include <QString>
#include <atomic>
#include <mutex>
#include <string>

class DetectorLibInterface;

namespace cgs {
namespace hardware {

class DetectorDetectorLib : public QObject, public IDetector {
    Q_OBJECT
public:
    DetectorDetectorLib();
    ~DetectorDetectorLib() override;

    bool open(const DetectorConfig& cfg) override;
    void close() override;
    bool start() override;
    bool stop() override;
    bool isRunning() const override { return m_running; }
    void setFrameCallback(FrameCallback cb) override;

    bool calibrateDark()   override;
    bool calibrateBright() override;

private slots:
    // Connected to DetectorLibInterface::signalAcqDataArray
    void onAcqDataArray(unsigned short* data, int width, int height, QString info);

private:
    DetectorLibInterface* m_det{nullptr};
    FrameCallback         m_cb;
    mutable std::mutex    m_cbMtx;
    std::atomic<bool>     m_running{false};
    uint64_t              m_frameId{0};
    int                   m_width{2180};
    int                   m_integrationUs{540};   // intTime — set via slotDet_setParams
    std::string           m_dataPath{"D:/data/"}; // data recording path for DetectorLib
};

}} // cgs::hardware

#endif // CGS_HAS_DETECTORLIB
#endif // CGS_DETECTORDETECTORLIB_H
