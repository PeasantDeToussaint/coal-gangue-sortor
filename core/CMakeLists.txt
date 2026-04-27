set(CGS_CORE_SOURCES
    Classifier/Classifier.cpp
    Classifier/Classifier.h
    Fusion/FusionPolicy.cpp
    Fusion/FusionPolicy.h
    NozzleMapping/NozzleMapper.cpp
    NozzleMapping/NozzleMapper.h
    Timing/TimingCalculator.cpp
    Timing/TimingCalculator.h
)

if(CGS_HAS_SERIAL)
    list(APPEND CGS_CORE_SOURCES
        Serial/SerialPort.cpp
        Serial/SerialPort.h
    )
endif()

add_library(cgs_core STATIC ${CGS_CORE_SOURCES})

target_include_directories(cgs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(CGS_HAS_SERIAL)
    target_compile_definitions(cgs_core PUBLIC CGS_HAS_SERIAL=1)
endif()
