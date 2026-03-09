cmake_minimum_required(VERSION 3.16)
project(DCQuestCompleter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(APP_ICON_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
file(WRITE "${APP_ICON_RESOURCE}"
        "IDI_ICON1 ICON DISCARDABLE \"${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico\"")

add_executable(DCQuestCompleter
        launcher/main.cpp
        launcher/MainWindow.h
        launcher/MainWindow.cpp
        launcher/GameModel.h
        launcher/ButtonDelegate.h
        resources/resources.qrc
        ${APP_ICON_RESOURCE}
)

include_directories(${CMAKE_SOURCE_DIR}/include)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Network
        REQUIRED)

target_link_libraries(DCQuestCompleter PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)

get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET DCQuestCompleter POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --verbose 0
            --no-compiler-runtime
            --no-translations
            $<$<CONFIG:Release>:--release>
            $<$<CONFIG:Debug>:--debug>
            "$<TARGET_FILE:DCQuestCompleter>"
            COMMENT "Deploying Qt dependencies..."
    )
else()
    message(WARNING "windeployqt not found!")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET DCQuestCompleter PROPERTY WIN32_EXECUTABLE true)
endif()