cmake_minimum_required(VERSION 3.16)
project(RovHud LANGUAGES CXX)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)
find_package(Qt5Multimedia 5.15 QUIET)

set(CMAKE_AUTOMOC ON)

add_executable(rov_hud
    src/main.cpp
    src/gyrohorizonwidget.cpp
    src/headingwidget.cpp
    src/rovhudwidget.cpp
    src/videosource.cpp
    src/videosource.h
)
target_compile_features(rov_hud PRIVATE cxx_std_17)
target_link_libraries(rov_hud PRIVATE Qt5::Widgets)

if(TARGET Qt5::Multimedia)
    target_sources(rov_hud PRIVATE src/webcamsource.cpp)
    target_compile_definitions(rov_hud PRIVATE ROV_HAS_QT_MULTIMEDIA)
    target_link_libraries(rov_hud PRIVATE Qt5::Multimedia)
else()
    message(STATUS "Qt 5 Multimedia is unavailable. Building with the test pattern source.")
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ARAVIS QUIET IMPORTED_TARGET aravis-0.8>=0.8.25)
endif()

if(TARGET PkgConfig::ARAVIS)
    find_package(Threads REQUIRED)
    target_sources(rov_hud PRIVATE src/gigecamerasource.cpp)
    target_compile_definitions(rov_hud PRIVATE ROV_HAS_ARAVIS)
    target_link_libraries(rov_hud PRIVATE PkgConfig::ARAVIS Threads::Threads)
else()
    message(STATUS "Aravis 0.8.25 or newer in the 0.8 series is unavailable. GigE Vision support is disabled.")
endif()
