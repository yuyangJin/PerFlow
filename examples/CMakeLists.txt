# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# PMU Sampler Demo Example
add_executable(pmu_sampler_demo pmu_sampler_demo.cpp)
target_link_libraries(pmu_sampler_demo PRIVATE perflow_sampling)

# Ensure frame pointers are preserved for stack unwinding
target_compile_options(pmu_sampler_demo PRIVATE -fno-omit-frame-pointer)

# Offset Converter Example
add_executable(offset_converter_example offset_converter_example.cpp)
target_link_libraries(offset_converter_example PRIVATE perflow_sampling)

# Post Analysis Example
add_executable(post_analysis_example post_analysis_example.cpp)
target_link_libraries(post_analysis_example PRIVATE perflow_sampling)

# Online Analysis Example (Analyzer)
add_executable(online_analysis_example online_analysis_example.cpp)
target_link_libraries(online_analysis_example PRIVATE perflow_sampling)

# Set output directory for examples
set_target_properties(
    pmu_sampler_demo
    offset_converter_example
    post_analysis_example
    online_analysis_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)
