# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# PMU Sampler Demo Example
add_executable(pmu_sampler_demo pmu_sampler_demo.cpp)
target_link_libraries(pmu_sampler_demo PRIVATE perflow_sampling)

# Ensure frame pointers are preserved for stack unwinding and add debug symbols
target_compile_options(pmu_sampler_demo PRIVATE -g -fno-omit-frame-pointer)

# Offset Converter Example
add_executable(offset_converter_example offset_converter_example.cpp)
target_link_libraries(offset_converter_example PRIVATE perflow_sampling)
target_compile_options(offset_converter_example PRIVATE -g)

# Post Analysis Example
add_executable(post_analysis_example post_analysis_example.cpp)
target_link_libraries(post_analysis_example PRIVATE perflow_sampling perflow_analysis ${CMAKE_DL_LIBS})
target_compile_options(post_analysis_example PRIVATE -g)

# Online Analysis Example (Analyzer)
add_executable(online_analysis_example online_analysis_example.cpp)
target_link_libraries(online_analysis_example PRIVATE perflow_sampling perflow_analysis ${CMAKE_DL_LIBS})
target_compile_options(online_analysis_example PRIVATE -g)

# Symbol Resolution Example
add_executable(symbol_resolution_example symbol_resolution_example.cpp)
target_link_libraries(symbol_resolution_example PRIVATE perflow_sampling perflow_analysis ${CMAKE_DL_LIBS})
# Add debug symbols (-g) and preserve frame pointers for symbol resolution
target_compile_options(symbol_resolution_example PRIVATE -g -fno-omit-frame-pointer)

# NPB Hotspot Analyzer for NPB Benchmark Tests
add_executable(npb_hotspot_analyzer npb_hotspot_analyzer.cpp)
target_link_libraries(npb_hotspot_analyzer PRIVATE perflow_sampling perflow_analysis ${CMAKE_DL_LIBS})
target_compile_options(npb_hotspot_analyzer PRIVATE -g)

# Parallel Tree Building Example (demonstrating concurrency models)
add_executable(parallel_tree_building_example parallel_tree_building_example.cpp)
target_link_libraries(parallel_tree_building_example PRIVATE perflow_sampling perflow_analysis ${CMAKE_DL_LIBS})
target_compile_options(parallel_tree_building_example PRIVATE -g)

# Set output directory for examples
set_target_properties(
    pmu_sampler_demo
    offset_converter_example
    post_analysis_example
    online_analysis_example
    symbol_resolution_example
    npb_hotspot_analyzer
    parallel_tree_building_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)
