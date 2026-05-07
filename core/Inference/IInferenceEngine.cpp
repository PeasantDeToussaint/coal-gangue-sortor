#include "IInferenceEngine.h"

#ifdef CGS_HAS_TENSORRT
#include "TrtInferenceEngine.h"
#endif

#ifdef CGS_HAS_ONNX
#include "OnnxInferenceEngine.h"
#endif

#include <filesystem>

namespace cgs {
namespace core {

// ---------------------------------------------------------------------------
// createBestInferenceEngine
//
// Selection priority (matches original Gangue.exe logic):
//   1. TensorRT if compiled in AND .trt file exists  →  fastest, GPU-native
//   2. ONNX Runtime if compiled in AND .onnx exists  →  cross-platform dev
//   3. nullptr                                        →  caller uses Classifier
// ---------------------------------------------------------------------------
std::unique_ptr<IInferenceEngine> createBestInferenceEngine(const InferenceConfig& cfg) {
    namespace fs = std::filesystem;

#ifdef CGS_HAS_TENSORRT
    if (!cfg.trtPath.empty() && fs::exists(cfg.trtPath)) {
        auto eng = std::make_unique<TrtInferenceEngine>();
        if (eng->load(cfg)) return eng;
        // Fall through to ONNX if TRT load failed (wrong GPU etc.)
    }
#endif

#ifdef CGS_HAS_ONNX
    if (!cfg.onnxPath.empty() && fs::exists(cfg.onnxPath)) {
        auto eng = std::make_unique<OnnxInferenceEngine>();
        if (eng->load(cfg)) return eng;
    }
#endif

    return nullptr;  // no engine available
}

}}
