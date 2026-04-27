if(CGS_BUILD_CONSOLE)
    add_executable(coal_gangue_sorter_console main_console.cpp)
    target_link_libraries(coal_gangue_sorter_console PRIVATE
        cgs_core
        cgs_hardware
    )
endif()

# Qt GUI app placeholder. Will be filled in Phase 6.
if(CGS_BUILD_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 5.12 COMPONENTS Widgets REQUIRED)
        set(QT_LIBS Qt5::Widgets)
    else()
        set(QT_LIBS Qt6::Widgets)
    endif()
    message(STATUS "GUI app placeholder enabled (Phase 6 will populate this).")
endif()
