# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# Download and configure Google Test
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY git@github.com:google/googletest.git
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
