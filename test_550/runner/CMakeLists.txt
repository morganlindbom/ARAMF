cmake_minimum_required(VERSION 3.21)
project(aramf_release_validation_runner LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)
file(GLOB_RECURSE ARAMF_SOURCES CONFIGURE_DEPENDS
    ../../src/core/*.cpp ../../src/core/*.h
    ../../src/ui/*.cpp ../../src/ui/*.h)
list(FILTER ARAMF_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")
add_executable(aramf_release_validation_runner main.cpp ${ARAMF_SOURCES})
target_include_directories(aramf_release_validation_runner PRIVATE ../../src)
target_link_libraries(aramf_release_validation_runner PRIVATE Qt6::Core Qt6::Widgets Qt6::Test)

add_executable(aramf_manual_visual_sweep visual_sweep.cpp ${ARAMF_SOURCES})
target_include_directories(aramf_manual_visual_sweep PRIVATE ../../src)
target_link_libraries(aramf_manual_visual_sweep PRIVATE Qt6::Core Qt6::Widgets Qt6::Test)

add_executable(aramf_manual_dialog_sweep dialog_sweep.cpp ${ARAMF_SOURCES})
target_include_directories(aramf_manual_dialog_sweep PRIVATE ../../src)
target_link_libraries(aramf_manual_dialog_sweep PRIVATE Qt6::Core Qt6::Widgets Qt6::Test)
