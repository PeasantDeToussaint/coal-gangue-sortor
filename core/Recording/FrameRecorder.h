#ifndef CGS_FRAMERECORDER_H
#define CGS_FRAMERECORDER_H

// Records 2D detector frames to date-based directories.
//
// Storage layout matches original Gangue.exe data/ tree:
//   data/YYYY_MM_DD-/<frameId>_raw.tif          — 16-bit grayscale TIFF
//   data/YYYY_MM_DD-/<frameId>_IO_Mat.pgm        — PGM P5 nozzle fire map
//                                                    (white columns = fired nozzles)
//
// The _IO_Mat.pgm diagnostic matches the original system's ImageJ workflow
// produced in D:\data\, used with ImageJ contrast adjustment to verify
// Beckhoff valve commands are being sent correctly.
//
// MustSave behaviour:
//   false (default) — only save when detections > 0 (saves disk space)
//   true            — save every frame (for data collection / debugging)
//
// TIFF writing does NOT require an external library; we write minimal
// uncompressed 16-bit TIFF headers by hand to avoid dependencies.

#include "../Pipeline/PipelineEngine.h"

#include <cstdint>
#include <string>

namespace cgs {
namespace core {

class FrameRecorder {
public:
    struct Config {
        std::string recordPath{"data"};   // parent dir; date subdir created daily
        bool        mustSave{false};      // false = save on detection only
        int         totalNozzles{136};    // for _IO_Mat.pgm width
    };

    explicit FrameRecorder(const Config& cfg = {});

    void setConfig(const Config& cfg) { m_cfg = cfg; }
    const Config& config() const { return m_cfg; }

    // Process one pipeline snapshot. Saves files if appropriate.
    // Thread-safe (uses internal mutex).
    void record(const PipelineFrameSnapshot& snap);

private:
    std::string dateDirPath() const;
    void ensureDirExists(const std::string& path) const;

    // Write uncompressed 16-bit grayscale TIFF (no external lib required).
    bool writeTiff16(const std::string& path,
                     const uint16_t* data,
                     int width, int height) const;

    // Write PGM P5 (binary grayscale): fired nozzle columns as white stripes.
    bool writeIoMatPgm(const std::string& path,
                       const std::vector<int>& firedNozzles,
                       int totalNozzles) const;

    Config m_cfg;
};

}}

#endif
