#!/usr/bin/env python3
"""
Setup script for PerFlow Python frontend.

Note: This is a lightweight setup for development and distribution.
The actual C++ bindings are built via CMake.
"""

from setuptools import setup, find_packages
import os

# Read README
with open(os.path.join(os.path.dirname(__file__), 'README.md'), 'r') as f:
    long_description = f.read()

setup(
    name='perflow',
    version='0.1.0',
    description='Python frontend for PerFlow performance analysis',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='Yuyang Jin',
    url='https://github.com/yuyangJin/PerFlow',
    license='Apache License 2.0',
    
    packages=find_packages(),
    
    python_requires='>=3.7',
    
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: Apache Software License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: C++',
        'Topic :: Software Development :: Libraries',
        'Topic :: System :: Monitoring',
        'Topic :: System :: Benchmark',
    ],
    
    keywords='performance profiling analysis mpi parallel hpc',
    
    project_urls={
        'Bug Reports': 'https://github.com/yuyangJin/PerFlow/issues',
        'Source': 'https://github.com/yuyangJin/PerFlow',
    },
)
