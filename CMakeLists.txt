# Copyright 2024 PerFlow Authors
# Licensed under the MIT License

cmake_minimum_required(VERSION 3.14)
project(PerFlow VERSION 2.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(MPI REQUIRED)
find_package(Threads REQUIRED)

# Find PAPI library
find_path(PAPI_INCLUDE_DIR papi.h
  HINTS ENV PAPI_DIR
  PATH_SUFFIXES include
  PATHS /usr /usr/local /opt/papi
)

find_library(PAPI_LIBRARY
  NAMES papi
  HINTS ENV PAPI_DIR
  PATH_SUFFIXES lib lib64
  PATHS /usr /usr/local /opt/papi
)

if(PAPI_INCLUDE_DIR AND PAPI_LIBRARY)
  set(PAPI_FOUND TRUE)
  message(STATUS "Found PAPI include: ${PAPI_INCLUDE_DIR}")
  message(STATUS "Found PAPI library: ${PAPI_LIBRARY}")
else()
  message(WARNING "PAPI not found. Some components may not be built.")
  set(PAPI_FOUND FALSE)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add source subdirectory
add_subdirectory(src/sampler)

# Add test subdirectory
add_subdirectory(test)

# Installation
install(DIRECTORY include/ DESTINATION include)
