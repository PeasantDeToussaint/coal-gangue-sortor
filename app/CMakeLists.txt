if(CGS_BUILD_CONSOLE)
    add_executable(coal_gangue_sorter_console main_console.cpp)
    target_link_libraries(coal_gangue_sorter_console PRIVATE
        cgs_core
        cgs_hardware
    )
endif()

if(CGS_BUILD_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 5.12 COMPONENTS Widgets REQUIRED)
        set(QT_TARGETS Qt5::Widgets)
    else()
        set(QT_TARGETS Qt6::Widgets)
    endif()

    add_executable(coal_gangue_sorter_gui
        gui/main_gui.cpp
        gui/MainWindow.cpp
        gui/MainWindow.h
        gui/XRayWaterfallView.cpp
        gui/XRayWaterfallView.h
        gui/NozzleMatrixView.cpp
        gui/NozzleMatrixView.h
        gui/ConfigPanel.cpp
        gui/ConfigPanel.h
        gui/StatsPanel.cpp
        gui/StatsPanel.h
    )
    target_link_libraries(coal_gangue_sorter_gui PRIVATE
        cgs_core
        cgs_hardware
        ${QT_TARGETS}
    )
endif()
