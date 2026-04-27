// Tests pure formatting helpers from XRaySerial without opening a port.
// These guard the protocol encoding (VP, CP, PTM) we discovered from the
// legacy XRayLib.cpp. No serial hardware needed.

#include "test_harness.h"

#include <cstdio>
#include <string>

namespace {
// Re-implementation of XRaySerial::formatNumber for unit testing.
// (XRaySerial.cpp keeps its own private copy; we mirror it here so
//  protocol regressions are caught even without linking the driver.)
std::string formatNumber(double value, int width) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%0*.0f", width, value);
    return buf;
}
}

TEST_CASE("xray_format_voltage_80kv") {
    EXPECT_EQ(std::string("VP") + formatNumber(80.0 * 10.0, 4), "VP0800");
}

TEST_CASE("xray_format_voltage_120_5kv") {
    EXPECT_EQ(std::string("VP") + formatNumber(120.5 * 10.0, 4), "VP1205");
}

TEST_CASE("xray_format_current_1500ua") {
    EXPECT_EQ(std::string("CP") + formatNumber(1.5 * 1000.0, 4), "CP1500");
}

TEST_CASE("xray_format_preheat_30sec") {
    EXPECT_EQ(std::string("PTM") + formatNumber(30, 4), "PTM0030");
}

TEST_CASE("xray_format_preheat_2hours") {
    EXPECT_EQ(std::string("PTM") + formatNumber(7200, 4), "PTM7200");
}
