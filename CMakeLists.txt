cmake_minimum_required(VERSION 3.16)
project(coal_gangue_sorter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CGS_BUILD_TESTS "Build unit tests" ON)
option(CGS_BUILD_GUI   "Build Qt GUI app (requires Qt5/Qt6)" OFF)
option(CGS_BUILD_CONSOLE "Build console pipeline runner" ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Threads (used by mock detector / camera).
find_package(Threads REQUIRED)

add_subdirectory(core)
add_subdirectory(hardware)

if(CGS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CGS_BUILD_CONSOLE OR CGS_BUILD_GUI)
    add_subdirectory(app)
endif()
