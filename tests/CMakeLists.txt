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
    test_library_map_export.cpp
    test_performance_tree.cpp
    test_analysis_tasks.cpp
    test_tree_builder.cpp
    test_online_analysis_integration.cpp
    test_symbol_resolver.cpp
    test_offset_converter_symbol_resolution.cpp
)

target_link_libraries(perflow_tests
    PRIVATE
    perflow_sampling
    perflow_analysis
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
        
        # Add debug symbols (-g) for symbol resolution testing
        # This enables the SymbolResolver to resolve function names and source locations
        target_compile_options(${test_prog}
            PRIVATE
            -g
        )
        
        # Set output directory for test executable
        set_target_properties(${test_prog} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
        )
        
        # Determine MPI runner command (prefer srun for SLURM environments, fallback to mpirun/mpiexec)
        # Allow override via MPI_EXEC environment variable or CMake variable
        if(NOT DEFINED MPIEXEC_EXECUTABLE)
            find_program(MPIEXEC_EXECUTABLE NAMES srun)
            if(NOT MPIEXEC_EXECUTABLE)
                # Fallback to mpirun/mpiexec if srun is not available
                find_program(MPIEXEC_EXECUTABLE NAMES mpirun mpiexec)
            endif()
        endif()
        
        if(NOT MPIEXEC_EXECUTABLE)
            message(WARNING "No MPI executor found (srun/mpirun/mpiexec) - MPI tests may not run")
            set(MPIEXEC_EXECUTABLE "mpirun")  # Default fallback
        endif()
        
        message(STATUS "Using MPI executor: ${MPIEXEC_EXECUTABLE}")
        
        # Add custom test target that uses LD_PRELOAD
        add_custom_target(run_${test_prog}
            COMMAND ${CMAKE_COMMAND} -E make_directory 
                    ${CMAKE_BINARY_DIR}/mpi_sampler_output/${test_prog}
            COMMAND ${CMAKE_COMMAND} -E env 
                    LD_PRELOAD=${CMAKE_BINARY_DIR}/lib/libperflow_mpi_sampler.so
                    PERFLOW_OUTPUT_DIR=${CMAKE_BINARY_DIR}/mpi_sampler_output/${test_prog}
                    ${MPIEXEC_EXECUTABLE} -n 4
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
    
    # ========================================================================
    # Fortran MPI Sampler Test
    # ========================================================================
    
    # Check if Fortran compiler is available
    if(CMAKE_Fortran_COMPILER)
        message(STATUS "Fortran compiler found - building Fortran MPI sampler test")
        
        # Find MPI Fortran (may be different from C MPI)
        find_package(MPI COMPONENTS Fortran QUIET)
        
        if(MPI_Fortran_FOUND)
            # Build Fortran test executable
            add_executable(mpi_sampler_fortran_test
                mpi_sampler_fortran_test.f90
            )
            
            target_include_directories(mpi_sampler_fortran_test
                PRIVATE
                ${MPI_Fortran_INCLUDE_DIRS}
            )
            
            target_link_libraries(mpi_sampler_fortran_test
                PRIVATE
                ${MPI_Fortran_LIBRARIES}
            )
            
            # Add debug symbols for symbol resolution
            target_compile_options(mpi_sampler_fortran_test
                PRIVATE
                -g
            )
            
            # Set output directory for test executable
            set_target_properties(mpi_sampler_fortran_test PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
            )
            
            # Add custom test target that uses LD_PRELOAD
            add_custom_target(run_mpi_sampler_fortran_test
                COMMAND ${CMAKE_COMMAND} -E make_directory 
                        ${CMAKE_BINARY_DIR}/mpi_sampler_output/mpi_sampler_fortran_test
                COMMAND ${CMAKE_COMMAND} -E env 
                        LD_PRELOAD=${CMAKE_BINARY_DIR}/lib/libperflow_mpi_sampler.so
                        PERFLOW_OUTPUT_DIR=${CMAKE_BINARY_DIR}/mpi_sampler_output/mpi_sampler_fortran_test
                        ${MPIEXEC_EXECUTABLE} -n 4
                        ${CMAKE_BINARY_DIR}/tests/mpi_sampler_fortran_test
                DEPENDS perflow_mpi_sampler mpi_sampler_fortran_test
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Running Fortran MPI sampler test with 4 processes"
            )
            
            message(STATUS "Fortran MPI test configured successfully")
        else()
            message(STATUS "MPI Fortran not found - skipping Fortran MPI sampler test")
        endif()
    else()
        message(STATUS "Fortran compiler not found - skipping Fortran MPI sampler test")
    endif()
    
endif()

