# Getting Started with PerFlow

This guide will help you install, configure, and run your first performance analysis with PerFlow.

## What is PerFlow?

PerFlow is a high-performance profiling tool designed for parallel MPI applications. It provides:

- **Ultra-low overhead** sampling (<0.5% @ 1kHz)
- **Two sampling modes**: Hardware PMU (PAPI) and Timer-based (POSIX)
- **Symbol resolution** for function names and source locations
- **Online analysis** with real-time performance tree generation
- **Workload balance analysis** across MPI processes
- **Hotspot detection** for performance bottlenecks
- **Tree visualization** with customizable PDF generation

## Prerequisites

### Required Dependencies
- **C++17 compiler** (GCC 7+, Clang 5+, or equivalent)
- **CMake 3.14+**
- **MPI** (OpenMPI, MPICH, or Intel MPI)
- **libunwind** - For call stack capture

### Optional Dependencies
- **PAPI library** - For hardware PMU sampling (highly recommended for lowest overhead)
- **zlib** - For compressed sample data (recommended)
- **GraphViz** - For PDF visualization generation

### Installing Dependencies

#### Ubuntu/Debian
```bash
# Required
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libopenmpi-dev \
    libunwind-dev

# Optional but recommended
sudo apt-get install -y \
    libpapi-dev \
    zlib1g-dev \
    graphviz
```

#### RHEL/CentOS/Fedora
```bash
# Required
sudo dnf install -y \
    gcc-c++ \
    cmake \
    openmpi-devel \
    libunwind-devel

# Optional but recommended
sudo dnf install -y \
    papi-devel \
    zlib-devel \
    graphviz
```

#### macOS
```bash
brew install cmake openmpi libunwind
brew install papi graphviz  # Optional
```

## Installation

### 1. Clone the Repository
```bash
git clone https://github.com/yuyangJin/PerFlow.git
cd PerFlow
```

### 2. Build PerFlow
```bash
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON -DPERFLOW_BUILD_EXAMPLES=ON
make -j$(nproc)
```

#### Build Options
- `PERFLOW_BUILD_TESTS=ON` - Build test suite (default: ON)
- `PERFLOW_BUILD_EXAMPLES=ON` - Build example programs (default: ON)
- `PERFLOW_ENABLE_SANITIZERS=ON` - Enable address/UB sanitizers for debugging (default: OFF)

### 3. Verify Installation
```bash
# Run unit tests
ctest

# Check built libraries
ls -lh lib/
# You should see:
# - libperflow_mpi_sampler.so (if PAPI found)
# - libperflow_mpi_sampler_timer.so (if MPI found)
```

## Quick Start

### Step 1: Choose Your Sampling Mode

PerFlow offers two sampling modes:

#### Option A: Hardware PMU Sampler (Recommended)
- **Pros**: Ultra-low overhead (<0.5%), cycle-accurate
- **Cons**: Requires PAPI, may need root/CAP_SYS_ADMIN for PMU access
- **Use when**: Running on bare metal with PMU access

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
mpirun -n 4 ./your_mpi_app
```

#### Option B: Timer-Based Sampler (Platform-Independent)
- **Pros**: Works everywhere (containers, VMs), no special privileges
- **Cons**: Slightly higher overhead (~1-2%)
- **Use when**: No PMU access or running in containers/VMs

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 4 ./your_mpi_app
```

### Step 2: Profile Your Application

Create a sample directory and run your MPI application with PerFlow:

```bash
# Create output directory
mkdir -p /tmp/perflow_samples

# Run your application with PerFlow (using timer sampler)
cd build
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/perflow_samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 4 ../examples/simple_mpi_app

# Check generated sample files
ls -lh /tmp/perflow_samples/
# You should see: rank_0.pflw, rank_1.pflw, rank_2.pflw, rank_3.pflw
```

### Step 3: Analyze Results

Use the online analysis example to process sample data:

```bash
# Analyze samples and generate visualization
./examples/online_analysis_example \
    /tmp/perflow_samples \
    /tmp/perflow_output

# View results
ls -lh /tmp/perflow_output/
# - performance_tree.pdf - Visual tree representation
# - balance_report.txt - Workload balance analysis
# - hotspots.txt - Top performance bottlenecks
```

## Configuration Options

### Environment Variables

#### Required
- `PERFLOW_OUTPUT_DIR` - Directory to store sample data files

#### Optional for Timer Sampler
- `PERFLOW_SAMPLING_FREQ` - Sampling frequency in Hz (default: 1000, range: 100-10000)

#### Optional for All Samplers
- `PERFLOW_MAX_STACK_DEPTH` - Maximum call stack depth (default: 128)
- `PERFLOW_ENABLE_COMPRESSION` - Enable zlib compression (default: 0, set to 1 to enable)

### Example: High-Frequency Sampling
```bash
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=5000 \
PERFLOW_MAX_STACK_DEPTH=64 \
mpirun -n 8 ./your_app
```

## Next Steps

- **[Configuration Guide](CONFIGURATION.md)** - Detailed configuration options
- **[Timer Sampler Usage](../TIMER_SAMPLER_USAGE.md)** - In-depth timer sampler guide
- **[Online Analysis API](../ONLINE_ANALYSIS_API.md)** - Programmatic analysis
- **[Examples](../../examples/README.md)** - More code examples
- **[Troubleshooting](TROUBLESHOOTING.md)** - Common issues and solutions

## Example Workflow

Here's a complete example workflow:

```bash
# 1. Build PerFlow
cd PerFlow
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_EXAMPLES=ON
make -j$(nproc)

# 2. Profile an example application
mkdir -p /tmp/perflow_demo
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/perflow_demo \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 4 ./examples/matrix_multiply

# 3. Analyze with online analysis
./examples/online_analysis_example /tmp/perflow_demo /tmp/perflow_results

# 4. View results
cat /tmp/perflow_results/balance_report.txt
cat /tmp/perflow_results/hotspots.txt
# Open /tmp/perflow_results/performance_tree.pdf in a PDF viewer
```

## Tips for Effective Profiling

💡 **Start with 1kHz sampling** - Good balance between overhead and accuracy

💡 **Use timer sampler first** - Easier to set up, works everywhere

💡 **Profile representative workloads** - Run long enough to capture meaningful data (at least 10 seconds)

💡 **Check for imbalance** - High imbalance factors indicate load distribution issues

⚠️ **Avoid very high frequencies** - >5kHz can increase overhead significantly

⚠️ **Check disk space** - Sample files can be large for long-running applications

## Getting Help

- **Documentation**: Browse the [docs](../README.md) directory
- **Examples**: Check [examples](../../examples/README.md) for working code
- **Issues**: Report bugs on [GitHub Issues](https://github.com/yuyangJin/PerFlow/issues)
- **Testing**: See [TESTING.md](../../TESTING.md) for validation procedures
