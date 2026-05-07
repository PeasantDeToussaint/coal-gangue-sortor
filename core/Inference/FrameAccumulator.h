#ifndef CGS_FRAMEACCUMULATOR_H
#define CGS_FRAMEACCUMULATOR_H

// Buffers individual detector scan lines until a complete 2D frame is ready
// for AI inference.
//
// The DT Aurora detector delivers one LINE at a time (height=1 DetectorFrame).
// The AI model (YOLOv8 segmentation) needs a 2D image of LineNumber rows.
// This class accumulates lines and signals when a full frame is ready.
//
// Original config values: LineNumber=1150, intTime=540µs, Speed=2035mm/s.
//
// Thread-safety: push() is called from the detector callback thread;
// getImage() / reset() are called from the pipeline thread.
// The class itself is NOT thread-safe — caller must serialise access
// (the pipeline already runs on a single callback thread per detector).

#include "../../hardware/DetectorInterface/IDetector.h"

#include <cstdint>
#include <vector>

namespace cgs {
namespace core {

class FrameAccumulator {
public:
    // targetLines: LineNumber from config (default 1150).
    explicit FrameAccumulator(int targetLines = 1150);

    void setTargetLines(int n);
    int  targetLines() const { return m_targetLines; }

    // Push one scan line from a DetectorFrame.
    // Returns true when the frame buffer is now complete (exactly targetLines received).
    // Caller should then call getImage() and process, then call reset().
    bool push(const hardware::DetectorFrame& frame);

    // Access the accumulated image (width × height pixels, row-major, 16-bit).
    // Valid only when the previous push() returned true.
    const uint16_t* getImage()  const { return m_buf.data(); }
    int             width()     const { return m_width; }
    int             height()    const { return m_linesCollected; }
    int             linesCollected() const { return m_linesCollected; }

    // Timestamp of the FIRST line in the current frame (nanoseconds, steady clock).
    uint64_t frameStartNs() const { return m_frameStartNs; }
    uint64_t frameId()      const { return m_frameId; }

    // Reset for next frame. Preserves width and targetLines.
    void reset();

    // Discard accumulated lines and start fresh with a new width.
    void resetWidth(int newWidth);

private:
    int  m_targetLines;
    int  m_width{0};
    int  m_linesCollected{0};
    uint64_t m_frameStartNs{0};
    uint64_t m_frameId{0};

    std::vector<uint16_t> m_buf;   // m_width × m_targetLines, row-major
};

}}

#endif
