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

### 3. Post Analysis Example (`post_analysis_example`) - **With PDF Visualization**

Demonstrates post-processing of collected sample data with tree visualization.

**Purpose**: Shows how to import and analyze sample data after collection with PDF visualization output.

**Usage**:
```bash
# Assuming you have sample data from MPI sampler
./build/examples/post_analysis_example [data_directory]
```

**Example**:
```bash
# Use default directory (/tmp)
./build/examples/post_analysis_example

# Or specify custom directory
./build/examples/post_analysis_example /path/to/sample/data
```

**What it does**:
- Loads library maps (.libmap files)
- Imports call stack data (.pflw files)
- Converts raw addresses to resolved frames
- **Builds performance tree from call stacks**
- **Generates PDF visualization of the call tree** 🎨
- Analyzes hotness by library
- Identifies hot call paths

**Output**:
- `performance_tree_rank_N.pdf` - Visual tree diagram (requires GraphViz)
- Library hotness statistics (console output)

### 4. Online Analysis Example (`online_analysis_example`) - **Continuous Monitoring**

**NEW**: Real-time monitoring and analysis demonstrating continuous profiling workflow.

**Purpose**: Demonstrates continuous directory monitoring where the analyzer processes sample data in real-time as MPI applications generate it.

**Usage**:
```bash
./build/examples/online_analysis_example <data_directory> [output_directory]
```

**Two Workflow Modes**:

#### Mode 1: Continuous Monitoring (Real-Time)
This is the primary use case - start the analyzer first, then run your MPI application.

```bash
# Terminal 1: Start the analyzer in monitoring mode
./build/examples/online_analysis_example /tmp/perflow_samples /tmp/analysis_output

# Terminal 2: Run your MPI application (after analyzer is running)
LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp/perflow_samples \
mpirun -n 4 ./your_mpi_app

# The analyzer will automatically detect and process files as they arrive
# Press Ctrl+C in Terminal 1 to stop monitoring and generate final report
```

**What it does**:
- Continuously monitors the specified directory for new .pflw and .libmap files
- Automatically processes files as they are created by the MPI sampler
- Displays real-time updates as samples are collected
- Prints periodic analysis summaries (every 10 seconds)
- Generates live PDF visualization (`performance_tree_live.pdf`)
- Produces comprehensive final report when stopped
- Never stops until you press Ctrl+C (ideal for long-running MPI jobs)

**Console Output (Real-Time)**:
```
PerFlow Online Analysis - Continuous Monitoring Mode
====================================================

Monitored directory: /tmp/perflow_samples
Output directory: /tmp/analysis_output
Press Ctrl+C to stop monitoring

Monitoring started. Waiting for sample files...

Tip: Run your MPI application now with:
  LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \
  PERFLOW_OUTPUT_DIR=/tmp/perflow_samples \
  mpirun -n <N> ./your_mpi_app

[1738342567] Loaded library map: /tmp/perflow_samples/perflow_mpi_rank_0.libmap
[1738342567] Loaded library map: /tmp/perflow_samples/perflow_mpi_rank_1.libmap
[1738342568] Processed sample file: /tmp/perflow_samples/perflow_mpi_rank_0.pflw
  Total samples so far: 180
[1738342568] Processed sample file: /tmp/perflow_samples/perflow_mpi_rank_1.pflw
  Total samples so far: 360

========================================
Current Analysis Summary
========================================

Performance Tree Statistics:
  Total samples: 360
  Process count: 2
  Root children: 3

Balance Analysis:
  Mean samples per process: 180.0
  Std deviation: 0.0
  Min samples: 180 (process 0)
  Max samples: 180 (process 1)
  Imbalance factor: 0.000

Top 5 Hotspots (Total Time):
  1. main (libapp.so) - 360 samples (100%)
  2. compute (libapp.so) - 280 samples (77.8%)
  3. MPI_Barrier (libmpi.so) - 80 samples (22.2%)

  Visualization saved: /tmp/analysis_output/performance_tree_live.pdf
========================================

# ...monitoring continues until Ctrl+C...

^C
Received shutdown signal. Stopping monitoring...

Monitoring stopped.

========================================
Final Analysis Report
========================================

Files processed:
  Library maps: 2
  Sample files: 2

[...full analysis results...]

Exporting results...
  Tree (text): /tmp/analysis_output/performance_tree_final.ptree.txt
  Tree (compressed): /tmp/analysis_output/performance_tree_final.ptree
  Visualization: /tmp/analysis_output/performance_tree_final.pdf

Analysis complete!
```

**Output Files**:
- `performance_tree_live.pdf` - Updated periodically during monitoring
- `performance_tree_final.pdf` - Final visualization at shutdown
- `performance_tree_final.ptree.txt` - Human-readable tree (final)
- `performance_tree_final.ptree` - Compressed binary format (final)

**Key Features**:
- ✅ Zero-configuration automatic file detection
- ✅ Real-time processing as files arrive
- ✅ Periodic summary reports while monitoring
- ✅ Graceful shutdown with Ctrl+C
- ✅ Perfect for long-running MPI applications
- ✅ Incremental tree building for efficiency

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
