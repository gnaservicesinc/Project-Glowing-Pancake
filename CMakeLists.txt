cmake_minimum_required(VERSION 3.24)

# Keep IDE/direct CMake configuration safe as well as the make.sh entry point.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake --preset core-release")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()
option(PGP_CORE_ONLY "Build without looking for Qt or desktop dependencies" OFF)
include(cmake/AppleToolchain.cmake)

# Component/release identity comes from project-versions.json in M0-05.
# Do not invent a product version for this development skeleton.
project(ProjectGlowingPancake LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)
include(cmake/ProjectOptions.cmake)

add_subdirectory(src/core)
add_subdirectory(src/headless)

if(NOT PGP_CORE_ONLY)
    find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)
    qt_standard_project_setup()
    add_subdirectory(src/platform/resources)
    add_subdirectory(src/platform/graphics)
    add_subdirectory(src/platform/audio)
    add_subdirectory(src/app)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
