#ifndef CGS_NOZZLEMAPPER_H
#define CGS_NOZZLEMAPPER_H

#include <cstdint>
#include <utility>
#include <vector>

namespace cgs {
namespace core {

// Maps pixel columns on the detection-belt frame to physical nozzle IDs.
// The actual mapping is calibrated per machine (nozzle_mapping.csv).
struct NozzleGeometry {
    int totalNozzles = 64;       // 1..N inclusive
    int firstNozzleId = 1;
    double pixelsPerNozzle = 16.0; // e.g. 1024 pixels / 64 nozzles
    int beltLeftPaddingPx = 0;   // pixels at left of frame outside any nozzle column
    int beltRightPaddingPx = 0;
};

class NozzleMapper {
public:
    explicit NozzleMapper(NozzleGeometry geo = {}) : m_geo(geo) {}

    void setGeometry(const NozzleGeometry& g) { m_geo = g; }
    const NozzleGeometry& geometry() const { return m_geo; }

    // Returns the nozzle ID covering pixel column x, or 0 if outside the active region.
    int nozzleForPixel(int x, int frameWidth) const;

    // Returns nozzle IDs covering the inclusive pixel range [x0, x1).
    std::vector<int> nozzlesForRange(int x0, int x1, int frameWidth) const;

private:
    NozzleGeometry m_geo;
};

}}

#endif
