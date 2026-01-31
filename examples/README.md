# PerFlow Examples

This directory contains example programs demonstrating various features of PerFlow.

## Building Examples

Examples are built when the `PERFLOW_BUILD_EXAMPLES` CMake option is enabled:

```bash
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_EXAMPLES=ON
make -j$(nproc)
```

Built executables will be in `build/examples/`.

## Available Examples

### 1. PMU Sampler Demo (`pmu_sampler_demo`)

Demonstrates low-level PMU sampling functionality.

**Purpose**: Shows how to use the PMU sampler API directly.

**Usage**:
```bash
./build/examples/pmu_sampler_demo
```

### 2. Offset Converter Example (`offset_converter_example`)

Demonstrates library mapping and address-to-offset conversion.

**Purpose**: Shows how to capture library maps and convert raw addresses to (library, offset) pairs.

**Usage**:
```bash
./build/examples/offset_converter_example
```

**What it does**:
- Captures the current process's library map
- Simulates call stack addresses
- Converts addresses to resolved frames

### 3. Post Analysis Example (`post_analysis_example`)

Demonstrates post-processing of collected sample data.

**Purpose**: Shows how to import and analyze sample data after collection.

**Usage**:
```bash
# Assuming you have sample data from MPI sampler
./build/examples/post_analysis_example
```

**What it does**:
- Loads library maps (.libmap files)
- Imports call stack data (.pflw files)
- Converts raw addresses to resolved frames
- Analyzes hotness by library
- Identifies hot call paths

### 4. Online Analysis Example (`online_analysis_example`) - **Analyzer**

**NEW**: Comprehensive analyzer demonstrating the online analysis module.

**Purpose**: Complete workflow for analyzing MPI application performance data.

**Usage**:
```bash
./build/examples/online_analysis_example <data_directory> [output_directory]
```

**Example**:
```bash
# First, collect data with MPI sampler
LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp/perflow_samples \
mpirun -n 4 ./your_mpi_app

# Then analyze the collected data
./build/examples/online_analysis_example /tmp/perflow_samples /tmp/analysis_output
```

**What it does**:
- Builds performance trees from sample data
- Performs workload balance analysis across processes
- Identifies performance hotspots (inclusive and exclusive time)
- Exports analysis results:
  - Text tree representation
  - Binary tree format
  - PDF visualization (if GraphViz is installed)

**Output**:
- `performance_tree.ptree.txt` - Human-readable tree
- `performance_tree.ptree` - Binary format for further processing
- `performance_tree.pdf` - Visual tree diagram (requires GraphViz)

## Typical Workflow

### 1. Data Collection Phase

Run your MPI application with the PerFlow sampler:

```bash
LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp/perflow_samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 4 ./your_mpi_app
```

This generates:
- `perflow_mpi_rank_0.pflw`, `perflow_mpi_rank_1.pflw`, ... (sample data)
- `perflow_mpi_rank_0.libmap`, `perflow_mpi_rank_1.libmap`, ... (library maps)
- `perflow_mpi_rank_0.txt`, `perflow_mpi_rank_1.txt`, ... (human-readable)

### 2. Analysis Phase

Use the online analysis example:

```bash
./build/examples/online_analysis_example /tmp/perflow_samples /tmp/analysis_output
```

**Console Output**:
```
PerFlow Online Analysis Example
================================

Building performance tree from sample data...
  Sample files loaded: 4
  Successfully loaded: 4 files
  Total samples: 15234
  Process count: 4

Balance Analysis
----------------
  Mean samples per process: 3808.5
  Std deviation: 152.3
  Min samples: 3642 (process 2)
  Max samples: 3991 (process 0)
  Imbalance factor: 0.092

Top 10 Hotspots (Total Time)
----------------------------
  1. compute_kernel (libmyapp.so)
     Total: 8234 samples (54.0%)
     Self:  6123 samples (40.2%)
     Location: compute.cpp:45

  2. MPI_Allreduce (libmpi.so)
     Total: 3421 samples (22.4%)
     Self:  3421 samples (22.4%)

  ...

Exporting results...
  Tree (text): /tmp/analysis_output/performance_tree.ptree.txt
  Tree (binary): /tmp/analysis_output/performance_tree.ptree
  Visualization: /tmp/performance_tree.pdf

Analysis complete!
```

### 3. Viewing Results

**Text format**:
```bash
less /tmp/analysis_output/performance_tree.ptree.txt
```

**PDF visualization**:
```bash
xdg-open /tmp/performance_tree.pdf  # Linux
# or
open /tmp/performance_tree.pdf      # macOS
```

## Requirements

### Basic Requirements
- C++17 compiler
- CMake 3.14+
- zlib (optional, for compression)

### For MPI Sampling
- MPI (OpenMPI, MPICH, or Intel MPI)
- PAPI library
- libunwind

### For Visualization
- GraphViz (`dot` command) for PDF generation

**Installing GraphViz**:
```bash
# Ubuntu/Debian
sudo apt-get install graphviz

# macOS
brew install graphviz
```

## Troubleshooting

### Example doesn't build
Make sure you enabled examples during CMake configuration:
```bash
cmake .. -DPERFLOW_BUILD_EXAMPLES=ON
```

### No sample files found
The online analysis example expects .pflw files in the data directory. Make sure you've run an MPI application with the sampler first.

### PDF generation fails
Install GraphViz:
```bash
sudo apt-get install graphviz
```

Or the example will skip PDF generation and only create text/binary outputs.

## See Also

- [Main README](../README.md) - Project overview
- [API Documentation](../docs/ONLINE_ANALYSIS_API.md) - Complete API reference
- [Testing Guide](../TESTING.md) - Testing documentation
