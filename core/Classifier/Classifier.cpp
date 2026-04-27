#include "Classifier.h"

#include <numeric>

namespace cgs {
namespace core {

Material Classifier::labelXRay(uint16_t p) const {
    if (p >= m_cfg.xrayEmptyMin) return Material::Empty;
    if (p >= m_cfg.xrayCoalMin)  return Material::Coal;
    if (p < m_cfg.xrayGangueMax) return Material::Gangue;
    return Material::Unknown;
}

Material Classifier::labelCamera(uint8_t p) const {
    if (p >= m_cfg.cameraEmptyMin) return Material::Empty;
    if (p <= m_cfg.cameraGangueMax) return Material::Gangue;
    return Material::Coal;
}

std::vector<ClassifiedSegment>
Classifier::classifyXRayRow(const uint16_t* row, int width) const {
    if (!row || width <= 0) return {};
    std::vector<Material> labels(width);
    for (int i = 0; i < width; ++i) labels[i] = labelXRay(row[i]);
    return segment(labels);
}

std::vector<ClassifiedSegment>
Classifier::classifyCameraRoi(const uint8_t* data,
                              int width, int height, int rowStride) const {
    if (!data || width <= 0 || height <= 0 || rowStride < width) return {};
    std::vector<Material> labels(width);
    for (int x = 0; x < width; ++x) {
        uint64_t sum = 0;
        for (int y = 0; y < height; ++y) {
            sum += data[(size_t)y * rowStride + x];
        }
        const uint8_t mean = (uint8_t)(sum / (uint64_t)height);
        labels[x] = labelCamera(mean);
    }
    return segment(labels);
}

std::vector<ClassifiedSegment>
Classifier::segment(const std::vector<Material>& perPixelIn) const {
    std::vector<ClassifiedSegment> out;
    if (perPixelIn.empty()) return out;

    // Pre-pass: collapse short non-Empty runs into Empty so they merge into
    // surrounding background. This treats them as sensor noise / belt seams.
    std::vector<Material> perPixel = perPixelIn;
    const int n = (int)perPixel.size();
    int runStart = 0;
    for (int i = 1; i <= n; ++i) {
        if (i == n || perPixel[i] != perPixel[runStart]) {
            const int width = i - runStart;
            if (perPixel[runStart] != Material::Empty && width < m_cfg.minObjectWidthPx) {
                for (int j = runStart; j < i; ++j) perPixel[j] = Material::Empty;
            }
            runStart = i;
        }
    }

    int start = 0;
    Material cur = perPixel[0];
    for (int i = 1; i <= n; ++i) {
        const bool boundary = (i == n) || (perPixel[i] != cur);
        if (boundary) {
            const int width = i - start;
            ClassifiedSegment seg;
            seg.startPx = start;
            seg.endPx = i;
            seg.label = cur;
            seg.confidence = (cur == Material::Unknown) ? 0.3 : std::min(1.0, width / 20.0);
            out.push_back(seg);
            if (i < n) {
                start = i;
                cur = perPixel[i];
            }
        }
    }
    return out;
}

}}
