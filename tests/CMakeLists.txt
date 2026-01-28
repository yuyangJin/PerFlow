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
    test_library_map.cpp
    test_offset_converter.cpp
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
# MPI Sampler Test
# ============================================================================

# Find MPI (needed for test executable)
find_package(MPI QUIET)

# Only build MPI sampler test if MPI is available and sampler was built
if(TARGET perflow_mpi_sampler AND MPI_FOUND)
    message(STATUS "Building MPI sampler test")
    
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
    
endif()

