# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
setup.py for PerFlow Python bindings

Installation:
    pip install .
    
Development installation:
    pip install -e .
"""

import os
import sys
import subprocess
from pathlib import Path

from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    """CMake extension for pybind11 modules."""
    
    def __init__(self, name, sourcedir=''):
        super().__init__(name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    """Build extension using CMake."""
    
    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        
        print(f"Building CMake extension {ext.name} in {extdir}")

        # Required for auto-detection of auxiliary "native" libs
        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep
        
        cmake_args = [
            f'-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}/perflow',
            f'-DCMAKE_BUILD_TYPE=Release',
            f'-DPERFLOW_BUILD_PYTHON=ON',
            f'-DPERFLOW_BUILD_TESTS=OFF',
        ]
        
        build_args = ['--config', 'Release']
        
        # Use parallel build
        if 'CMAKE_BUILD_PARALLEL_LEVEL' not in os.environ:
            build_args += ['--', '-j4']
        
        build_temp = Path(self.build_temp) / ext.name
        build_temp.mkdir(parents=True, exist_ok=True)
        
        subprocess.check_call(
            ['cmake', ext.sourcedir] + cmake_args,
            cwd=build_temp
        )
        subprocess.check_call(
            ['cmake', '--build', '.'] + build_args,
            cwd=build_temp
        )
        subprocess.check_call(
            ['cmake', '--install', '.'],
            cwd=build_temp
        )


# Read long description from README
long_description = """
# PerFlow Python Bindings

Python bindings for the PerFlow performance analysis library.

## Features

- Build performance trees from sample data files
- Analyze workload balance across processes  
- Identify performance hotspots (both inclusive and exclusive time)
- Tree traversal and filtering APIs
- Parallel file reading support

## Quick Start

```python
import perflow

# Build tree from sample files
builder = perflow.TreeBuilder()
builder.build_from_files([('rank_0.pflw', 0), ('rank_1.pflw', 1)])
tree = builder.tree

# Find hotspots
hotspots = perflow.HotspotAnalyzer.find_hotspots(tree, top_n=10)
for h in hotspots:
    print(f"{h.function_name}: {h.self_percentage:.1f}%")

# Analyze balance
balance = perflow.BalanceAnalyzer.analyze(tree)
print(f"Imbalance factor: {balance.imbalance_factor:.2f}")
```
"""

setup(
    name='perflow',
    version='0.1.0',
    author='PerFlow Authors',
    author_email='',
    description='Python bindings for PerFlow performance analysis library',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/yuyangJin/PerFlow',
    packages=find_packages(),
    ext_modules=[CMakeExtension('_perflow', sourcedir='.')],
    cmdclass={'build_ext': CMakeBuild},
    python_requires='>=3.7',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: C++',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Topic :: Software Development :: Libraries',
        'Topic :: System :: Monitoring',
    ],
    keywords='performance analysis profiling HPC MPI parallel',
)
