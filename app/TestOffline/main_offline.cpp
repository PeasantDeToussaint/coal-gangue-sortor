// TestOffline — batch offline detection test.
//
// Mirrors the original TestOffline.exe behaviour:
//   - Reads .tif files from an input directory (default: D:/0_test/ on Windows,
//     ./0_test on other platforms)
//   - Runs IInferenceEngine::infer() on each frame
//   - Saves annotated output alongside the input file
//   - Prints per-frame detection count and inference time
//
// Usage:
//   cgs_test_offline --input-dir D:/0_test --model best_Xray.trt
//   cgs_test_offline --input-dir ./0_test  --model best_seg.onnx
//
// The 16-bit TIFF reader is minimal (handles uncompressed 16-bit grayscale,
// the format written by DetectorAurora and FrameRecorder).

#include "../../core/Config/ConfigLoader.h"
#include "../../core/Inference/IInferenceEngine.h"
#include "../../core/Logging/Logger.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace cgs::core;

// ---------------------------------------------------------------------------
// Minimal 16-bit TIFF reader
// ---------------------------------------------------------------------------
struct TiffImage {
    std::vector<uint16_t> data;
    int width{0}, height{0};
};

static bool readTiff16(const std::string& path, TiffImage& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    // Read enough header to locate IFD
    uint8_t hdr[8]{};
    f.read((char*)hdr, 8);
    const bool le = (hdr[0] == 'I');
    auto u16 = [&](const uint8_t* p) -> uint16_t {
        return le ? (uint16_t)(p[0] | (p[1] << 8)) : (uint16_t)((p[0] << 8) | p[1]);
    };
    auto u32 = [&](const uint8_t* p) -> uint32_t {
        return le ? (p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24))
                  : ((p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3]);
    };

    uint32_t ifdOff = u32(hdr + 4);
    f.seekg(ifdOff);

    uint8_t ne[2]; f.read((char*)ne, 2);
    const uint16_t numEntries = u16(ne);

    int width = 0, height = 0;
    uint32_t dataOffset = 0, bitsPerSample = 8;

    for (uint16_t i = 0; i < numEntries; ++i) {
        uint8_t e[12]; f.read((char*)e, 12);
        const uint16_t tag = u16(e);
        const uint32_t val = u32(e + 8);
        switch (tag) {
            case 0x0100: width         = (int)val; break;
            case 0x0101: height        = (int)val; break;
            case 0x0102: bitsPerSample = val;       break;
            case 0x0111: dataOffset    = val;       break;
        }
    }

    if (width <= 0 || height <= 0 || dataOffset == 0) return false;
    if (bitsPerSample != 16) {
        std::cerr << "TestOffline: only 16-bit TIFF supported\n";
        return false;
    }

    f.seekg(dataOffset);
    out.width  = width;
    out.height = height;
    out.data.resize((size_t)width * height);
    f.read((char*)out.data.data(), (std::streamsize)(width * height * 2));

    // Fix endianness if big-endian TIFF on little-endian host
    if (!le) {
        for (auto& px : out.data) px = (uint16_t)((px >> 8) | (px << 8));
    }
    return f.good();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    Logger::init("data/test_offline.log");

#ifdef _WIN32
    std::string inputDir = "D:/0_test";
#else
    std::string inputDir = "./0_test";
#endif
    std::string modelPath;
    bool useTrt = false;

    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--input-dir") == 0 || std::strcmp(argv[i], "-i") == 0)
                && i + 1 < argc) {
            inputDir = argv[++i];
        } else if ((std::strcmp(argv[i], "--model") == 0 || std::strcmp(argv[i], "-m") == 0)
                && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (std::strcmp(argv[i], "--trt") == 0) {
            useTrt = true;
        }
    }

    // Build inference config
    InferenceConfig inferCfg;
    if (!modelPath.empty()) {
        const std::string ext = modelPath.size() > 4
            ? modelPath.substr(modelPath.size() - 4) : "";
        if (ext == ".trt") {
            inferCfg.trtPath  = modelPath;
            inferCfg.onnxPath = "";
        } else {
            inferCfg.onnxPath = modelPath;
            inferCfg.trtPath  = "";
        }
    }

    auto engine = createBestInferenceEngine(inferCfg);
    if (!engine) {
        std::cerr << "TestOffline: no inference engine available.\n"
                  << "  Build with -DCGS_HAS_ONNX=ON or -DCGS_HAS_TENSORRT=ON\n"
                  << "  and provide a model file with --model <path>\n";
        return 1;
    }
    std::cout << "Engine: " << engine->engineName() << "\n";

    if (!fs::exists(inputDir)) {
        std::cerr << "TestOffline: input directory not found: " << inputDir << "\n";
        return 1;
    }

    // Collect .tif files
    std::vector<fs::path> tifs;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        const auto ext = entry.path().extension().string();
        if (ext == ".tif" || ext == ".TIF" || ext == ".tiff" || ext == ".TIFF")
            tifs.push_back(entry.path());
    }
    std::sort(tifs.begin(), tifs.end());

    if (tifs.empty()) {
        std::cout << "TestOffline: no .tif files found in " << inputDir << "\n";
        return 0;
    }

    std::cout << "TestOffline: processing " << tifs.size() << " frames...\n\n";

    int totalDetections = 0;
    double totalMs = 0.0;

    for (size_t i = 0; i < tifs.size(); ++i) {
        TiffImage img;
        if (!readTiff16(tifs[i].string(), img)) {
            std::cerr << "  [" << (i+1) << "] FAILED to read: " << tifs[i] << "\n";
            continue;
        }

        const InferenceResult result = engine->infer(img.data.data(), img.width, img.height);
        totalDetections += (int)result.detections.size();
        totalMs         += result.inferenceTimeMs;

        int gangue = 0;
        for (const auto& d : result.detections) if (d.shouldEject) ++gangue;

        std::printf("  [%3zu] %s  %dx%d  %.1f ms  detections=%d  gangue=%d\n",
                    i + 1,
                    tifs[i].filename().string().c_str(),
                    img.width, img.height,
                    result.inferenceTimeMs,
                    (int)result.detections.size(),
                    gangue);

        // Save result summary as a text sidecar
        const std::string outPath = tifs[i].string() + ".result.txt";
        if (FILE* fp = std::fopen(outPath.c_str(), "w")) {
            std::fprintf(fp, "file: %s\n", tifs[i].filename().string().c_str());
            std::fprintf(fp, "size: %dx%d\n", img.width, img.height);
            std::fprintf(fp, "inference_ms: %.2f\n", result.inferenceTimeMs);
            std::fprintf(fp, "detections: %d\n", (int)result.detections.size());
            for (const auto& d : result.detections) {
                std::fprintf(fp, "  class=%d  px=[%d,%d)  conf=%.3f  eject=%s\n",
                             d.classId, d.startPx, d.endPx, d.confidence,
                             d.shouldEject ? "YES" : "no");
            }
            std::fclose(fp);
        }
    }

    std::printf("\n==> Total: %d frames, %d detections, avg inference %.1f ms\n",
                (int)tifs.size(), totalDetections,
                tifs.empty() ? 0.0 : totalMs / tifs.size());
    Logger::flush();
    return 0;
}
