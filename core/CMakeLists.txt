add_library(cgs_core STATIC
    Classifier/Classifier.cpp
    Classifier/Classifier.h
    Fusion/FusionPolicy.cpp
    Fusion/FusionPolicy.h
    NozzleMapping/NozzleMapper.cpp
    NozzleMapping/NozzleMapper.h
    Timing/TimingCalculator.cpp
    Timing/TimingCalculator.h
)

target_include_directories(cgs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
