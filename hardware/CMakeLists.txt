add_library(cgs_hardware STATIC
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

target_include_directories(cgs_hardware PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cgs_hardware PUBLIC Threads::Threads)
