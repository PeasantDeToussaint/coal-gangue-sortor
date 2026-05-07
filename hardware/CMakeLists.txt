set(CGS_HARDWARE_SOURCES
    XRayInterface/IXRaySource.h
    XRayInterface/XRayMock.cpp
    XRayInterface/XRayMock.h
    DetectorInterface/IDetector.h
    DetectorInterface/DetectorMock.cpp
    DetectorInterface/DetectorMock.h
    CameraInterface/ICamera.h
    CameraInterface/CameraMock.cpp
    CameraInterface/CameraMock.h
    ValveDriverInterface/IValveDriver.h
    ValveDriverInterface/ValveDriverMock.cpp
    ValveDriverInterface/ValveDriverMock.h
    PLCInterface/IPLC.h
    PLCInterface/PLCMock.cpp
    PLCInterface/PLCMock.h
)

if(CGS_HAS_SERIAL)
    list(APPEND CGS_HARDWARE_SOURCES
        XRayInterface/XRaySerial.cpp
        XRayInterface/XRaySerial.h
    )
endif()

if(CGS_HAS_AURORA_SDK)
    list(APPEND CGS_HARDWARE_SOURCES
        DetectorInterface/DetectorAurora.cpp
        DetectorInterface/DetectorAurora.h
    )
endif()

# DetectorLib.dll adapter (袁工's Aurora wrapper — preferred over raw Aurora SDK).
# Requires Qt (DetectorLibInterface is a QObject).
# Place DetectorLib.lib in third_party/detectorlib/ (already done).
if(CGS_HAS_DETECTORLIB)
    list(APPEND CGS_HARDWARE_SOURCES
        DetectorInterface/DetectorDetectorLib.cpp
        DetectorInterface/DetectorDetectorLib.h
    )
endif()

if(CGS_HAS_BECKHOFF_ADS)
    list(APPEND CGS_HARDWARE_SOURCES
        PLCInterface/PLCBeckhoff.cpp
        PLCInterface/PLCBeckhoff.h
        ValveDriverInterface/ValveDriverEL2828.cpp
        ValveDriverInterface/ValveDriverEL2828.h
        XRayInterface/BeiduAdapter.cpp
        XRayInterface/BeiduAdapter.h
    )
endif()

# ACS motion controller TCP client (no extra library, uses platform sockets)
list(APPEND CGS_HARDWARE_SOURCES
    PLCInterface/ACSMotionClient.cpp
    PLCInterface/ACSMotionClient.h
)

# XRayLib.dll adapter (袁工's VJ serial wrapper — preferred on production machine)
if(CGS_HAS_XRAYLIB)
    list(APPEND CGS_HARDWARE_SOURCES
        XRayInterface/XRayLibAdapter.cpp
        XRayInterface/XRayLibAdapter.h
    )
endif()

# BeckHoffLib.dll adapter (simpler ADS wrapper, no open-source AdsLib needed)
if(CGS_HAS_BECKOFFLIB)
    list(APPEND CGS_HARDWARE_SOURCES
        ValveDriverInterface/ValveDriverBeckHoffLib.cpp
        ValveDriverInterface/ValveDriverBeckHoffLib.h
    )
endif()

# PLCControlLib.dll adapter (袁工's S7 TCP wrapper)
if(CGS_HAS_PLCCONTROLLIB)
    list(APPEND CGS_HARDWARE_SOURCES
        PLCInterface/PLCControlLibAdapter.cpp
        PLCInterface/PLCControlLibAdapter.h
    )
endif()

# All source appends must be before add_library.
if(CGS_HAS_HIKVISION)
    list(APPEND CGS_HARDWARE_SOURCES
        CameraInterface/CameraHikGigE.h
        CameraInterface/CameraHikGigE.cpp
    )
endif()

if(CGS_HAS_SNAP7)
    list(APPEND CGS_HARDWARE_SOURCES
        PLCInterface/PLCS7.h
        PLCInterface/PLCS7.cpp
    )
endif()

add_library(cgs_hardware STATIC ${CGS_HARDWARE_SOURCES})

target_include_directories(cgs_hardware PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cgs_hardware PUBLIC Threads::Threads cgs_core)

# --------------------------------------------------------------------------
# DetectorLib.dll (袁工's wrapper — preferred; no Aurora SDK headers needed)
# --------------------------------------------------------------------------
if(CGS_HAS_DETECTORLIB)
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_DETECTORLIB=1)
    set(CGS_DETECTORLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/detectorlib"
        CACHE PATH "DetectorLib root (contains DetectorLib.lib + DetectorLibInterface.h)")
    target_include_directories(cgs_hardware PUBLIC "${CGS_DETECTORLIB_DIR}")
    if(WIN32)
        target_link_libraries(cgs_hardware PUBLIC "${CGS_DETECTORLIB_DIR}/DetectorLib.lib")
    endif()
    # DetectorLibInterface is a QObject — needs Qt
    find_package(Qt5 5.12 COMPONENTS Core QUIET)
    if(Qt5Core_FOUND)
        target_link_libraries(cgs_hardware PUBLIC Qt5::Core)
    endif()
endif()

# --------------------------------------------------------------------------
# Aurora SDK (only if DetectorLib is unavailable)
# --------------------------------------------------------------------------
if(CGS_HAS_AURORA_SDK)
    if(NOT CGS_AURORA_SDK_DIR)
        message(FATAL_ERROR "CGS_HAS_AURORA_SDK=ON but CGS_AURORA_SDK_DIR not set")
    endif()
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_AURORA_SDK=1)
    target_include_directories(cgs_hardware PUBLIC "${CGS_AURORA_SDK_DIR}/include")
    if(WIN32)
        # The SDK DLL is named XLibDll.lib (not xlib.lib — match actual SDK filename)
        target_link_libraries(cgs_hardware PUBLIC "${CGS_AURORA_SDK_DIR}/lib/XLibDll.lib")
    else()
        target_link_libraries(cgs_hardware PUBLIC "${CGS_AURORA_SDK_DIR}/lib/libxlib.so")
    endif()
endif()

# --------------------------------------------------------------------------
# Beckhoff TwinCAT ADS
# --------------------------------------------------------------------------
if(CGS_HAS_BECKHOFF_ADS)
    if(NOT CGS_BECKHOFF_ADS_DIR)
        message(FATAL_ERROR "CGS_HAS_BECKHOFF_ADS=ON but CGS_BECKHOFF_ADS_DIR not set")
    endif()
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_BECKHOFF_ADS=1)
    target_include_directories(cgs_hardware PUBLIC "${CGS_BECKHOFF_ADS_DIR}/AdsLib")
    add_subdirectory("${CGS_BECKHOFF_ADS_DIR}" beckhoff_ads_build EXCLUDE_FROM_ALL)
    target_link_libraries(cgs_hardware PUBLIC AdsLib)
endif()

# --------------------------------------------------------------------------
# Hikvision GigE camera
# --------------------------------------------------------------------------
if(CGS_HAS_HIKVISION)
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_HIKVISION=1)
    set(CGS_HIKVISION_DIR "" CACHE PATH "Hikvision MvCameraControl SDK root")
    if(CGS_HIKVISION_DIR)
        target_include_directories(cgs_hardware PUBLIC "${CGS_HIKVISION_DIR}/Includes")
        if(WIN32)
            target_link_libraries(cgs_hardware PUBLIC
                "${CGS_HIKVISION_DIR}/Libraries/win64/MvCameraControl.lib")
        else()
            target_link_libraries(cgs_hardware PUBLIC
                "${CGS_HIKVISION_DIR}/Libraries/Linux_x86_64/libMvCameraControl.so")
        endif()
    else()
        message(WARNING "CGS_HAS_HIKVISION=ON but CGS_HIKVISION_DIR not set. "
                        "Set -DCGS_HIKVISION_DIR=<SDK root>.")
    endif()
endif()

# --------------------------------------------------------------------------
# Siemens S7-1500 via snap7
# --------------------------------------------------------------------------
if(CGS_HAS_SNAP7)
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_SNAP7=1)
    if(CGS_SNAP7_DIR)
        target_include_directories(cgs_hardware PUBLIC "${CGS_SNAP7_DIR}/include")
        if(WIN32)
            target_link_libraries(cgs_hardware PUBLIC "${CGS_SNAP7_DIR}/lib/snap7.lib")
        else()
            target_link_libraries(cgs_hardware PUBLIC "${CGS_SNAP7_DIR}/lib/libsnap7.so")
        endif()
    else()
        find_library(SNAP7_LIB snap7)
        if(SNAP7_LIB)
            target_link_libraries(cgs_hardware PUBLIC ${SNAP7_LIB})
        else()
            message(WARNING "CGS_HAS_SNAP7=ON but snap7 not found. "
                            "Set CGS_SNAP7_DIR or install libsnap7-dev.")
        endif()
    endif()
endif()

# --------------------------------------------------------------------------
# XRayLib.dll (袁工's VJ X-ray serial wrapper)
# --------------------------------------------------------------------------
if(CGS_HAS_XRAYLIB)
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_XRAYLIB=1)
    set(CGS_XRAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/xraylib"
        CACHE PATH "XRayLib root (contains XRayLib.lib + XRayLibInterface.h)")
    target_include_directories(cgs_hardware PUBLIC "${CGS_XRAYLIB_DIR}")
    if(WIN32)
        target_link_libraries(cgs_hardware PUBLIC "${CGS_XRAYLIB_DIR}/XRayLib.lib")
    endif()
    find_package(Qt5 5.12 COMPONENTS Core QUIET)
    if(Qt5Core_FOUND)
        target_link_libraries(cgs_hardware PUBLIC Qt5::Core)
    endif()
endif()

# --------------------------------------------------------------------------
# BeckHoffLib.dll (袁工's ADS wrapper)
# --------------------------------------------------------------------------
if(CGS_HAS_BECKOFFLIB)
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_BECKOFFLIB=1)
    set(CGS_BECKOFFLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/beckofflib"
        CACHE PATH "BeckHoffLib root (contains BeckHoffLib.lib + BeckHoffLibInterface.h)")
    target_include_directories(cgs_hardware PUBLIC "${CGS_BECKOFFLIB_DIR}")
    if(WIN32)
        target_link_libraries(cgs_hardware PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/detectorlib/../../Gangue/BeckHoffLib.lib")
    endif()
endif()

# --------------------------------------------------------------------------
# PLCControlLib.dll (袁工's S7 TCP wrapper)
# --------------------------------------------------------------------------
if(CGS_HAS_PLCCONTROLLIB)
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_PLCCONTROLLIB=1)
    set(CGS_PLCCONTROLLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/plccontrollib"
        CACHE PATH "PLCControlLib root (contains PLCControlLib.lib + PLCControlLibInterface.h)")
    target_include_directories(cgs_hardware PUBLIC "${CGS_PLCCONTROLLIB_DIR}")
    if(WIN32)
        target_link_libraries(cgs_hardware PUBLIC "${CGS_PLCCONTROLLIB_DIR}/PLCControlLib.lib")
    endif()
    find_package(Qt5 5.12 COMPONENTS Core Network QUIET)
    if(Qt5Network_FOUND)
        target_link_libraries(cgs_hardware PUBLIC Qt5::Core Qt5::Network)
    endif()
endif()
