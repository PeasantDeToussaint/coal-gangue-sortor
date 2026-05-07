cmake_minimum_required(VERSION 3.16)
project(coal_gangue_sorter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CGS_BUILD_TESTS    "Build unit tests" ON)
option(CGS_BUILD_GUI      "Build Qt GUI app (requires Qt5/Qt6)" OFF)
option(CGS_BUILD_CONSOLE  "Build console pipeline runner" ON)
option(CGS_HAS_SERIAL       "Enable POSIX/Win32 serial port support (XRaySerial)" ON)
option(CGS_HAS_DETECTORLIB  "Enable DetectorLib.dll adapter (袁工's Aurora wrapper, preferred)" OFF)
option(CGS_HAS_AURORA_SDK   "Enable Detection Technology Aurora SDK adapter (needs SDK)" OFF)
option(CGS_HAS_XRAYLIB      "Enable XRayLib.dll adapter (袁工's VJ serial wrapper, preferred)" OFF)
option(CGS_HAS_BECKOFFLIB   "Enable BeckHoffLib.dll adapter (袁工's ADS wrapper)" OFF)
option(CGS_HAS_PLCCONTROLLIB "Enable PLCControlLib.dll adapter (袁工's S7 TCP wrapper)" OFF)
option(CGS_HAS_BECKHOFF_ADS "Enable Beckhoff TwinCAT ADS adapter (needs Beckhoff/ADS lib)" OFF)
option(CGS_HAS_SNAP7        "Enable Siemens S7-1500 PLC adapter via snap7 library" OFF)
option(CGS_HAS_ONNX         "Enable ONNX Runtime inference engine" OFF)
option(CGS_HAS_TENSORRT     "Enable TensorRT inference engine (Windows+NVIDIA only)" OFF)
option(CGS_HAS_HIKVISION    "Enable Hikvision GigE camera adapter (needs MvCameraControl SDK)" OFF)
set(CGS_ONNX_DIR      "" CACHE PATH "ONNX Runtime SDK root (leave empty to auto-fetch)")
set(CGS_TENSORRT_DIR  "" CACHE PATH "TensorRT SDK root (contains include/ and lib/)")
set(CGS_SNAP7_DIR     "" CACHE PATH "snap7 library root (contains libsnap7.so or snap7.lib)")
set(CGS_AURORA_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/aurora-sdk"
    CACHE PATH "Path to Aurora SDK root (contains include/ + lib/)")
set(CGS_BECKHOFF_ADS_DIR "" CACHE PATH "Path to Beckhoff/ADS source tree (clone of github.com/Beckhoff/ADS)")
if(NOT CGS_BECKHOFF_ADS_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ads/AdsLib/AdsLib.h")
    set(CGS_BECKHOFF_ADS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ads" CACHE PATH "Path to Beckhoff/ADS source tree (clone of github.com/Beckhoff/ADS)" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Threads (used by mock detector / camera).
find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG 10.0.0)
FetchContent_MakeAvailable(tinyxml2)

# spdlog — rotating file logger matching original gangue_sys.log pattern.
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1)
FetchContent_MakeAvailable(spdlog)

add_subdirectory(core)
add_subdirectory(hardware)

if(CGS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CGS_BUILD_CONSOLE OR CGS_BUILD_GUI)
    add_subdirectory(app)
endif()
