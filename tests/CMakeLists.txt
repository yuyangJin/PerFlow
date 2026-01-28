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
# MPI Sampler Test
# ============================================================================

# Find MPI (needed for test executable)
find_package(MPI QUIET)

# Only build MPI sampler test if MPI is available and sampler was built
if(TARGET perflow_mpi_sampler AND MPI_FOUND)
    message(STATUS "Building MPI sampler tests")
    
    # List of all MPI test programs
    set(MPI_TEST_PROGRAMS
        mpi_sampler_test
        test_compute_intensive
        test_comm_intensive
        test_memory_intensive
        test_hybrid
        test_stress
    )
    
    # Build each MPI test executable
    foreach(test_prog ${MPI_TEST_PROGRAMS})
        add_executable(${test_prog}
            ${test_prog}.cpp
        )
        
        target_include_directories(${test_prog}
            PRIVATE
            ${MPI_CXX_INCLUDE_DIRS}
        )
        
        target_link_libraries(${test_prog}
            PRIVATE
            ${MPI_CXX_LIBRARIES}
        )
        
        # Set output directory for test executable
        set_target_properties(${test_prog} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
        )
        
        # Add custom test target that uses LD_PRELOAD
        add_custom_target(run_${test_prog}
            COMMAND ${CMAKE_COMMAND} -E env 
                    LD_PRELOAD=${CMAKE_BINARY_DIR}/lib/libperflow_mpi_sampler.so
                    PERFLOW_OUTPUT_DIR=/tmp
                    ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 
                    ${CMAKE_BINARY_DIR}/tests/${test_prog}
            DEPENDS perflow_mpi_sampler ${test_prog}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running ${test_prog} with 4 processes"
        )
    endforeach()
    
    # Add a target to run all MPI tests
    add_custom_target(run_all_mpi_tests
        DEPENDS ${MPI_TEST_PROGRAMS}
        COMMENT "Running all MPI sampler tests"
    )
    
    # Add individual test runs as dependencies
    foreach(test_prog ${MPI_TEST_PROGRAMS})
        add_dependencies(run_all_mpi_tests run_${test_prog})
    endforeach()
    
endif()

