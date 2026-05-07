#include "PipelineEngine.h"

#include "../Logging/Logger.h"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace cgs {
namespace core {

PipelineEngine::PipelineEngine() = default;
PipelineEngine::~PipelineEngine() = default;

void PipelineEngine::setConfig(const PipelineConfig& cfg) {
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    m_cfg = cfg;
    m_classifier.setConfig(cfg.classifier);
    m_fusion.setConfig(cfg.fusion);
    m_mapper.setGeometry(cfg.nozzles);
    m_timing.setConfig(cfg.timing);
    m_accumulator.setTargetLines(cfg.lineCount > 0 ? cfg.lineCount : 1150);
}

PipelineConfig PipelineEngine::config() const {
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    return m_cfg;
}

void PipelineEngine::attach(hardware::IDetector*    detector,
                             hardware::ICamera*      camera,
                             hardware::IValveDriver* valves,
                             hardware::IPLC*         plc,
                             hardware::IXRaySource*  xray) {
    m_detector = detector;
    m_camera   = camera;
    m_valves   = valves;
    m_plc      = plc;
    m_xray     = xray;

    if (m_detector) {
        m_detector->setFrameCallback([this](const hardware::DetectorFrame& f){
            processFrame(f);
        });
    }
}

void PipelineEngine::setSnapshotCallback(SnapshotCallback cb) {
    std::lock_guard<std::mutex> lk(m_cbMtx);
    m_snapshotCb = std::move(cb);
}

void PipelineEngine::setInferenceEngine(std::unique_ptr<IInferenceEngine> engine) {
    m_inferenceEngine = std::move(engine);
    // If we now have an engine, reload its config.
    if (m_inferenceEngine && !m_inferenceEngine->isLoaded()) {
        std::lock_guard<std::mutex> lk(m_cfgMtx);
        m_inferenceEngine->load(m_cfg.inference);
    }
}

void PipelineEngine::arm()    { m_armed = true; }
void PipelineEngine::disarm() { m_armed = false; }

PipelineStats PipelineEngine::stats() const {
    std::lock_guard<std::mutex> lk(m_statsMtx);
    return m_stats;
}

void PipelineEngine::resetStats() {
    std::lock_guard<std::mutex> lk(m_statsMtx);
    m_stats = PipelineStats{};
}

// ---------------------------------------------------------------------------
// processFrame — called per detector scan line from hardware callback thread.
//
// Matches original Gangue.exe heartbeat pattern visible in gangue_sys.log:
//   "-----begin process"  → push scan line
//   "-----finish process" → complete 2D frame processed, commands dispatched
// ---------------------------------------------------------------------------
void PipelineEngine::processFrame(const hardware::DetectorFrame& f) {
    // Push one scan line; returns true when LineNumber lines are accumulated.
    const bool frameComplete = m_accumulator.push(f);
    if (!frameComplete) return;

    processCompleteFrame(m_accumulator.frameId(), m_accumulator.frameStartNs());
    m_accumulator.reset();
}

void PipelineEngine::processCompleteFrame(uint64_t frameId, uint64_t timestampNs) {
    const uint16_t* image  = m_accumulator.getImage();
    const int       width  = m_accumulator.width();
    const int       height = m_accumulator.height();

    std::ostringstream ts;
    ts << "frame=" << frameId << " t_ns=" << timestampNs;
    const std::string timestampStr = ts.str();
    CGS_LOG_DEBUG("-----begin process: \"{}\"", timestampStr);
    // "is not Busy" — emitted after acquiring the frame, matching original log pattern:
    //   -----begin process: "HH:MM:SS.mmm"
    //   -----is not Busy  : "HH:MM:SS.mmm"
    //   -----finish process: "HH:MM:SS.mmm"
    CGS_LOG_DEBUG("-----is not Busy  : \"{}\"", timestampStr);

    // -----------------------------------------------------------------------
    // 1. Get belt speed from PLC (S7-1500). Fall back to config default.
    // -----------------------------------------------------------------------
    double beltMps = m_cfg.defaultBeltSpeedMps;
    if (m_plc) {
        const auto pst = m_plc->pollStatus();
        if (pst.detectionBeltSpeedMps > 0.0) beltMps = pst.detectionBeltSpeedMps;
    }

    {
        std::lock_guard<std::mutex> lk(m_statsMtx);
        ++m_stats.framesProcessed;
        m_stats.currentBeltSpeedMps = beltMps;
    }

    // -----------------------------------------------------------------------
    // 2. Run inference (TRT / ONNX) or fall back to threshold classifier.
    // -----------------------------------------------------------------------
    InferenceResult inferResult;

    if (m_inferenceEngine && m_inferenceEngine->isLoaded()) {
        inferResult = m_inferenceEngine->infer(image, width, height);
        {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            m_stats.lastInferenceMs = inferResult.inferenceTimeMs;
        }
    } else {
        // Threshold fallback: classify the middle scan row.
        const int midRow = height / 2;
        const uint16_t* row = image + (size_t)midRow * (size_t)width;
        auto segments = m_classifier.classifyXRayRow(row, width);
        for (const auto& seg : segments) {
            if (seg.label != Material::Gangue) continue;
            InferenceResult::Detection d;
            d.startPx    = seg.startPx;
            d.endPx      = seg.endPx;
            d.classId    = 0;
            d.confidence = (float)seg.confidence;
            d.shouldEject = true;
            inferResult.detections.push_back(d);
        }
    }

    // -----------------------------------------------------------------------
    // 3. Map detections to nozzle IDs and schedule valve fires.
    // -----------------------------------------------------------------------
    PipelineFrameSnapshot snap;
    snap.frameId  = frameId;
    snap.width    = width;
    snap.height   = height;
    snap.inferenceMs = inferResult.inferenceTimeMs;
    snap.detections  = inferResult.detections;

    // Preview row for waterfall: middle of the frame.
    {
        const int midRow = height / 2;
        const uint16_t* row = image + (size_t)midRow * (size_t)width;
        snap.previewRow.assign(row, row + width);
    }

    std::vector<hardware::NozzleCommand> batch;

    for (const auto& det : inferResult.detections) {
        if (!det.shouldEject) continue;

        {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            ++m_stats.gangueRejected;
        }

        // Compute burst duration from object width on belt.
        const double segWidthPx = (double)(det.endPx - det.startPx);
        const double pxPerMm = 1.0 / m_cfg.nozzles.pixelsPerNozzle;  // approximate
        const double objLenMm = segWidthPx * (1.0 / pxPerMm);

        const double delayMs = m_timing.computeFireDelayMs(beltMps);
        const double burstMs = m_timing.computeBurstDurationMs(objLenMm, beltMps);

        const auto nozzles = m_mapper.nozzlesForRange(det.startPx, det.endPx, width);
        for (int nId : nozzles) {
            hardware::NozzleCommand c;
            c.nozzleId   = nId;
            c.fireAtNs   = TimingCalculator::fireAtNs(timestampNs, std::max(0.0, delayMs));
            c.durationMs = (uint32_t)std::max(20.0, burstMs);
            batch.push_back(c);
            snap.firedNozzleIds.push_back(nId);
        }
    }

    {
        std::lock_guard<std::mutex> lk(m_statsMtx);
        m_stats.segmentsDetected += inferResult.detections.size();
    }

    // -----------------------------------------------------------------------
    // 4. Dispatch valve commands (only when armed).
    // -----------------------------------------------------------------------
    if (m_armed && m_valves && !batch.empty()) {
        if (m_valves->schedule(batch)) {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            m_stats.commandsScheduled += batch.size();
        } else {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            m_stats.commandsRejected += batch.size();
        }
    }

    // -----------------------------------------------------------------------
    // 5. Emit snapshot to GUI / recording layer.
    // -----------------------------------------------------------------------
    SnapshotCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_snapshotCb;
    }
    if (cb) cb(snap);

    CGS_LOG_DEBUG("-----finish process: \"{}\"", timestampStr);
    CGS_LOG_DEBUG("-----finishfinish process: \"{}\"", timestampStr);
}

}}
