#include "TrtInferenceEngine.h"

#ifdef CGS_HAS_TENSORRT

#include <NvInferPlugin.h>
#include <cuda_runtime.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace cgs {
namespace core {

// ---------------------------------------------------------------------------
// Minimal TRT logger (suppress INFO; forward WARNING+ to stderr).
// ---------------------------------------------------------------------------
namespace {
class TrtLogger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING)
            std::cerr << "[TRT] " << msg << '\n';
    }
};
TrtLogger g_trtLogger;
}

TrtInferenceEngine::TrtInferenceEngine() = default;

TrtInferenceEngine::~TrtInferenceEngine() {
    if (m_devInput)  cudaFree(m_devInput);
    if (m_devOutput) cudaFree(m_devOutput);
}

// ---------------------------------------------------------------------------
// load — deserialise a .trt engine from disk.
// ---------------------------------------------------------------------------
bool TrtInferenceEngine::load(const InferenceConfig& cfg) {
    m_cfg    = cfg;
    m_loaded = false;

    const std::string& path = cfg.trtPath;
    if (path.empty()) return false;

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const size_t size = (size_t)f.tellg();
    f.seekg(0);
    std::vector<char> data(size);
    f.read(data.data(), (std::streamsize)size);
    if (!f) return false;

    // Initialise TRT plugins (required by YOLOv8-seg models).
    initLibNvInferPlugins(&g_trtLogger, "");

    m_runtime.reset(nvinfer1::createInferRuntime(g_trtLogger));
    if (!m_runtime) return false;

    m_engine.reset(m_runtime->deserializeCudaEngine(data.data(), size, nullptr));
    if (!m_engine) return false;

    m_context.reset(m_engine->createExecutionContext());
    if (!m_context) return false;

    // Allocate device buffers for input and output.
    const int S = cfg.imageSize;
    m_inputBytes = (size_t)3 * S * S * sizeof(float);
    cudaMalloc(&m_devInput, m_inputBytes);

    // Output tensor size: [1, stride, num_anchors]. Determine from engine.
    const int nbBindings = m_engine->getNbBindings();
    for (int i = 0; i < nbBindings; ++i) {
        if (!m_engine->bindingIsInput(i)) {
            const auto dims = m_engine->getBindingDimensions(i);
            size_t outElems = 1;
            for (int d = 0; d < dims.nbDims; ++d) outElems *= (size_t)dims.d[d];
            m_outputBytes = outElems * sizeof(float);
            cudaMalloc(&m_devOutput, m_outputBytes);
            m_hostOutput.resize(outElems);
            break;
        }
    }
    m_hostInput.resize((size_t)3 * S * S);

    m_loaded = true;
    return true;
}

// ---------------------------------------------------------------------------
// normalise — identical to OnnxInferenceEngine (shared logic).
// ---------------------------------------------------------------------------
void TrtInferenceEngine::normalise(const uint16_t* src, int srcW, int srcH,
                                    float* dst, int targetSize) const {
    const int total = 3 * targetSize * targetSize;
    std::fill(dst, dst + total, 0.0f);

    const float scaleX = (float)targetSize / (float)srcW;
    const float scaleY = (float)targetSize / (float)srcH;
    const float scale  = std::min(scaleX, scaleY);
    const int   dstW   = (int)(srcW * scale);
    const int   dstH   = (int)(srcH * scale);
    const int   offX   = (targetSize - dstW) / 2;
    const int   offY   = (targetSize - dstH) / 2;
    const float inv    = 1.0f / scale;
    const float norm   = 1.0f / 65535.0f;

    for (int y = 0; y < dstH; ++y) {
        const float srcYf = (y + 0.5f) * inv - 0.5f;
        const int   sy0   = std::max(0, (int)srcYf);
        const int   sy1   = std::min(srcH - 1, sy0 + 1);
        const float ty    = srcYf - sy0;
        for (int x = 0; x < dstW; ++x) {
            const float srcXf = (x + 0.5f) * inv - 0.5f;
            const int   sx0   = std::max(0, (int)srcXf);
            const int   sx1   = std::min(srcW - 1, sx0 + 1);
            const float tx    = srcXf - sx0;
            const float v = ((float)src[sy0*srcW+sx0]*(1-tx) + (float)src[sy0*srcW+sx1]*tx)*(1-ty)
                          + ((float)src[sy1*srcW+sx0]*(1-tx) + (float)src[sy1*srcW+sx1]*tx)*ty;
            const float n = v * norm;
            const int p = (offY+y)*targetSize + (offX+x);
            dst[0*targetSize*targetSize + p] = n;
            dst[1*targetSize*targetSize + p] = n;
            dst[2*targetSize*targetSize + p] = n;
        }
    }
}

// ---------------------------------------------------------------------------
// infer — normalise → H2D → run → D2H → parse.
// ---------------------------------------------------------------------------
InferenceResult TrtInferenceEngine::infer(const uint16_t* image, int width, int height) {
    InferenceResult result;
    if (!m_loaded || !m_context || !image) return result;

    const auto t0 = std::chrono::steady_clock::now();
    const int  S  = m_cfg.imageSize;

    // Normalise into host buffer.
    normalise(image, width, height, m_hostInput.data(), S);

    // Host → device.
    cudaMemcpy(m_devInput, m_hostInput.data(), m_inputBytes, cudaMemcpyHostToDevice);

    // Execute.
    void* bindings[] = {m_devInput, m_devOutput};
    m_context->executeV2(bindings);
    cudaDeviceSynchronize();

    // Device → host.
    cudaMemcpy(m_hostOutput.data(), m_devOutput, m_outputBytes, cudaMemcpyDeviceToHost);

    // Parse.
    // Shape: [1, stride, num_anchors]
    const auto dims = m_engine->getBindingDimensions(1);
    const int stride     = (int)dims.d[1];
    const int numAnchors = (int)dims.d[2];
    result = parseOutput(m_hostOutput.data(), numAnchors, stride, width, S);

    const auto t1 = std::chrono::steady_clock::now();
    result.inferenceTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

// ---------------------------------------------------------------------------
// parseOutput — identical logic to OnnxInferenceEngine.
// ---------------------------------------------------------------------------
InferenceResult TrtInferenceEngine::parseOutput(const float* data,
                                                 int numAnchors, int stride,
                                                 int origWidth, int imageSize) const {
    InferenceResult result;
    const float scaleX = (float)imageSize / (float)origWidth;
    const float scale  = scaleX;   // height usually > width for line-scan, but clamp to width
    const float offX   = ((float)imageSize - origWidth * scale) * 0.5f;

    std::vector<InferenceResult::Detection> candidates;

    for (int a = 0; a < numAnchors; ++a) {
        const float cx = data[0*numAnchors + a];
        const float bw = data[2*numAnchors + a];
        int   bestCls = -1;
        float bestSco = m_cfg.scoreThreshold;
        for (int c = 0; c < m_cfg.classCount; ++c) {
            const float s = data[(4+c)*numAnchors + a];
            if (s > bestSco) { bestSco = s; bestCls = c; }
        }
        if (bestCls < 0) continue;

        const float x1m = cx - bw * 0.5f;
        const float x2m = cx + bw * 0.5f;
        const int   s0  = (int)std::max(0.0f, (x1m - offX) / scale);
        const int   s1  = (int)std::min((float)(origWidth-1), (x2m - offX) / scale) + 1;
        if (s1 - s0 < m_cfg.minDetectionWidthPx) continue;

        InferenceResult::Detection d;
        d.startPx    = s0; d.endPx = s1;
        d.classId    = bestCls; d.confidence = bestSco;
        d.shouldEject = bestCls < (int)m_cfg.classEjectMask.size()
                       && m_cfg.classEjectMask[bestCls];
        candidates.push_back(d);
    }

    nms(candidates);
    result.detections = std::move(candidates);
    return result;
}

void TrtInferenceEngine::nms(std::vector<InferenceResult::Detection>& dets) const {
    std::sort(dets.begin(), dets.end(),
              [](const auto& a, const auto& b){ return a.confidence > b.confidence; });
    std::vector<bool> keep(dets.size(), true);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (!keep[i]) continue;
        for (size_t j = i+1; j < dets.size(); ++j) {
            if (!keep[j]) continue;
            const int interL = std::max(dets[i].startPx, dets[j].startPx);
            const int interR = std::min(dets[i].endPx,   dets[j].endPx);
            if (interR <= interL) continue;
            const float inter = (float)(interR - interL);
            const float uni   = (float)(std::max(dets[i].endPx, dets[j].endPx)
                                       - std::min(dets[i].startPx, dets[j].startPx));
            if (uni > 0 && inter/uni > m_cfg.iouThreshold) keep[j] = false;
        }
    }
    std::vector<InferenceResult::Detection> f;
    for (size_t i = 0; i < dets.size(); ++i) if (keep[i]) f.push_back(dets[i]);
    dets = std::move(f);
}

}}

#endif // CGS_HAS_TENSORRT
