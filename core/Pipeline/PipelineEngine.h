#ifndef CGS_PIPELINEENGINE_H
#define CGS_PIPELINEENGINE_H

// Pure C++ orchestrator: detector frame → classify → map nozzles → schedule fires.
// No Qt or hardware-vendor SDK dependency, so it can be unit-tested and reused
// by both the console app and the Qt GUI.

#include "../Classifier/Classifier.h"
#include "../Fusion/FusionPolicy.h"
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
    ClassifierConfig classifier;
    FusionConfig fusion;
    NozzleGeometry nozzles;
    TimingConfig timing;
};

struct PipelineStats {
    uint64_t framesProcessed = 0;
    uint64_t segmentsDetected = 0;
    uint64_t gangueRejected = 0;
    uint64_t commandsScheduled = 0;
    uint64_t commandsRejected = 0;
    double currentBeltSpeedMps = 0.0;
};

// Live snapshot of a single detector frame's classification result, used by
// the GUI to draw the waterfall plot and the active-nozzles overlay.
struct PipelineFrameSnapshot {
    uint64_t frameId = 0;
    int width = 0;
    std::vector<uint16_t> rawRow;          // copy of the X-ray row
    std::vector<ClassifiedSegment> segments;
    std::vector<int> firedNozzleIds;
};

class PipelineEngine {
public:
    PipelineEngine();
    ~PipelineEngine();

    void setConfig(const PipelineConfig& cfg);
    PipelineConfig config() const;

    // Wire the engine to concrete hardware. Engine does NOT take ownership;
    // caller manages lifetime.
    void attach(hardware::IDetector* detector,
                hardware::ICamera* camera,
                hardware::IValveDriver* valves,
                hardware::IPLC* plc,
                hardware::IXRaySource* xray);

    // Subscribe to live stream for UI / logging.
    using SnapshotCallback = std::function<void(const PipelineFrameSnapshot&)>;
    void setSnapshotCallback(SnapshotCallback cb);

    // Engine is event-driven; just call start()/stop() on the hardware
    // mocks/real impls. This method only flips an internal "armed" flag that
    // determines whether classification results actually trigger fires.
    void arm();
    void disarm();
    bool isArmed() const { return m_armed; }

    PipelineStats stats() const;
    void resetStats();

    // Hand a detector frame in directly (used by tests and replay tools).
    void processFrame(const hardware::DetectorFrame& f);

private:
    PipelineConfig m_cfg;
    mutable std::mutex m_cfgMtx;

    Classifier m_classifier;
    FusionPolicy m_fusion;
    NozzleMapper m_mapper;
    TimingCalculator m_timing;

    hardware::IDetector* m_detector = nullptr;
    hardware::ICamera* m_camera = nullptr;
    hardware::IValveDriver* m_valves = nullptr;
    hardware::IPLC* m_plc = nullptr;
    hardware::IXRaySource* m_xray = nullptr;

    std::atomic<bool> m_armed{false};

    mutable std::mutex m_statsMtx;
    PipelineStats m_stats;

    SnapshotCallback m_snapshotCb;
    std::mutex m_cbMtx;
};

}}

#endif
