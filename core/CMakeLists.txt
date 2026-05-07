set(CGS_CORE_SOURCES
    Classifier/Classifier.cpp
    Classifier/Classifier.h
    Config/ConfigLoader.cpp
    Config/ConfigLoader.h
    Fusion/FusionPolicy.cpp
    Fusion/FusionPolicy.h
    NozzleMapping/NozzleMapper.cpp
    NozzleMapping/NozzleMapper.h
    Timing/TimingCalculator.cpp
    Timing/TimingCalculator.h
    Pipeline/PipelineEngine.cpp
    Pipeline/PipelineEngine.h
    Inference/IInferenceEngine.h
    Inference/IInferenceEngine.cpp
    Inference/FrameAccumulator.h
    Inference/FrameAccumulator.cpp
    Logging/Logger.h
    Logging/Logger.cpp
    Recording/FrameRecorder.h
    Recording/FrameRecorder.cpp
)

if(CGS_HAS_SERIAL)
    list(APPEND CGS_CORE_SOURCES
        Serial/SerialPort.cpp
        Serial/SerialPort.h
    )
endif()

if(CGS_HAS_ONNX)
    list(APPEND CGS_CORE_SOURCES
        Inference/OnnxInferenceEngine.h
        Inference/OnnxInferenceEngine.cpp
    )
endif()

if(CGS_HAS_TENSORRT)
    list(APPEND CGS_CORE_SOURCES
        Inference/TrtInferenceEngine.h
        Inference/TrtInferenceEngine.cpp
    )
endif()

add_library(cgs_core STATIC ${CGS_CORE_SOURCES})

target_include_directories(cgs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cgs_core PUBLIC tinyxml2 Threads::Threads spdlog::spdlog)

if(CGS_HAS_SERIAL)
    target_compile_definitions(cgs_core PUBLIC CGS_HAS_SERIAL=1)
endif()

# --------------------------------------------------------------------------
# ONNX Runtime
# --------------------------------------------------------------------------
if(CGS_HAS_ONNX)
    target_compile_definitions(cgs_core PUBLIC CGS_HAS_ONNX=1)
    if(NOT DEFINED CGS_ONNX_DIR OR CGS_ONNX_DIR STREQUAL "")
        include(FetchContent)
        message(STATUS "CGS: Fetching ONNX Runtime (this may take a while on first build)")
        if(WIN32)
            FetchContent_Declare(onnxruntime_pkg
                URL "https://github.com/microsoft/onnxruntime/releases/download/v1.18.0/onnxruntime-win-x64-gpu-1.18.0.zip"
                URL_HASH SHA256=0)
        elseif(APPLE)
            FetchContent_Declare(onnxruntime_pkg
                URL "https://github.com/microsoft/onnxruntime/releases/download/v1.18.0/onnxruntime-osx-universal2-1.18.0.tgz"
                URL_HASH SHA256=0)
        else()
            FetchContent_Declare(onnxruntime_pkg
                URL "https://github.com/microsoft/onnxruntime/releases/download/v1.18.0/onnxruntime-linux-x64-gpu-1.18.0.tgz"
                URL_HASH SHA256=0)
        endif()
        FetchContent_MakeAvailable(onnxruntime_pkg)
        set(CGS_ONNX_DIR "${onnxruntime_pkg_SOURCE_DIR}" CACHE PATH "ONNX Runtime SDK root" FORCE)
    endif()
    target_include_directories(cgs_core PUBLIC "${CGS_ONNX_DIR}/include")
    if(WIN32)
        target_link_libraries(cgs_core PUBLIC "${CGS_ONNX_DIR}/lib/onnxruntime.lib")
    elseif(APPLE)
        target_link_libraries(cgs_core PUBLIC "${CGS_ONNX_DIR}/lib/libonnxruntime.dylib")
    else()
        target_link_libraries(cgs_core PUBLIC "${CGS_ONNX_DIR}/lib/libonnxruntime.so")
    endif()
endif()

# --------------------------------------------------------------------------
# TensorRT (Windows + NVIDIA only)
# --------------------------------------------------------------------------
if(CGS_HAS_TENSORRT)
    if(NOT WIN32)
        message(FATAL_ERROR "CGS_HAS_TENSORRT is only supported on Windows (NVIDIA)")
    endif()
    target_compile_definitions(cgs_core PUBLIC CGS_HAS_TENSORRT=1)
    if(NOT DEFINED CGS_TENSORRT_DIR OR CGS_TENSORRT_DIR STREQUAL "")
        # Default: look for TensorRT in the CUDA installation
        set(CGS_TENSORRT_DIR "$ENV{TENSORRT_DIR}" CACHE PATH "TensorRT SDK root")
    endif()
    if(CGS_TENSORRT_DIR)
        target_include_directories(cgs_core PUBLIC "${CGS_TENSORRT_DIR}/include")
        target_link_libraries(cgs_core PUBLIC
            "${CGS_TENSORRT_DIR}/lib/nvinfer.lib"
            "${CGS_TENSORRT_DIR}/lib/nvinfer_plugin.lib"
            "${CGS_TENSORRT_DIR}/lib/nvonnxparser.lib"
        )
    endif()
    find_package(CUDAToolkit REQUIRED)
    target_link_libraries(cgs_core PUBLIC CUDA::cudart)
endif()
