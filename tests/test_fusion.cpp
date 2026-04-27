#include "test_harness.h"

#include "../core/Fusion/FusionPolicy.h"

using cgs::core::FusionPolicy;
using cgs::core::FusionConfig;
using cgs::core::FusionMode;
using cgs::core::Material;

TEST_CASE("fusion_xray_only") {
    FusionConfig c; c.mode = FusionMode::XRayOnly;
    FusionPolicy p(c);
    EXPECT_EQ(p.combine(Material::Coal, Material::Gangue), Material::Coal);
    EXPECT_EQ(p.combine(Material::Gangue, Material::Coal), Material::Gangue);
}

TEST_CASE("fusion_camera_only") {
    FusionConfig c; c.mode = FusionMode::CameraOnly;
    FusionPolicy p(c);
    EXPECT_EQ(p.combine(Material::Coal, Material::Gangue), Material::Gangue);
}

TEST_CASE("fusion_and_reject_conservative") {
    FusionConfig c; c.mode = FusionMode::AndReject;
    FusionPolicy p(c);
    EXPECT_EQ(p.combine(Material::Gangue, Material::Gangue), Material::Gangue);
    EXPECT_EQ(p.combine(Material::Gangue, Material::Coal), Material::Coal);
    EXPECT_EQ(p.combine(Material::Coal, Material::Gangue), Material::Coal);
    EXPECT_EQ(p.combine(Material::Empty, Material::Empty), Material::Empty);
}

TEST_CASE("fusion_or_reject_aggressive") {
    FusionConfig c; c.mode = FusionMode::OrReject;
    FusionPolicy p(c);
    EXPECT_EQ(p.combine(Material::Gangue, Material::Coal), Material::Gangue);
    EXPECT_EQ(p.combine(Material::Coal, Material::Gangue), Material::Gangue);
    EXPECT_EQ(p.combine(Material::Coal, Material::Coal), Material::Coal);
}

TEST_CASE("fusion_xray_authoritative_uses_camera_for_unknown") {
    FusionConfig c; c.mode = FusionMode::XRayAuthoritative;
    FusionPolicy p(c);
    EXPECT_EQ(p.combine(Material::Coal, Material::Gangue), Material::Coal);
    EXPECT_EQ(p.combine(Material::Unknown, Material::Gangue), Material::Gangue);
    EXPECT_EQ(p.combine(Material::Unknown, Material::Coal), Material::Coal);
}
