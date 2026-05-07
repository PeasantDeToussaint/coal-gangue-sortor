#include "OnnxInferenceEngine.h"

#ifdef CGS_HAS_ONNX

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numeric>
#include <stdexcept>

namespace cgs {
namespace core {

OnnxInferenceEngine::OnnxInferenceEngine()
    : m_env(ORT_LOGGING_LEVEL_WARNING, "cgs_onnx") {
    m_sessionOptions.SetIntraOpNumThreads(2);
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
}

OnnxInferenceEngine::~OnnxInferenceEngine() = default;

bool OnnxInferenceEngine::load(const InferenceConfig& cfg) {
    m_cfg = cfg;
    m_loaded = false;

    const std::string& path = cfg.onnxPath;
    if (path.empty()) return false;

    // Try CUDA provider first (Windows+NVIDIA), fall back to CPU.
    try {
        OrtCUDAProviderOptions cudaOpts{};
        cudaOpts.device_id = 0;
        m_sessionOptions.AppendExecutionProvider_CUDA(cudaOpts);
    } catch (...) {
        // CUDA not available — CPU only, that's fine for dev.
    }

    try {
#ifdef _WIN32
        std::wstring wpath(path.begin(), path.end());
        m_session = std::make_unique<Ort::Session>(m_env, wpath.c_str(), m_sessionOptions);
#else
        m_session = std::make_unique<Ort::Session>(m_env, path.c_str(), m_sessionOptions);
#endif
    } catch (const Ort::Exception& e) {
        m_session.reset();
        return false;
    }

    m_loaded = true;
    return true;
}

// ---------------------------------------------------------------------------
// Normalise: resize + scale 16-bit → float32 [0,1] for YOLOv8 input.
//
// YOLOv8 expects CHW float32 with values in [0,1].
// We scale linearly: pixel / 65535.0f.
// The image is letterboxed (with zeros = transparent belt background) to
// maintain aspect ratio before scaling to imageSize × imageSize.
// ---------------------------------------------------------------------------
void OnnxInferenceEngine::normalise(const uint16_t* src, int srcW, int srcH,
                                    std::vector<float>& dst, int targetSize) const {
    dst.assign((size_t)3 * targetSize * targetSize, 0.0f);

    // Scale to fit within targetSize preserving aspect ratio.
    const float scaleX = (float)targetSize / (float)srcW;
    const float scaleY = (float)targetSize / (float)srcH;
    const float scale  = std::min(scaleX, scaleY);
    const int   dstW   = (int)(srcW * scale);
    const int   dstH   = (int)(srcH * scale);

    // Letterbox offsets (top-left padding).
    const int offX = (targetSize - dstW) / 2;
    const int offY = (targetSize - dstH) / 2;

    // Bilinear resize + channel fill (grayscale → 3 identical channels).
    const float invScale = 1.0f / scale;
    const float norm     = 1.0f / 65535.0f;

    for (int y = 0; y < dstH; ++y) {
        const float srcYf  = (y + 0.5f) * invScale - 0.5f;
        const int   srcY0  = std::max(0, (int)srcYf);
        const int   srcY1  = std::min(srcH - 1, srcY0 + 1);
        const float ty     = srcYf - srcY0;

        for (int x = 0; x < dstW; ++x) {
            const float srcXf = (x + 0.5f) * invScale - 0.5f;
            const int   srcX0 = std::max(0, (int)srcXf);
            const int   srcX1 = std::min(srcW - 1, srcX0 + 1);
            const float tx    = srcXf - srcX0;

            // Bilinear sample
            const float v00 = (float)src[srcY0 * srcW + srcX0];
            const float v10 = (float)src[srcY0 * srcW + srcX1];
            const float v01 = (float)src[srcY1 * srcW + srcX0];
            const float v11 = (float)src[srcY1 * srcW + srcX1];
            const float v   = (v00 * (1-tx) + v10 * tx) * (1-ty)
                            + (v01 * (1-tx) + v11 * tx) * ty;
            const float normalized = v * norm;

            const int dstPx = (offY + y) * targetSize + (offX + x);
            // Fill R, G, B channels identically (grayscale → RGB).
            dst[0 * targetSize * targetSize + dstPx] = normalized;
            dst[1 * targetSize * targetSize + dstPx] = normalized;
            dst[2 * targetSize * targetSize + dstPx] = normalized;
        }
    }
}

// ---------------------------------------------------------------------------
// infer — main entry point called per 2D frame.
// ---------------------------------------------------------------------------
InferenceResult OnnxInferenceEngine::infer(const uint16_t* image,
                                            int width, int height) {
    InferenceResult result;
    if (!m_loaded || !m_session || !image) return result;

    const auto t0 = std::chrono::steady_clock::now();

    const int S = m_cfg.imageSize;

    // 1) Normalise image → float32 CHW input tensor.
    normalise(image, width, height, m_inputBuf, S);

    // 2) Create input tensor.
    Ort::AllocatorWithDefaultOptions alloc;
    const std::array<int64_t, 4> inputShape{1, 3, S, S};
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeCPU);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memInfo,
        m_inputBuf.data(), m_inputBuf.size(),
        inputShape.data(), inputShape.size());

    // 3) Run inference.
    const char* inputNames[]  = {"images"};
    const char* outputNames[] = {"output0"};

    auto outputs = m_session->Run(
        Ort::RunOptions{nullptr},
        inputNames,  &inputTensor, 1,
        outputNames, 1);

    // 4) Parse output0.
    // YOLOv8-seg output shape: [1, num_classes+4+num_masks, num_anchors]
    // We only need the bounding box + class scores portion.
    auto& outTensor = outputs[0];
    const auto outShape = outTensor.GetTensorTypeAndShapeInfo().GetShape();
    const float* outData = outTensor.GetTensorMutableData<float>();

    // Shape: [1, 4+num_classes+num_masks, num_anchors] — transpose to [num_anchors, ...]
    const int numAnchors = (int)outShape[2];
    const int stride     = (int)outShape[1];   // 4 + classCount + maskDim

    std::vector<InferenceResult::Detection> candidates;

    for (int a = 0; a < numAnchors; ++a) {
        // Box (cx, cy, w, h) in model coordinates
        const float cx = outData[0 * numAnchors + a];
        const float cy = outData[1 * numAnchors + a];
        const float bw = outData[2 * numAnchors + a];
        const float bh = outData[3 * numAnchors + a];

        // Class scores
        int   bestClass = -1;
        float bestScore = m_cfg.scoreThreshold;
        for (int c = 0; c < m_cfg.classCount; ++c) {
            const float score = outData[(4 + c) * numAnchors + a];
            if (score > bestScore) {
                bestScore = score;
                bestClass = c;
            }
        }
        if (bestClass < 0) continue;

        // Convert from model-space to original pixel columns.
        // The model input was letterboxed; undo the scaling/offset.
        const float scaleX = (float)S / (float)width;
        const float scaleY = (float)S / (float)height;
        const float scale  = std::min(scaleX, scaleY);
        const float offX   = ((float)S - width  * scale) * 0.5f;
        // offY not needed for column-only mapping.

        const float x1_model = cx - bw * 0.5f;
        const float x2_model = cx + bw * 0.5f;
        const int   startPx  = (int)std::max(0.0f, (x1_model - offX) / scale);
        const int   endPx    = (int)std::min((float)(width - 1), (x2_model - offX) / scale) + 1;

        if (endPx - startPx < m_cfg.minDetectionWidthPx) continue;

        InferenceResult::Detection d;
        d.startPx    = startPx;
        d.endPx      = endPx;
        d.classId    = bestClass;
        d.confidence = bestScore;
        d.shouldEject = (bestClass < (int)m_cfg.classEjectMask.size())
                      && (m_cfg.classEjectMask[bestClass] != 0);
        candidates.push_back(d);
    }

    nms(candidates);
    result.detections = std::move(candidates);

    const auto t1 = std::chrono::steady_clock::now();
    result.inferenceTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

// ---------------------------------------------------------------------------
// NMS — suppress detections whose column ranges overlap by > iouThreshold.
// Sorted by confidence descending; highest confidence wins.
// ---------------------------------------------------------------------------
void OnnxInferenceEngine::nms(std::vector<InferenceResult::Detection>& dets) const {
    std::sort(dets.begin(), dets.end(),
              [](const auto& a, const auto& b){ return a.confidence > b.confidence; });

    std::vector<bool> keep(dets.size(), true);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (!keep[i]) continue;
        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (!keep[j]) continue;
            const int interL = std::max(dets[i].startPx, dets[j].startPx);
            const int interR = std::min(dets[i].endPx,   dets[j].endPx);
            if (interR <= interL) continue;
            const float inter = (float)(interR - interL);
            const float unionW = (float)(std::max(dets[i].endPx, dets[j].endPx)
                                       - std::min(dets[i].startPx, dets[j].startPx));
            if (unionW <= 0) continue;
            if (inter / unionW > m_cfg.iouThreshold) keep[j] = false;
        }
    }

    std::vector<InferenceResult::Detection> filtered;
    for (size_t i = 0; i < dets.size(); ++i) {
        if (keep[i]) filtered.push_back(dets[i]);
    }
    dets = std::move(filtered);
}

}}

#endif // CGS_HAS_ONNX
