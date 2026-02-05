# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

cmake_minimum_required(VERSION 3.14)

project(PerFlow
    VERSION 0.1.0
    DESCRIPTION "A programmable and fast performance analysis for parallel programs"
    LANGUAGES CXX Fortran
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
option(PERFLOW_BUILD_EXAMPLES "Build example programs" ON)
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

# ============================================================================
# Analysis Library (with symbol resolution)
# ============================================================================

add_library(perflow_analysis INTERFACE)

target_include_directories(perflow_analysis
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(perflow_analysis
    INTERFACE
    perflow_sampling
    ${CMAKE_DL_LIBS}  # For dlopen, dladdr, etc.
)

# ============================================================================
# MPI Sampler Library
# ============================================================================

# Find PAPI library
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PAPI QUIET papi)
endif()

# Find MPI
find_package(MPI QUIET)

# Only build MPI sampler if both PAPI and MPI are available
if(PAPI_FOUND AND MPI_FOUND)
    message(STATUS "PAPI and MPI found - building MPI sampler")
    
    # Build MPI sampler as a shared library for LD_PRELOAD
    add_library(perflow_mpi_sampler SHARED
        ${CMAKE_SOURCE_DIR}/src/sampler/mpi_sampler.cpp
        ${CMAKE_SOURCE_DIR}/src/sampler/mpi_init_setup.cpp
    )
    
    target_include_directories(perflow_mpi_sampler
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${PAPI_INCLUDE_DIRS}
        ${MPI_CXX_INCLUDE_DIRS}
    )
    
    target_link_libraries(perflow_mpi_sampler
        PRIVATE
        perflow_sampling
        ${PAPI_LIBRARIES}
        ${MPI_CXX_LIBRARIES}
        unwind
        dl
    )
    
    # Add PAPI library directories
    if(PAPI_LIBRARY_DIRS)
        target_link_directories(perflow_mpi_sampler PRIVATE ${PAPI_LIBRARY_DIRS})
    endif()
    
    # Set output directory
    set_target_properties(perflow_mpi_sampler PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
    
else()
    if(NOT PAPI_FOUND)
        message(STATUS "PAPI not found - skipping MPI sampler build")
        message(STATUS "  Install PAPI: sudo apt-get install libpapi-dev")
    endif()
    if(NOT MPI_FOUND)
        message(STATUS "MPI not found - skipping MPI sampler build")
        message(STATUS "  Install MPI: sudo apt-get install libopenmpi-dev")
    endif()
endif()

# ============================================================================
# Timer-Based MPI Sampler Library (PAPI-independent)
# ============================================================================

# Build timer-based MPI sampler if MPI is available
# This sampler doesn't require PAPI and uses POSIX timers instead
if(MPI_FOUND)
    message(STATUS "MPI found - building timer-based MPI sampler")
    
    # Build timer-based MPI sampler as a shared library for LD_PRELOAD
    add_library(perflow_mpi_sampler_timer SHARED
        ${CMAKE_SOURCE_DIR}/src/sampler/mpi_sampler_timer.cpp
        ${CMAKE_SOURCE_DIR}/src/sampler/mpi_init_setup.cpp
    )
    
    target_include_directories(perflow_mpi_sampler_timer
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${MPI_CXX_INCLUDE_DIRS}
    )
    
    target_link_libraries(perflow_mpi_sampler_timer
        PRIVATE
        perflow_sampling
        ${MPI_CXX_LIBRARIES}
        unwind
        dl
        rt  # Required for POSIX timers
    )
    
    # Set output directory
    set_target_properties(perflow_mpi_sampler_timer PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
    
else()
    message(STATUS "MPI not found - skipping timer-based MPI sampler build")
endif()

# Tests
if(PERFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(PERFLOW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Python bindings (optional)
add_subdirectory(python)

# Installation
include(GNUInstallDirs)

install(TARGETS perflow_sampling perflow_analysis
    EXPORT PerFlowTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/sampling
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY include/analysis
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
