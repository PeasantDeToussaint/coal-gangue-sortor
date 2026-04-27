#include "FusionPolicy.h"

namespace cgs {
namespace core {

Material FusionPolicy::combine(Material xray, Material camera) const {
    switch (m_cfg.mode) {
        case FusionMode::XRayOnly:
            return xray;
        case FusionMode::CameraOnly:
            return camera;
        case FusionMode::AndReject:
            if (xray == Material::Gangue && camera == Material::Gangue) return Material::Gangue;
            if (xray == Material::Empty && camera == Material::Empty)   return Material::Empty;
            return Material::Coal;
        case FusionMode::OrReject:
            if (xray == Material::Gangue || camera == Material::Gangue) return Material::Gangue;
            if (xray == Material::Empty && camera == Material::Empty)   return Material::Empty;
            return Material::Coal;
        case FusionMode::XRayAuthoritative:
        default:
            if (xray != Material::Unknown) return xray;
            return camera;
    }
}

}}
