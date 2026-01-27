# Copyright 2024 PerFlow Authors
# Licensed under the MIT License

cmake_minimum_required(VERSION 3.14)

# Find required packages
find_package(MPI REQUIRED)

# Include directories
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${MPI_CXX_INCLUDE_PATH}
)

# MPI Test Program - A simple MPI workload for testing dynamic instrumentation
# This program does NOT link against the sampler - it is instrumented via LD_PRELOAD
add_executable(mpi_sampler_test
  mpi_sampler_test.cpp
)

# Link libraries (only MPI, no PAPI or sampler - instrumentation is via LD_PRELOAD)
target_link_libraries(mpi_sampler_test
  ${MPI_CXX_LIBRARIES}
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

# Add test target to run with LD_PRELOAD dynamic instrumentation
# This shows how to use the sampler with any MPI program
# Only available when the sampler library was built
if(TARGET perflow_sampler)
  add_custom_target(run_mpi_sampler_test
    COMMAND ${CMAKE_COMMAND} -E env 
            "PERFLOW_ENABLE_SAMPLING=1"
            "PERFLOW_SAMPLING_FREQ=1000"
            "PERFLOW_OUTPUT_PATH=${CMAKE_CURRENT_BINARY_DIR}/samples"
            "LD_PRELOAD=$<TARGET_FILE:perflow_sampler>"
            ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
            ${MPIEXEC_PREFLAGS} $<TARGET_FILE:mpi_sampler_test> ${MPIEXEC_POSTFLAGS}
    DEPENDS mpi_sampler_test perflow_sampler
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running MPI test with dynamic instrumentation (LD_PRELOAD)"
  )

  # Add a target to run without instrumentation for comparison
  add_custom_target(run_mpi_test_uninstrumented
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
            ${MPIEXEC_PREFLAGS} $<TARGET_FILE:mpi_sampler_test> ${MPIEXEC_POSTFLAGS}
    DEPENDS mpi_sampler_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running MPI test without instrumentation"
  )
endif()

# Enable testing
enable_testing()

# Test without instrumentation (basic functionality)
add_test(
  NAME mpi_test_basic
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
          ${MPIEXEC_PREFLAGS} $<TARGET_FILE:mpi_sampler_test> ${MPIEXEC_POSTFLAGS}
)

set_tests_properties(mpi_test_basic PROPERTIES
  TIMEOUT 60
  ENVIRONMENT "OMP_NUM_THREADS=1"
)

# Test with LD_PRELOAD instrumentation (requires sampler library)
if(TARGET perflow_sampler)
  add_test(
    NAME mpi_test_instrumented
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
            ${MPIEXEC_PREFLAGS} $<TARGET_FILE:mpi_sampler_test> ${MPIEXEC_POSTFLAGS}
  )

  set_tests_properties(mpi_test_instrumented PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "PERFLOW_ENABLE_SAMPLING=1;PERFLOW_SAMPLING_FREQ=1000;PERFLOW_OUTPUT_PATH=${CMAKE_CURRENT_BINARY_DIR}/samples;LD_PRELOAD=$<TARGET_FILE:perflow_sampler>;OMP_NUM_THREADS=1"
  )
endif()
