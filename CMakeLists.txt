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
