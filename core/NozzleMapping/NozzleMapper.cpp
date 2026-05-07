#include "NozzleMapper.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace cgs {
namespace core {

// ---------------------------------------------------------------------------
// NozzleGeometry::setXccrFromString
// Parses 5 or 6 comma-separated doubles (high-to-low polynomial order).
// Real machine config.xml has 5 values (degree-4 polynomial).
// The 6-element array is always used; missing 6th value is set to 0.
// ---------------------------------------------------------------------------
bool NozzleGeometry::setXccrFromString(const std::string& s) {
    xccrCoeff.fill(0.0);
    std::istringstream ss(s);
    std::string token;
    int i = 0;
    while (std::getline(ss, token, ',') && i < 6) {
        try { xccrCoeff[i++] = std::strtod(token.c_str(), nullptr); }
        catch (...) { break; }
    }
    return i >= 5;  // accept 5 or 6 coefficients
}

int NozzleMapper::nozzleForPixel(int x, int frameWidth) const {
    if (x < 0 || x >= frameWidth) return 0;
    const int activeStart = m_geo.beltLeftPaddingPx;
    const int activeEnd = frameWidth - m_geo.beltRightPaddingPx;
    if (x < activeStart || x >= activeEnd) return 0;
    if (m_geo.pixelsPerNozzle <= 0.0) return 0;
    const int idx = (int)std::floor((x - activeStart) / m_geo.pixelsPerNozzle);
    if (idx < 0 || idx >= m_geo.totalNozzles) return 0;
    return m_geo.firstNozzleId + idx;
}

std::vector<int> NozzleMapper::nozzlesForRange(int x0, int x1, int frameWidth) const {
    std::vector<int> out;
    if (x1 <= x0) return out;
    const int n0 = nozzleForPixel(x0, frameWidth);
    const int n1 = nozzleForPixel(std::max(x0, x1 - 1), frameWidth);
    if (n0 == 0 && n1 == 0) {
        // Try to find any covered nozzle in between.
        const int activeStart = m_geo.beltLeftPaddingPx;
        const int activeEnd = frameWidth - m_geo.beltRightPaddingPx;
        const int from = std::max(x0, activeStart);
        const int to = std::min(x1, activeEnd);
        if (from >= to) return out;
        for (int x = from; x < to; ++x) {
            const int n = nozzleForPixel(x, frameWidth);
            if (n != 0 && (out.empty() || out.back() != n)) out.push_back(n);
        }
        return out;
    }
    const int lo = std::min(n0 == 0 ? n1 : n0, n1 == 0 ? n0 : n1);
    const int hi = std::max(n0, n1);
    for (int n = lo; n <= hi; ++n) {
        if (n >= m_geo.firstNozzleId && n < m_geo.firstNozzleId + m_geo.totalNozzles) {
            out.push_back(n);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// cameraPixelToDetectorPixel — evaluate the XCCR polynomial.
// Coefficients are stored HIGH-to-LOW order, so:
//   xccrCoeff[0] = x^5 term (or x^4 for 5-coeff variant)
//   xccrCoeff[4] ≈ 1.208 = near-linear magnification (x^1 coefficient)
//   xccrCoeff[5] = constant term
// This matches the real machine's config.xml where XCCR lists 5 values
// f(x) = c0*x^4 + c1*x^3 + c2*x^2 + c3*x + c4.
// Use Horner's method for numerical stability.
// ---------------------------------------------------------------------------
int NozzleMapper::cameraPixelToDetectorPixel(int camPx) const {
    const auto& c = m_geo.xccrCoeff;
    const double x = (double)camPx;
    // Horner evaluation of c[0]*x^5 + c[1]*x^4 + c[2]*x^3 + c[3]*x^2 + c[4]*x + c[5]
    // For 5-coeff config (c[0]=0): = c[1]*x^4 + c[2]*x^3 + c[3]*x^2 + c[4]*x + c[5]
    const double result = ((((c[0]*x + c[1])*x + c[2])*x + c[3])*x + c[4])*x + c[5];
    return (int)std::round(result);
}

std::vector<int> NozzleMapper::nozzlesForCameraRange(int camX0, int camX1,
                                                      int cameraWidth,
                                                      int detectorWidth) const {
    const int detX0 = cameraPixelToDetectorPixel(camX0);
    const int detX1 = cameraPixelToDetectorPixel(std::max(camX0, camX1 - 1)) + 1;
    return nozzlesForRange(std::min(detX0, detX1),
                           std::max(detX0, detX1),
                           detectorWidth);
}

}}
