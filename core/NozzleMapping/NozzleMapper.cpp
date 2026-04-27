#include "NozzleMapper.h"

#include <algorithm>
#include <cmath>

namespace cgs {
namespace core {

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

}}
