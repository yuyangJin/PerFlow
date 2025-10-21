# CMake build configuration for PerFlow GPU components
cmake_minimum_required(VERSION 3.18)
project(perflow_gpu LANGUAGES CXX CUDA)

# Set C++ and CUDA standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_SEPARABLE_COMPILATION ON)

# Find CUDA toolkit
find_package(CUDAToolkit REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(CUDA_SOURCES
    trace_replay_kernel.cu
)

set(HEADER_FILES
    event_array.h
)

# Build shared library for Python binding
add_library(trace_replay_gpu SHARED ${CUDA_SOURCES})

# Set CUDA architecture (adjust based on your GPU)
# Common architectures:
# - sm_60: Pascal (GTX 10 series, P100)
# - sm_70: Volta (V100)
# - sm_75: Turing (RTX 20 series)
# - sm_80: Ampere (A100, RTX 30 series)
# - sm_86: Ampere (RTX 30 series mobile)
# - sm_89: Ada Lovelace (RTX 40 series)
set_target_properties(trace_replay_gpu PROPERTIES 
    CUDA_ARCHITECTURES "60;70;75;80;86"
    POSITION_INDEPENDENT_CODE ON
)

# Link CUDA runtime
target_link_libraries(trace_replay_gpu CUDA::cudart)

# Installation
install(TARGETS trace_replay_gpu 
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/perflow/task/trace_analysis/gpu
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/perflow/task/trace_analysis/gpu
)

# Optional: Build tests if requested
option(BUILD_TESTS "Build GPU tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    # Add test executables here
endif()
