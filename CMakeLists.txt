# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

cmake_minimum_required(VERSION 3.14)

project(PerFlow
    VERSION 0.1.0
    DESCRIPTION "A programmable and fast performance analysis for parallel programs"
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Options
option(PERFLOW_BUILD_TESTS "Build unit tests" ON)
option(PERFLOW_BUILD_EXAMPLES "Build example programs" OFF)
option(PERFLOW_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

# Sanitizers for development
if(PERFLOW_ENABLE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address,undefined")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Header-only library (no source files needed for the core)
add_library(perflow_sampling INTERFACE)
target_include_directories(perflow_sampling INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional: Find zlib for compression support
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_compile_definitions(perflow_sampling INTERFACE PERFLOW_HAVE_ZLIB)
    target_link_libraries(perflow_sampling INTERFACE ZLIB::ZLIB)
    message(STATUS "zlib found - compression support enabled")
else()
    message(STATUS "zlib not found - compression support disabled")
endif()

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(perflow_sampling INTERFACE Threads::Threads)

# Tests
if(PERFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(PERFLOW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS perflow_sampling
    EXPORT PerFlowTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/sampling
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT PerFlowTargets
    FILE PerFlowTargets.cmake
    NAMESPACE PerFlow::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PerFlow
)

# Package configuration
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PerFlowConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PerFlowConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/PerFlowConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PerFlow
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PerFlowConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PerFlowConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PerFlow
)

# Summary
message(STATUS "")
message(STATUS "PerFlow Configuration Summary")
message(STATUS "==============================")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:   C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests:    ${PERFLOW_BUILD_TESTS}")
message(STATUS "Build examples: ${PERFLOW_BUILD_EXAMPLES}")
message(STATUS "Sanitizers:     ${PERFLOW_ENABLE_SANITIZERS}")
message(STATUS "")
