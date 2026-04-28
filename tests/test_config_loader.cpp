#include "test_harness.h"

#include "../core/Config/ConfigLoader.h"
#include "../core/Fusion/FusionPolicy.h"

using cgs::core::FusionMode;
using cgs::core::HardwareConfig;
using cgs::core::PipelineConfig;
using cgs::core::loadConfigFromFile;
using cgs::core::parseFusionModeString;
using cgs::testing::EXPECT_EQ;
using cgs::testing::EXPECT_TRUE;

TEST_CASE("parseFusionModeString") {
    EXPECT_EQ(parseFusionModeString("XRayOnly"), FusionMode::XRayOnly);
    EXPECT_EQ(parseFusionModeString("cameraonly"), FusionMode::CameraOnly);
    EXPECT_EQ(parseFusionModeString(" AndReject "), FusionMode::AndReject);
    EXPECT_EQ(parseFusionModeString("OrReject"), FusionMode::OrReject);
    EXPECT_EQ(parseFusionModeString("XRayAuthoritative"), FusionMode::XRayAuthoritative);
    EXPECT_EQ(parseFusionModeString(""), FusionMode::XRayAuthoritative);
}

TEST_CASE("loadConfigFromFile_example") {
    PipelineConfig p{};
    HardwareConfig h{};
    const bool ok = loadConfigFromFile("../../config/config.example.xml", p, h);
    EXPECT_TRUE(ok);
    EXPECT_EQ(h.xrayType, "mock");
    EXPECT_EQ(h.detectorWidth, 1024);
    EXPECT_NEAR(p.nozzles.pixelsPerNozzle, 16.0, 1e-6);
    EXPECT_EQ(p.fusion.mode, FusionMode::XRayAuthoritative);
}
