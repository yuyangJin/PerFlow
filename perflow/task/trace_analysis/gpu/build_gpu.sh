#!/bin/bash
# Build script for PerFlow GPU components
#
# This script compiles the CUDA kernels into a shared library
# that can be loaded by Python.
#
# Requirements:
# - CUDA Toolkit (nvcc compiler)
# - CMake >= 3.18
#
# Usage:
#   ./build_gpu.sh [--clean] [--arch=<cuda_arch>]
#
# Options:
#   --clean         Clean build directory before building
#   --arch=<arch>   Specify CUDA architecture (e.g., sm_75 for RTX 20 series)
#                   Default: auto-detect or use sm_60,sm_70,sm_75,sm_80,sm_86

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_DIR="build"
CLEAN_BUILD=false
CUDA_ARCH=""

# Parse arguments
for arg in "$@"; do
    case $arg in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --arch=*)
            CUDA_ARCH="${arg#*=}"
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $arg${NC}"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}PerFlow GPU Build Script${NC}"
echo "================================"

# Check for CUDA
if ! command -v nvcc &> /dev/null; then
    echo -e "${RED}Error: nvcc not found. Please install CUDA Toolkit.${NC}"
    echo "Download from: https://developer.nvidia.com/cuda-downloads"
    exit 1
fi

echo -e "${GREEN}✓${NC} Found CUDA compiler: $(nvcc --version | grep release)"

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: cmake not found. Please install CMake >= 3.18.${NC}"
    exit 1
fi

echo -e "${GREEN}✓${NC} Found CMake: $(cmake --version | head -n1)"

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Configuring build...${NC}"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release"

if [ -n "$CUDA_ARCH" ]; then
    echo "Using CUDA architecture: $CUDA_ARCH"
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CUDA_ARCHITECTURES=$CUDA_ARCH"
fi

cmake .. $CMAKE_ARGS

# Build
echo -e "${YELLOW}Building GPU library...${NC}"
cmake --build . --config Release -j$(nproc 2>/dev/null || echo 4)

# Check if library was built
LIBRARY_NAME="libtrace_replay_gpu.so"
if [[ "$OSTYPE" == "darwin"* ]]; then
    LIBRARY_NAME="libtrace_replay_gpu.dylib"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    LIBRARY_NAME="trace_replay_gpu.dll"
fi

if [ -f "$LIBRARY_NAME" ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo "Library: $SCRIPT_DIR/$BUILD_DIR/$LIBRARY_NAME"
    
    # Copy to parent directory for Python to find
    cp "$LIBRARY_NAME" "$SCRIPT_DIR/"
    echo -e "${GREEN}✓ Library copied to: $SCRIPT_DIR/$LIBRARY_NAME${NC}"
else
    echo -e "${RED}✗ Build failed: library not found${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}GPU build complete!${NC}"
echo ""
echo "Next steps:"
echo "  1. Run tests: pytest tests/unit/test_gpu_trace_analysis.py"
echo "  2. Try examples: python tests/examples/example_gpu_late_sender_analysis.py"
