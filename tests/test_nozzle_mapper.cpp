#include "test_harness.h"

#include "../core/NozzleMapping/NozzleMapper.h"

using cgs::core::NozzleMapper;
using cgs::core::NozzleGeometry;

TEST_CASE("nozzle_mapper_default_64") {
    NozzleGeometry g;
    g.totalNozzles = 64;
    g.firstNozzleId = 1;
    g.pixelsPerNozzle = 16.0;
    NozzleMapper m(g);
    EXPECT_EQ(m.nozzleForPixel(0, 1024), 1);
    EXPECT_EQ(m.nozzleForPixel(15, 1024), 1);
    EXPECT_EQ(m.nozzleForPixel(16, 1024), 2);
    EXPECT_EQ(m.nozzleForPixel(1023, 1024), 64);
}

TEST_CASE("nozzle_mapper_padding") {
    NozzleGeometry g;
    g.totalNozzles = 64;
    g.firstNozzleId = 1;
    g.pixelsPerNozzle = 16.0;
    g.beltLeftPaddingPx = 10;
    g.beltRightPaddingPx = 10;
    NozzleMapper m(g);
    EXPECT_EQ(m.nozzleForPixel(5, 1024 + 20), 0);     // outside left pad
    EXPECT_EQ(m.nozzleForPixel(10, 1024 + 20), 1);    // start of active region
    EXPECT_EQ(m.nozzleForPixel(1043, 1024 + 20), 0);  // inside right pad
}

TEST_CASE("nozzle_mapper_range") {
    NozzleGeometry g;
    g.totalNozzles = 64;
    g.firstNozzleId = 1;
    g.pixelsPerNozzle = 16.0;
    NozzleMapper m(g);
    auto v = m.nozzlesForRange(0, 48, 1024);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

TEST_CASE("nozzle_mapper_out_of_range") {
    NozzleGeometry g;
    g.totalNozzles = 64;
    g.firstNozzleId = 1;
    g.pixelsPerNozzle = 16.0;
    NozzleMapper m(g);
    EXPECT_EQ(m.nozzleForPixel(-1, 1024), 0);
    EXPECT_EQ(m.nozzleForPixel(2000, 1024), 0);
}
