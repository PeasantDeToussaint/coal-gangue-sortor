#ifndef CGS_TRTINFERENCEENGINE_H
#define CGS_TRTINFERENCEENGINE_H

// TensorRT inference engine — Windows + NVIDIA GPU production mode.
//
// Enabled with: cmake -DCGS_HAS_TENSORRT=ON
//
// Loads a pre-built .trt engine file produced by trtexec:
//   trtexec --onnx=best_seg.onnx --saveEngine=best_Xray.trt
//
// The .trt file is GPU-specific (compiled for a particular CUDA compute
// capability). It must be rebuilt with trtexec when the GPU changes.
//
// Identical normalisation and output parsing as OnnxInferenceEngine so
// PipelineEngine can switch transparently.

#include "IInferenceEngine.h"

#ifdef CGS_HAS_TENSORRT

#include <NvInfer.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace cgs {
namespace core {

// NvInfer objects require custom deleters.
struct TrtDeleter {
    template <typename T> void operator()(T* obj) const { if (obj) obj->destroy(); }
};

class TrtInferenceEngine : public IInferenceEngine {
public:
    TrtInferenceEngine();
    ~TrtInferenceEngine() override;

    bool load(const InferenceConfig& cfg) override;
    InferenceResult infer(const uint16_t* image, int width, int height) override;
    bool isLoaded() const override { return m_loaded; }
    std::string engineName() const override { return "TensorRT"; }

private:
    void normalise(const uint16_t* src, int srcW, int srcH,
                   float* dst, int targetSize) const;

    InferenceResult parseOutput(const float* outputData,
                                int numAnchors, int stride,
                                int origWidth, int imageSize) const;

    void nms(std::vector<InferenceResult::Detection>& dets) const;

    // TRT objects
    std::unique_ptr<nvinfer1::IRuntime,        TrtDeleter> m_runtime;
    std::unique_ptr<nvinfer1::ICudaEngine,     TrtDeleter> m_engine;
    std::unique_ptr<nvinfer1::IExecutionContext, TrtDeleter> m_context;

    // CUDA device buffers
    void*  m_devInput{nullptr};
    void*  m_devOutput{nullptr};
    size_t m_inputBytes{0};
    size_t m_outputBytes{0};

    // Host staging buffers
    std::vector<float> m_hostInput;
    std::vector<float> m_hostOutput;

    InferenceConfig m_cfg;
    bool            m_loaded{false};
};

}}

#endif // CGS_HAS_TENSORRT
#endif
