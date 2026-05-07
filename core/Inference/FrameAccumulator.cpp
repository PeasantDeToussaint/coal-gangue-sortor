#include "FrameAccumulator.h"

#include <algorithm>
#include <cstring>

namespace cgs {
namespace core {

FrameAccumulator::FrameAccumulator(int targetLines)
    : m_targetLines(targetLines) {}

void FrameAccumulator::setTargetLines(int n) {
    m_targetLines = n;
    reset();
}

bool FrameAccumulator::push(const hardware::DetectorFrame& frame) {
    if (frame.data.empty() || frame.width <= 0) return false;

    // First line of a new frame — initialise buffer.
    if (m_linesCollected == 0) {
        if (m_width != frame.width) {
            m_width = frame.width;
            m_buf.assign((size_t)m_width * (size_t)m_targetLines, 0);
        }
        m_frameStartNs = frame.timestampNs;
        m_frameId      = frame.frameId;
    }

    if (m_width != frame.width) {
        // Width mismatch mid-frame (detector reconfigured) — restart.
        resetWidth(frame.width);
        m_frameStartNs = frame.timestampNs;
        m_frameId      = frame.frameId;
    }

    if (m_linesCollected >= m_targetLines) return false;  // full, caller must call reset()

    // Copy scan line into buffer (handle frames with height > 1 by taking first row).
    const size_t dstOffset = (size_t)m_linesCollected * (size_t)m_width;
    const size_t srcPixels = std::min<size_t>(frame.data.size(), (size_t)m_width);
    std::memcpy(m_buf.data() + dstOffset,
                frame.data.data(),
                srcPixels * sizeof(uint16_t));

    ++m_linesCollected;
    return (m_linesCollected >= m_targetLines);
}

void FrameAccumulator::reset() {
    m_linesCollected = 0;
    m_frameStartNs   = 0;
    m_frameId        = 0;
    // Buffer remains allocated at current size for next frame.
}

void FrameAccumulator::resetWidth(int newWidth) {
    m_width = newWidth;
    m_buf.assign((size_t)m_width * (size_t)m_targetLines, 0);
    reset();
}

}}
