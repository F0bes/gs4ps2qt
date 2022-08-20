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

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
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

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(gs4ps2qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
        add_executable(gs4ps2qt
            ${PROJECT_SOURCES}
        )
endif()

target_include_directories(gs4ps2qt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sockpp/include)
target_link_libraries(gs4ps2qt PRIVATE ${SOCKPP_LIB})

target_link_libraries(gs4ps2qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)


set_target_properties(gs4ps2qt PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE
)

install(TARGETS gs4ps2qt
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(gs4ps2qt)
endif()
