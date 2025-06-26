cmake_minimum_required(VERSION 3.16)

project(RCmdLaunch VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Widgets QuickControls2 Sql)

qt_standard_project_setup(REQUIRES 6.8)

file(GLOB_RECURSE CPP_FILE_LIST
    "./src/cpp/*.cpp"
    "./src/cpp/*.h"
)

qt_add_executable(appRCmdLaunch
    ${CPP_FILE_LIST} ./src/res/resources.qrc
)

# 添加 Windows 图标
if(WIN32)
    set_property(TARGET appRCmdLaunch
 PROPERTY WIN32_EXECUTABLE TRUE)
    target_sources(appRCmdLaunch
 PRIVATE ./src/res/app.rc)
endif()

file(GLOB_RECURSE QML_FILE_LIST
    "${CMAKE_CURRENT_SOURCE_DIR}/src/layout/*.qml"
)

foreach(filepath ${QML_FILE_LIST})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" filename ${filepath}) #.+/(.+)\\\\..*" "\\\\1
    list(APPEND QML_PATH_LIST ${filename})
    message("Adding QML file: ${filename}")
endforeach(filepath)

qt_add_qml_module(appRCmdLaunch
    URI RCmdLaunch

    VERSION 1.0
    QML_FILES ${QML_PATH_LIST}
    RESOURCES ./src/res/resources.qrc
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appRCmdLaunch PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appRCmdLaunch
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appRCmdLaunch
    PRIVATE Qt6::Quick Qt6::Widgets Qt6::QuickControls2 Qt6::Sql
)

include(GNUInstallDirs)
install(TARGETS appRCmdLaunch
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
