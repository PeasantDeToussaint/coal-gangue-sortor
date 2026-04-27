add_executable(cgs_tests
    test_main.cpp
    test_harness.h
    test_classifier.cpp
    test_nozzle_mapper.cpp
    test_timing.cpp
    test_fusion.cpp
    test_xray_serial_format.cpp
)

target_link_libraries(cgs_tests PRIVATE cgs_core)

add_test(NAME cgs_tests COMMAND cgs_tests)
