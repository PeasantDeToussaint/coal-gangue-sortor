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
    // Path is relative to the build/tests directory — adjust if test binary location differs.
    const bool ok = loadConfigFromFile("../../config/config.example.xml", p, h);
    EXPECT_TRUE(ok);
    // Production example config uses real hardware type, not mock.
    EXPECT_EQ(h.xrayType, "vj-serial");
    EXPECT_EQ(h.xrayKv, 200.0);
    // Detector: 2180 pixels wide (DNum=17 modules)
    EXPECT_EQ(h.detectorWidth, 2180);
    EXPECT_EQ(h.detectorLineCount, 1150);
    // Nozzle geometry: QNum=136, QStart=11, pixelsPerNozzle=(1066-11)/136≈7.757
    EXPECT_EQ(p.nozzles.totalNozzles, 136);
    EXPECT_EQ(p.nozzles.beltLeftPaddingPx, 11);
    EXPECT_NEAR(p.nozzles.pixelsPerNozzle, 7.757, 0.01);
    // DQ timing: sensorToNozzleMm=2498mm
    EXPECT_NEAR(p.timing.sensorToNozzleMm, 2498.0, 0.1);
    // Fusion: XRayOnly (single sensor, no camera fusion by default)
    EXPECT_EQ(p.fusion.mode, FusionMode::XRayOnly);
}
