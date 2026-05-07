#ifndef CGS_ONNXINFERENCEENGINE_H
#define CGS_ONNXINFERENCEENGINE_H

// ONNX Runtime inference engine.
//
// Enabled with: cmake -DCGS_HAS_ONNX=ON
//
// Loads a YOLOv8-seg .onnx model, normalises 16-bit detector images to
// float32, runs inference, and parses the segmentation output into column-
// range detections that drive the nozzle mapper.
//
// Supports both CPU and CUDA execution providers. On Windows with an NVIDIA
// GPU, CUDA is preferred; falls back to CPU automatically.

#include "IInferenceEngine.h"

#ifdef CGS_HAS_ONNX

#include <onnxruntime_cxx_api.h>

#include <memory>
#include <string>
#include <vector>

namespace cgs {
namespace core {

class OnnxInferenceEngine : public IInferenceEngine {
public:
    OnnxInferenceEngine();
    ~OnnxInferenceEngine() override;

    bool load(const InferenceConfig& cfg) override;
    InferenceResult infer(const uint16_t* image, int width, int height) override;
    bool isLoaded() const override { return m_loaded; }
    std::string engineName() const override { return "ONNX Runtime"; }

private:
    // Normalise 16-bit X-ray pixels → float32 in [0, 1].
    // Detector max value is 65535 (16-bit full scale).
    void normalise(const uint16_t* src, int srcW, int srcH,
                   std::vector<float>& dst, int targetSize) const;

    // Parse YOLOv8-seg model output tensors into InferenceResult.
    // YOLOv8-seg outputs: [1, num_detections, 4+num_classes+num_masks]
    InferenceResult parseOutput(const std::vector<float>& boxes,
                                const std::vector<int64_t>& boxShape,
                                int origWidth,
                                int imageSize) const;

    // NMS: suppress overlapping detections by column overlap ratio.
    void nms(std::vector<InferenceResult::Detection>& dets) const;

    Ort::Env            m_env;
    Ort::SessionOptions m_sessionOptions;
    std::unique_ptr<Ort::Session> m_session;

    InferenceConfig m_cfg;
    bool            m_loaded{false};

    // Pre-allocated inference buffers (reused across frames).
    std::vector<float> m_inputBuf;
    std::vector<float> m_outputBuf;
};

}}

#endif // CGS_HAS_ONNX
#endif
