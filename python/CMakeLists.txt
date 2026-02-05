# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

cmake_minimum_required(VERSION 3.14)

# Python bindings module
project(perflow_python_bindings)

# Find Python and pybind11
find_package(Python3 COMPONENTS Interpreter Development QUIET)

if(Python3_FOUND)
    message(STATUS "Python ${Python3_VERSION} found - building Python bindings")
    
    # Add pybind11 subdirectory
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/pybind11 ${CMAKE_BINARY_DIR}/pybind11)
    
    # Create Python bindings module
    pybind11_add_module(_perflow_bindings
        perflow_bindings/bindings.cpp
    )
    
    # Link against perflow libraries
    target_link_libraries(_perflow_bindings
        PRIVATE
        perflow_analysis
        perflow_sampling
    )
    
    # Set output directory
    set_target_properties(_perflow_bindings PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/python/perflow
    )
    
    # Install Python module
    install(TARGETS _perflow_bindings
        LIBRARY DESTINATION ${Python3_SITEARCH}/perflow
    )
    
else()
    message(STATUS "Python development libraries not found - skipping Python bindings")
    message(STATUS "  Install Python development libraries:")
    message(STATUS "    Ubuntu/Debian: sudo apt-get install python3-dev")
    message(STATUS "    RHEL/CentOS:   sudo dnf install python3-devel")
endif()
