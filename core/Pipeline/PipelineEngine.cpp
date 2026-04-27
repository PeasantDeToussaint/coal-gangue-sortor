#include "PipelineEngine.h"

#include <algorithm>

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
}

PipelineConfig PipelineEngine::config() const {
    std::lock_guard<std::mutex> lk(m_cfgMtx);
    return m_cfg;
}

void PipelineEngine::attach(hardware::IDetector* detector,
                            hardware::ICamera* camera,
                            hardware::IValveDriver* valves,
                            hardware::IPLC* plc,
                            hardware::IXRaySource* xray) {
    m_detector = detector;
    m_camera = camera;
    m_valves = valves;
    m_plc = plc;
    m_xray = xray;
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

void PipelineEngine::processFrame(const hardware::DetectorFrame& f) {
    PipelineFrameSnapshot snap;
    snap.frameId = f.frameId;
    snap.width = f.width;
    snap.rawRow.assign(f.data.begin(),
                       f.data.begin() + std::min<size_t>(f.data.size(), (size_t)f.width));

    snap.segments = m_classifier.classifyXRayRow(f.data.data(), f.width);

    double beltMps = 1.5; // default fallback if PLC absent
    if (m_plc) {
        const auto pst = m_plc->pollStatus();
        if (pst.detectionBeltSpeedMps > 0.0) beltMps = pst.detectionBeltSpeedMps;
    }

    {
        std::lock_guard<std::mutex> lk(m_statsMtx);
        ++m_stats.framesProcessed;
        m_stats.segmentsDetected += snap.segments.size();
        m_stats.currentBeltSpeedMps = beltMps;
    }

    std::vector<hardware::NozzleCommand> batch;
    for (const auto& seg : snap.segments) {
        const Material finalLabel = m_fusion.combine(seg.label, Material::Unknown);
        if (finalLabel != Material::Gangue) continue;

        {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            ++m_stats.gangueRejected;
        }

        const auto noz = m_mapper.nozzlesForRange(seg.startPx, seg.endPx, f.width);
        const double delayMs = m_timing.computeFireDelayMs(beltMps);
        const double burstMs = m_timing.computeBurstDurationMs(20.0, beltMps);
        for (int n : noz) {
            hardware::NozzleCommand c;
            c.nozzleId = n;
            c.fireAtNs = TimingCalculator::fireAtNs(f.timestampNs, std::max(0.0, delayMs));
            c.durationMs = (uint32_t)std::max(20.0, burstMs);
            batch.push_back(c);
            snap.firedNozzleIds.push_back(n);
        }
    }

    if (m_armed && m_valves && !batch.empty()) {
        if (m_valves->schedule(batch)) {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            m_stats.commandsScheduled += batch.size();
        } else {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            m_stats.commandsRejected += batch.size();
        }
    }

    SnapshotCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_cbMtx);
        cb = m_snapshotCb;
    }
    if (cb) cb(snap);
}

}}
