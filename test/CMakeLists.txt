# Copyright 2024 PerFlow Authors
# Licensed under the MIT License

cmake_minimum_required(VERSION 3.14)

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
  message(STATUS "Found PAPI: ${PAPI_LIBRARY}")
else()
  message(WARNING "PAPI not found. MPI sampler tests will not be built.")
  set(PAPI_FOUND FALSE)
endif()

# Only build tests if PAPI is found
if(PAPI_FOUND)
  # Include directories
  include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${PAPI_INCLUDE_DIR}
    ${MPI_CXX_INCLUDE_PATH}
  )

  # MPI Sampler Test executable
  add_executable(mpi_sampler_test
    mpi_sampler_test.cpp
    ${CMAKE_SOURCE_DIR}/src/sampler/mpi_sampler.cpp
    ${CMAKE_SOURCE_DIR}/src/sampler/call_stack.cpp
    ${CMAKE_SOURCE_DIR}/src/sampler/data_collection.cpp
  )

  # Link libraries
  target_link_libraries(mpi_sampler_test
    ${PAPI_LIBRARY}
    ${MPI_CXX_LIBRARIES}
    Threads::Threads
    dl
  )

  # Set compile flags
  target_compile_features(mpi_sampler_test PRIVATE cxx_std_14)
  target_compile_options(mpi_sampler_test PRIVATE -Wall -Wextra -O2)

  # Add MPI compile flags
  if(MPI_CXX_COMPILE_FLAGS)
    target_compile_options(mpi_sampler_test PRIVATE ${MPI_CXX_COMPILE_FLAGS})
  endif()

  # Add MPI link flags
  if(MPI_CXX_LINK_FLAGS)
    set_target_properties(mpi_sampler_test PROPERTIES
      LINK_FLAGS "${MPI_CXX_LINK_FLAGS}"
    )
  endif()

  # Add test target to run with 4 MPI processes
  add_custom_target(run_mpi_sampler_test
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
            ${MPIEXEC_PREFLAGS} $<TARGET_FILE:mpi_sampler_test> ${MPIEXEC_POSTFLAGS}
    DEPENDS mpi_sampler_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running MPI sampler test with 4 processes"
  )

  # Enable testing
  enable_testing()
  add_test(
    NAME mpi_sampler_test
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
            ${MPIEXEC_PREFLAGS} $<TARGET_FILE:mpi_sampler_test> ${MPIEXEC_POSTFLAGS}
  )

  # Set test properties
  set_tests_properties(mpi_sampler_test PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "OMP_NUM_THREADS=1"
  )
endif()
