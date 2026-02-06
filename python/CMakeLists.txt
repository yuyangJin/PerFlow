# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# CMakeLists.txt for PerFlow Python bindings

# Find Python
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# Create the Python module
pybind11_add_module(_perflow
    perflow_bindings.cpp
)

# Include directories
target_include_directories(_perflow
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link with threading library
find_package(Threads REQUIRED)
target_link_libraries(_perflow PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

# Set output directory to be alongside the Python package
set_target_properties(_perflow PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/perflow
)

# Install target
install(TARGETS _perflow
    LIBRARY DESTINATION ${Python3_SITEARCH}/perflow
)

# Install Python files
install(FILES perflow/__init__.py
    DESTINATION ${Python3_SITEARCH}/perflow
)
