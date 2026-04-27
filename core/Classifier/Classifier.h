#ifndef CGS_CLASSIFIER_H
#define CGS_CLASSIFIER_H

#include <cstdint>
#include <vector>

namespace cgs {
namespace core {

enum class Material : uint8_t {
    Empty   = 0,    // belt is empty
    Coal    = 1,    // pass-through
    Gangue  = 2,    // reject
    Unknown = 3     // out-of-range or noisy
};

struct ClassifierConfig {
    // X-ray transmission thresholds (16-bit pixel values, lower = denser).
    uint16_t xrayEmptyMin = 50000;       // belt-empty pixel >= this
    uint16_t xrayCoalMin  = 25000;       // coal range: [coalMin, emptyMin)
    uint16_t xrayGangueMax = 25000;      // gangue: < gangueMax

    // Camera grayscale thresholds (8-bit). Lower = darker, gangue tends darker.
    uint8_t cameraEmptyMin = 180;
    uint8_t cameraGangueMax = 80;

    // Object detection: minimum contiguous pixels to consider a real object.
    int minObjectWidthPx = 5;
};

struct ClassifiedSegment {
    int startPx = 0;
    int endPx = 0;       // exclusive
    Material label = Material::Empty;
    double confidence = 0.0;   // 0.0 .. 1.0
};

class Classifier {
public:
    explicit Classifier(ClassifierConfig cfg = {}) : m_cfg(cfg) {}

    void setConfig(const ClassifierConfig& cfg) { m_cfg = cfg; }
    const ClassifierConfig& config() const { return m_cfg; }

    // Classifies a single X-ray row of 16-bit pixels.
    // Returns segments of contiguous same-label pixels (no segments < minObjectWidthPx).
    std::vector<ClassifiedSegment> classifyXRayRow(const uint16_t* row, int width) const;

    // Classifies an 8-bit grayscale ROI by reducing to a column-wise mean and
    // running the same segmentation logic.
    std::vector<ClassifiedSegment> classifyCameraRoi(const uint8_t* data,
                                                     int width, int height,
                                                     int rowStride) const;

private:
    Material labelXRay(uint16_t pixel) const;
    Material labelCamera(uint8_t pixel) const;
    std::vector<ClassifiedSegment> segment(const std::vector<Material>& perPixel) const;

    ClassifierConfig m_cfg;
};

}}

#endif
