# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# PMU Sampler Demo Example
add_executable(pmu_sampler_demo pmu_sampler_demo.cpp)
target_link_libraries(pmu_sampler_demo PRIVATE perflow_sampling)

# Ensure frame pointers are preserved for stack unwinding
target_compile_options(pmu_sampler_demo PRIVATE -fno-omit-frame-pointer)
