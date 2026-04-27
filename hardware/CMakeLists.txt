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

if(CGS_HAS_BECKHOFF_ADS)
    list(APPEND CGS_HARDWARE_SOURCES
        PLCInterface/PLCBeckhoff.cpp
        PLCInterface/PLCBeckhoff.h
    )
endif()

add_library(cgs_hardware STATIC ${CGS_HARDWARE_SOURCES})

target_include_directories(cgs_hardware PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cgs_hardware PUBLIC Threads::Threads cgs_core)

if(CGS_HAS_AURORA_SDK)
    if(NOT CGS_AURORA_SDK_DIR)
        message(FATAL_ERROR "CGS_HAS_AURORA_SDK=ON but CGS_AURORA_SDK_DIR not set")
    endif()
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_AURORA_SDK=1)
    target_include_directories(cgs_hardware PUBLIC "${CGS_AURORA_SDK_DIR}/include")
    if(WIN32)
        target_link_libraries(cgs_hardware PUBLIC "${CGS_AURORA_SDK_DIR}/lib/xlib.lib")
    else()
        target_link_libraries(cgs_hardware PUBLIC "${CGS_AURORA_SDK_DIR}/lib/libxlib.so")
    endif()
endif()

if(CGS_HAS_BECKHOFF_ADS)
    if(NOT CGS_BECKHOFF_ADS_DIR)
        message(FATAL_ERROR "CGS_HAS_BECKHOFF_ADS=ON but CGS_BECKHOFF_ADS_DIR not set")
    endif()
    target_compile_definitions(cgs_hardware PUBLIC CGS_HAS_BECKHOFF_ADS=1)
    target_include_directories(cgs_hardware PUBLIC "${CGS_BECKHOFF_ADS_DIR}/AdsLib")
    add_subdirectory("${CGS_BECKHOFF_ADS_DIR}" beckhoff_ads_build EXCLUDE_FROM_ALL)
    target_link_libraries(cgs_hardware PUBLIC AdsLib)
endif()
