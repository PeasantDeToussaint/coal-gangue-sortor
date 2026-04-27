#include "test_harness.h"

#include "../core/Classifier/Classifier.h"

#include <vector>

using cgs::core::Classifier;
using cgs::core::ClassifierConfig;
using cgs::core::Material;

TEST_CASE("classifier_xray_empty_belt") {
    Classifier c;
    std::vector<uint16_t> row(1024, 60000);
    auto segs = c.classifyXRayRow(row.data(), (int)row.size());
    EXPECT_EQ(segs.size(), 1u);
    EXPECT_EQ(segs[0].label, Material::Empty);
    EXPECT_EQ(segs[0].startPx, 0);
    EXPECT_EQ(segs[0].endPx, 1024);
}

TEST_CASE("classifier_xray_coal_block") {
    Classifier c;
    std::vector<uint16_t> row(1024, 60000);
    for (int i = 100; i < 200; ++i) row[i] = 30000; // coal range
    auto segs = c.classifyXRayRow(row.data(), (int)row.size());
    EXPECT_EQ(segs.size(), 3u);
    EXPECT_EQ(segs[0].label, Material::Empty);
    EXPECT_EQ(segs[1].label, Material::Coal);
    EXPECT_EQ(segs[1].startPx, 100);
    EXPECT_EQ(segs[1].endPx, 200);
    EXPECT_EQ(segs[2].label, Material::Empty);
}

TEST_CASE("classifier_xray_gangue_block") {
    Classifier c;
    std::vector<uint16_t> row(1024, 60000);
    for (int i = 500; i < 540; ++i) row[i] = 15000; // gangue
    auto segs = c.classifyXRayRow(row.data(), (int)row.size());
    bool foundGangue = false;
    for (const auto& s : segs) {
        if (s.label == Material::Gangue && s.startPx == 500 && s.endPx == 540) {
            foundGangue = true;
        }
    }
    EXPECT_TRUE(foundGangue);
}

TEST_CASE("classifier_xray_min_object_filter") {
    ClassifierConfig cfg;
    cfg.minObjectWidthPx = 10;
    Classifier c(cfg);
    std::vector<uint16_t> row(1024, 60000);
    for (int i = 100; i < 103; ++i) row[i] = 15000; // gangue, but only 3 px
    auto segs = c.classifyXRayRow(row.data(), (int)row.size());
    // 3-pixel gangue should be dropped, so we get a single Empty segment.
    EXPECT_EQ(segs.size(), 1u);
    EXPECT_EQ(segs[0].label, Material::Empty);
}

TEST_CASE("classifier_camera_dark_object") {
    Classifier c;
    const int W = 256, H = 16;
    std::vector<uint8_t> img((size_t)W * H, 220);
    for (int y = 0; y < H; ++y) {
        for (int x = 50; x < 100; ++x) img[(size_t)y * W + x] = 30; // gangue dark
    }
    auto segs = c.classifyCameraRoi(img.data(), W, H, W);
    bool foundGangue = false;
    for (const auto& s : segs) {
        if (s.label == Material::Gangue) foundGangue = true;
    }
    EXPECT_TRUE(foundGangue);
}
