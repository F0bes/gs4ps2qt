cmake_minimum_required(VERSION 3.5)

project(gs4ps2qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
	set(SOCKPP_LIB sockpp-static)
else()
	set(SOCKPP_LIB sockpp)
endif()

add_subdirectory(sockpp)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ps2client.cpp
        ps2client.h
        gsdump.cpp
        gsdump.h
        screenshotdialog.cpp
        screenshotdialog.h
        screenshotdialog.ui
        clickablelabel.cpp
        clickablelabel.h
)

qt_add_executable(gs4ps2qt MANUAL_FINALIZATION ${PROJECT_SOURCES})


target_include_directories(gs4ps2qt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sockpp/include)
target_link_libraries(gs4ps2qt PRIVATE ${SOCKPP_LIB})

target_link_libraries(gs4ps2qt PRIVATE Qt6::Widgets Qt6::Network)


set_target_properties(gs4ps2qt PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(gs4ps2qt)
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
    set(BINARIES_TYPE --release)
else()
    set(BINARIES_TYPE --debug)
endif()

add_custom_command(
    TARGET gs4ps2qt POST_BUILD
    COMMAND $ENV{Qt6_DIR}/bin/windeployqt.exe ${BINARIES_TYPE} --compiler-runtime --dir ${CMAKE_BINARY_DIR}/gs4ps2qt $<TARGET_FILE:gs4ps2qt>
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gs4ps2qt> ${CMAKE_BINARY_DIR}/gs4ps2qt
)

install(
    DIRECTORY ${CMAKE_BINARY_DIR}/gs4ps2qt
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    COMPONENT gs4ps2qt_component
)

