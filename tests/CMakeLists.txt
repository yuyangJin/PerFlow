# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# Download and configure Google Test
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(perflow_tests
    test_call_stack.cpp
    test_static_hash_map.cpp
    test_data_export.cpp
    test_pmu_sampler.cpp
)

target_link_libraries(perflow_tests
    PRIVATE
    perflow_sampling
    GTest::gtest
    GTest::gtest_main
)

# Enable testing
include(GoogleTest)
gtest_discover_tests(perflow_tests)

# ============================================================================
# MPI Sampler Library and Test
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
    )
    
    # Add PAPI library directories
    if(PAPI_LIBRARY_DIRS)
        target_link_directories(perflow_mpi_sampler PRIVATE ${PAPI_LIBRARY_DIRS})
    endif()
    
    # Set output directory
    set_target_properties(perflow_mpi_sampler PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
    
    # Build MPI sampler test executable
    add_executable(mpi_sampler_test
        mpi_sampler_test.cpp
    )
    
    target_include_directories(mpi_sampler_test
        PRIVATE
        ${MPI_CXX_INCLUDE_DIRS}
    )
    
    target_link_libraries(mpi_sampler_test
        PRIVATE
        ${MPI_CXX_LIBRARIES}
    )
    
    # Set output directory for test executable
    set_target_properties(mpi_sampler_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    
    # Add custom test target that uses LD_PRELOAD
    add_custom_target(run_mpi_sampler_test
        COMMAND ${CMAKE_COMMAND} -E env 
                LD_PRELOAD=${CMAKE_BINARY_DIR}/lib/libperflow_mpi_sampler.so
                PERFLOW_OUTPUT_DIR=/tmp
                ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 
                ${CMAKE_BINARY_DIR}/tests/mpi_sampler_test
        DEPENDS perflow_mpi_sampler mpi_sampler_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running MPI sampler test with 4 processes"
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
