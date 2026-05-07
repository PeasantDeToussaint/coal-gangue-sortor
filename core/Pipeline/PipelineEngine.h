#ifndef CGS_PIPELINEENGINE_H
#define CGS_PIPELINEENGINE_H

// Main orchestrator: detector scan line → frame accumulation → AI inference
// → nozzle mapping → timed valve fire.
//
// No Qt or hardware-vendor SDK dependency; can be unit-tested and reused by
// both the console app and the Qt GUI.
//
// Processing loop (per DetectorFrame callback — ~2 Hz at 1150 lines/frame):
//   1. Push scan line into FrameAccumulator
//   2. When LineNumber lines collected → run IInferenceEngine::infer()
//   3. For each gangue detection → NozzleMapper → TimingCalculator → schedule
//   4. Emit PipelineFrameSnapshot for GUI / recording

#include "../Classifier/Classifier.h"
#include "../Fusion/FusionPolicy.h"
#include "../Inference/FrameAccumulator.h"
#include "../Inference/IInferenceEngine.h"
#include "../NozzleMapping/NozzleMapper.h"
#include "../Timing/TimingCalculator.h"

#include "../../hardware/DetectorInterface/IDetector.h"
#include "../../hardware/CameraInterface/ICamera.h"
#include "../../hardware/ValveDriverInterface/IValveDriver.h"
#include "../../hardware/PLCInterface/IPLC.h"
#include "../../hardware/XRayInterface/IXRaySource.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace cgs {
namespace core {

struct PipelineConfig {
    ClassifierConfig  classifier;   // fallback if no inference engine loaded
    FusionConfig      fusion;
    NozzleGeometry    nozzles;
    TimingConfig      timing;
    InferenceConfig   inference;
    int               lineCount{1150};   // LineNumber: scan lines per 2D frame
    double            defaultBeltSpeedMps{2.035};  // used when PLC absent
};

struct PipelineStats {
    uint64_t framesProcessed    = 0;
    uint64_t segmentsDetected   = 0;
    uint64_t gangueRejected     = 0;
    uint64_t commandsScheduled  = 0;
    uint64_t commandsRejected   = 0;
    double   currentBeltSpeedMps = 0.0;
    double   lastInferenceMs     = 0.0;  // wall-clock inference time for last frame
};

// Live snapshot of one complete 2D frame, forwarded to the GUI.
struct PipelineFrameSnapshot {
    uint64_t frameId   = 0;
    int      width     = 0;
    int      height    = 0;
    // Downsampled preview row (middle row of the 2D frame, for waterfall view)
    std::vector<uint16_t> previewRow;
    std::vector<InferenceResult::Detection> detections;
    std::vector<int>      firedNozzleIds;
    double   inferenceMs = 0.0;
};

class PipelineEngine {
public:
    PipelineEngine();
    ~PipelineEngine();

    void setConfig(const PipelineConfig& cfg);
    PipelineConfig config() const;

    // Attach hardware. Engine does NOT take ownership; caller manages lifetime.
    void attach(hardware::IDetector*    detector,
                hardware::ICamera*      camera,
                hardware::IValveDriver* valves,
                hardware::IPLC*         plc,
                hardware::IXRaySource*  xray);

    // Subscribe to per-frame snapshots (called on the detector callback thread).
    using SnapshotCallback = std::function<void(const PipelineFrameSnapshot&)>;
    void setSnapshotCallback(SnapshotCallback cb);

    // Arm/disarm: when armed, detections trigger valve fires.
    void arm();
    void disarm();
    bool isArmed() const { return m_armed; }

    PipelineStats stats() const;
    void resetStats();

    // Direct frame injection (tests, replay tool, TestOffline).
    void processFrame(const hardware::DetectorFrame& f);

    // Replace or clear the inference engine at runtime (e.g. model hot-swap).
    void setInferenceEngine(std::unique_ptr<IInferenceEngine> engine);
    IInferenceEngine* inferenceEngine() const { return m_inferenceEngine.get(); }

private:
    void processCompleteFrame(uint64_t frameId, uint64_t timestampNs);

    PipelineConfig    m_cfg;
    mutable std::mutex m_cfgMtx;

    FrameAccumulator  m_accumulator;
    std::unique_ptr<IInferenceEngine> m_inferenceEngine;
    Classifier        m_classifier;   // fallback
    FusionPolicy      m_fusion;
    NozzleMapper      m_mapper;
    TimingCalculator  m_timing;

    hardware::IDetector*    m_detector = nullptr;
    hardware::ICamera*      m_camera   = nullptr;
    hardware::IValveDriver* m_valves   = nullptr;
    hardware::IPLC*         m_plc      = nullptr;
    hardware::IXRaySource*  m_xray     = nullptr;

    std::atomic<bool>     m_armed{false};

    mutable std::mutex    m_statsMtx;
    PipelineStats         m_stats;

    SnapshotCallback      m_snapshotCb;
    std::mutex            m_cbMtx;
};

}}

#endif
