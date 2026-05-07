#ifndef CGS_NOZZLEMAPPER_H
#define CGS_NOZZLEMAPPER_H

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cgs {
namespace core {

// Machine orientation modes (para0_0_1)
enum class MachineOrientation {
    Horizontal = 0,   // 卧式 (standard belt sorter)
    Vertical   = 1    // 立式 (chute/vertical sorter, different DQ)
};

// Maps pixel columns on the detection-belt frame to physical nozzle IDs.
// The actual mapping is calibrated per machine (nozzle_mapping.csv).
struct NozzleGeometry {
    // Real machine: 136 nozzles (QNum=136), active pixel range QStart=11..QEnd=1066
    // on a 2180-pixel-wide detector (DNum=17 modules × 128 pixels each).
    int totalNozzles = 136;
    int firstNozzleId = 1;
    // pixelsPerNozzle = (QEnd - QStart) / totalNozzles = (1066-11)/136 ≈ 7.76
    // Configured per machine from geometry.pixelsPerNozzle in config.xml.
    double pixelsPerNozzle = 7.76;
    int beltLeftPaddingPx  = 11;    // QStart — pixels before first active nozzle column
    int beltRightPaddingPx = 0;     // set to (detectorWidth - QEnd) at startup

    // Machine orientation
    MachineOrientation orientation = MachineOrientation::Horizontal;

    // XCCR: polynomial mapping camera pixel column → detector pixel column.
    // Used when sensor mode is X-ray+Camera (para0_0_2=2).
    // Real config.xml has 5 coefficients ordered high-to-low (degree 4):
    //   f(x) = c[0]*x^4 + c[1]*x^3 + c[2]*x^2 + c[3]*x + c[4]
    // The array always has 6 slots; c[5] (x^5 term) is 0 when string has 5 values.
    // From real machine config.xml (Xianyuan 2026):
    //   XCCR="0.00000000868533,-0.000000060953391,0.00020454513172,1.20877861052329,-0.337909834130844"
    std::array<double, 6> xccrCoeff{0.0, 0.0, 0.0, 1.0, 0.0, 0.0};  // default: identity (linear)

    // Parse XCCR coefficient string — accepts 5 or 6 comma-separated doubles.
    // Coefficients are ordered HIGH-to-LOW power (c[0]=highest-order term).
    bool setXccrFromString(const std::string& s);
};

class NozzleMapper {
public:
    explicit NozzleMapper(NozzleGeometry geo = {}) : m_geo(geo) {}

    void setGeometry(const NozzleGeometry& g) { m_geo = g; }
    const NozzleGeometry& geometry() const { return m_geo; }

    // Returns the nozzle ID covering detector pixel column x, or 0 if outside active region.
    int nozzleForPixel(int x, int frameWidth) const;

    // Returns nozzle IDs covering the inclusive pixel range [x0, x1).
    std::vector<int> nozzlesForRange(int x0, int x1, int frameWidth) const;

    // Map a CAMERA pixel column to a DETECTOR pixel column using the XCCR polynomial.
    // Uses XCCR coefficients: c0 + c1*x + c2*x^2 + c3*x^3 + c4*x^4 + c5*x^5
    // (index 0 = highest-order coefficient matching original config.xml order)
    int cameraPixelToDetectorPixel(int camPx) const;

    // Returns nozzle IDs for a camera pixel range (applies XCCR then maps to nozzles).
    std::vector<int> nozzlesForCameraRange(int camX0, int camX1, int cameraWidth,
                                            int detectorWidth) const;

private:
    NozzleGeometry m_geo;
};

}}

#endif
