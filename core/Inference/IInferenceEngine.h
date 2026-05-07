#ifndef CGS_IINFERENCEENGINE_H
#define CGS_IINFERENCEENGINE_H

// Abstract inference engine interface.
//
// The real system uses YOLOv8 segmentation models:
//   - .onnx  during development / cross-platform (OnnxInferenceEngine)
//   - .trt   in production on Windows+NVIDIA GPU (TrtInferenceEngine)
//
// Both engines expose the same interface so PipelineEngine can switch
// between them at startup without changing any other code.

#include "../Classifier/Classifier.h"   // for Material enum

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cgs {
namespace core {

// ---------------------------------------------------------------------------
// InferenceConfig — parameters read from <inference> section of config.xml
// ---------------------------------------------------------------------------
struct InferenceConfig {
    std::string trtPath{"best_Xray.trt"};    // TensorRT engine (Windows+NVIDIA)
    std::string onnxPath{"best_seg.onnx"};   // ONNX fallback
    int         imageSize{1280};             // model input dimension (square)
    float       iouThreshold{0.1f};          // NMS IoU threshold
    float       scoreThreshold{0.1f};        // minimum detection confidence
    int         classCount{3};               // number of model output classes
    // classEjectMask[i] = 1  → eject if classified as class i
    // Matches original Classes="1,0,0" — class 0 is gangue, eject it.
    std::vector<int> classEjectMask{1, 0, 0};
    int minDetectionWidthPx{5};
    // Air-burst duration scaling (100 = calculated time, <100 = shorter, >100 = longer)
    int burstScalePercent{100};
    int burstScaleSmallPercent{100};
    int burstScaleLargePercent{100};
    double smallObjectLenMm{40.0};
    double largeObjectLenMm{120.0};
};

// ---------------------------------------------------------------------------
// InferenceResult — output of one 2D frame inference pass
// ---------------------------------------------------------------------------
struct InferenceResult {
    struct Detection {
        int    startPx;      // left pixel column in detector frame (inclusive)
        int    endPx;        // right pixel column (exclusive)
        int    classId;      // model class index (0 = gangue on real machine)
        float  confidence;   // detection confidence 0..1
        bool   shouldEject;  // pre-computed from classEjectMask
    };
    std::vector<Detection> detections;
    double inferenceTimeMs{0.0};  // wall-clock time for this frame's inference
};

// ---------------------------------------------------------------------------
// IInferenceEngine — abstract interface
// ---------------------------------------------------------------------------
class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;

    // Load the model from disk. Returns false if file not found or model is invalid.
    virtual bool load(const InferenceConfig& cfg) = 0;

    // Run inference on a 16-bit grayscale image (width × height pixels).
    // Image is the full 2D frame produced by FrameAccumulator.
    virtual InferenceResult infer(const uint16_t* image,
                                  int width,
                                  int height) = 0;

    virtual bool isLoaded() const = 0;

    // Human-readable engine name for logging.
    virtual std::string engineName() const = 0;
};

// ---------------------------------------------------------------------------
// Factory: picks TRT if available and .trt file exists, else ONNX.
// Returns nullptr if neither is compiled in / available.
// ---------------------------------------------------------------------------
std::unique_ptr<IInferenceEngine> createBestInferenceEngine(const InferenceConfig& cfg);

}}

#endif
