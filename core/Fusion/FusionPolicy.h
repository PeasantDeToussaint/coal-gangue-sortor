#ifndef CGS_FUSIONPOLICY_H
#define CGS_FUSIONPOLICY_H

#include "../Classifier/Classifier.h"

namespace cgs {
namespace core {

enum class FusionMode {
    XRayOnly,        // ignore camera (works without camera hardware)
    CameraOnly,      // ignore X-ray
    AndReject,       // reject only if BOTH say gangue (conservative)
    OrReject,        // reject if EITHER says gangue (aggressive)
    XRayAuthoritative // X-ray decides; camera resolves Unknown only
};

struct FusionConfig {
    FusionMode mode = FusionMode::XRayAuthoritative;
};

class FusionPolicy {
public:
    explicit FusionPolicy(FusionConfig cfg = {}) : m_cfg(cfg) {}

    void setConfig(const FusionConfig& cfg) { m_cfg = cfg; }
    FusionMode mode() const { return m_cfg.mode; }

    // Combine two single-pixel labels. Used after column-wise downsampling so
    // both inputs are aligned to the same belt pixel/nozzle column.
    Material combine(Material xray, Material camera) const;

private:
    FusionConfig m_cfg;
};

}}

#endif
