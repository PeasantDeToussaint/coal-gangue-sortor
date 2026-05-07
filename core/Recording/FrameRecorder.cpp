#include "FrameRecorder.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace cgs {
namespace core {

namespace fs = std::filesystem;

FrameRecorder::FrameRecorder(const Config& cfg) : m_cfg(cfg) {}

// ---------------------------------------------------------------------------
// record — main entry point
// ---------------------------------------------------------------------------
void FrameRecorder::record(const PipelineFrameSnapshot& snap) {
    const bool hasDetections = !snap.detections.empty();
    if (!m_cfg.mustSave && !hasDetections) return;

    const std::string dir = dateDirPath();
    ensureDirExists(dir);

    // Build filename base: <dir>/<frameId>
    const std::string base = dir + "/" + std::to_string(snap.frameId);

    // 1) Write raw 16-bit TIFF if we have image data
    if (!snap.previewRow.empty() && snap.width > 0) {
        // For the TIFF we write the preview row repeated as a minimal 1-row image.
        // When the full 2D buffer is available it should be passed instead.
        writeTiff16(base + "_raw.tif",
                    snap.previewRow.data(),
                    snap.width, 1);
    }

    // 2) Write _IO_Mat.pgm — fired nozzle column map (ImageJ diagnostic; PGM P5 binary)
    if (!snap.firedNozzleIds.empty()) {
        writeIoMatPgm(base + "_IO_Mat.pgm",
                      snap.firedNozzleIds,
                      m_cfg.totalNozzles);
    }
}

// ---------------------------------------------------------------------------
// dateDirPath — "data/YYYY_MM_DD-" matching original naming convention
// ---------------------------------------------------------------------------
std::string FrameRecorder::dateDirPath() const {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info{};
#ifdef _WIN32
    localtime_s(&tm_info, &t);
#else
    localtime_r(&t, &tm_info);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y_%m_%d-", &tm_info);
    return m_cfg.recordPath + "/" + buf;
}

void FrameRecorder::ensureDirExists(const std::string& path) const {
    try { fs::create_directories(path); } catch (...) {}
}

// ---------------------------------------------------------------------------
// writeTiff16 — minimal uncompressed 16-bit grayscale TIFF.
// No external library; writes the smallest valid TIFF header + raw pixels.
// Produces files that ImageJ can open and analyse.
// ---------------------------------------------------------------------------
bool FrameRecorder::writeTiff16(const std::string& path,
                                 const uint16_t* data,
                                 int width, int height) const {
    if (!data || width <= 0 || height <= 0) return false;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    // TIFF header — little-endian, magic 0x002A.
    // IFD offset at byte 8.
    const uint32_t ifdOffset = 8;
    f.write("II", 2);                              // byte order: little-endian
    const uint16_t magic = 42; f.write((char*)&magic, 2);
    f.write((char*)&ifdOffset, 4);

    // IFD: 11 entries
    const uint16_t numEntries = 11;
    f.write((char*)&numEntries, 2);

    // Image data offset: after IFD entries + 4-byte next-IFD pointer.
    // IFD starts at 8, each entry = 12 bytes, + 4 for next-IFD.
    const uint32_t dataOffset = (uint32_t)(8 + 2 + numEntries * 12 + 4);

    auto writeTag = [&](uint16_t tag, uint16_t type, uint32_t count, uint32_t val) {
        f.write((char*)&tag,   2);
        f.write((char*)&type,  2);
        f.write((char*)&count, 4);
        f.write((char*)&val,   4);
    };

    const uint32_t bitsPerSample = 16;
    const uint32_t sampleFormat  = 1;  // unsigned integer
    const uint32_t stripBytes    = (uint32_t)(width * height * 2);

    writeTag(0x0100, 3, 1, (uint32_t)width);       // ImageWidth
    writeTag(0x0101, 3, 1, (uint32_t)height);       // ImageLength
    writeTag(0x0102, 3, 1, bitsPerSample);          // BitsPerSample
    writeTag(0x0103, 3, 1, 1);                       // Compression=None
    writeTag(0x0106, 3, 1, 1);                       // PhotometricInterpretation=BlackIsZero
    writeTag(0x0111, 4, 1, dataOffset);              // StripOffsets
    writeTag(0x0115, 3, 1, 1);                       // SamplesPerPixel
    writeTag(0x0116, 3, 1, (uint32_t)height);        // RowsPerStrip
    writeTag(0x0117, 4, 1, stripBytes);             // StripByteCounts
    writeTag(0x011C, 3, 1, 1);                       // PlanarConfiguration=Chunky
    writeTag(0x0153, 3, 1, sampleFormat);           // SampleFormat

    const uint32_t nextIfd = 0;
    f.write((char*)&nextIfd, 4);

    // Pixel data (16-bit little-endian, row-major)
    f.write((const char*)data, (std::streamsize)(width * height * 2));
    return f.good();
}

// ---------------------------------------------------------------------------
// writeIoMatPgm — writes a PGM (P5) image where fired nozzle columns are
// white (255) and others are black (0). ImageJ can open PGM natively.
// ---------------------------------------------------------------------------
bool FrameRecorder::writeIoMatPgm(const std::string& path,
                                   const std::vector<int>& firedNozzles,
                                   int totalNozzles) const {
    if (totalNozzles <= 0) return false;

    // Single-row image, width=totalNozzles. White=fired, black=off.
    std::vector<uint8_t> row(totalNozzles, 0);
    for (int nId : firedNozzles) {
        const int idx = nId - 1;  // 1-based to 0-based
        if (idx >= 0 && idx < totalNozzles) row[idx] = 255;
    }

    // Write PGM P5 (binary grayscale)
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P5\n" << totalNozzles << " 1\n255\n";
    f.write((const char*)row.data(), totalNozzles);
    return f.good();
}

}}
