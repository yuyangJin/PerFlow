# NPB Benchmark Test Scripts for PerFlow

This directory contains comprehensive test scripts for evaluating PerFlow's MPI sampler using the NAS Parallel Benchmarks (NPB 3.4 MPI).

## Overview

These scripts provide automated testing infrastructure to:
1. Compile NPB benchmarks
2. Measure baseline performance
3. Analyze PerFlow sampler overhead
4. Identify performance hotspots
5. Generate visualizations and reports

## Quick Start

### One-Click Execution

Run the complete benchmark suite with a single command:

```bash
./run_npb_benchmark.sh
```

This will:
- Clone and compile NPB 3.4 MPI benchmarks
- Run baseline tests (without sampler)
- Run overhead tests (with PerFlow sampler)
- Analyze hotspots
- Generate visualizations and reports

### Custom Configuration

```bash
# Run specific benchmarks with custom parameters
./run_npb_benchmark.sh -b "cg ep ft" -c B -s "4 16 64"

# Run with SLURM
./run_npb_benchmark.sh --slurm -p compute

# Run with higher sampling frequency
./run_npb_benchmark.sh -f 5000

# Only regenerate visualizations
./run_npb_benchmark.sh --only-visualize
```

## Script Components

### 1. npb_setup.sh - NPB Compilation Setup

Clones and compiles NPB 3.4 MPI benchmarks.

**Usage:**
```bash
./npb_setup.sh [-w WORKDIR] [-b BENCHMARKS] [-c CLASS]
```

**Options:**
- `-w DIR` - Working directory (default: ./npb_workspace)
- `-b LIST` - Benchmarks to compile (default: bt cg ep ft is lu mg sp)
- `-c CLASS` - Problem class: A-F (default: C)

**Example:**
```bash
# Compile all default benchmarks
./npb_setup.sh

# Compile specific benchmarks with class B
./npb_setup.sh -b "cg ep ft" -c B
```

### 2. npb_baseline.sh - Baseline Performance Measurement

Runs NPB benchmarks without PerFlow sampler to establish baseline performance.

**Usage:**
```bash
./npb_baseline.sh [-w WORKDIR] [-r RESULTSDIR] [-s SCALES] [-i ITERATIONS]
```

**Options:**
- `-w DIR` - NPB working directory
- `-r DIR` - Results directory (default: ./npb_results)
- `-s SCALES` - Process scales (default: "1 4 16 64 128 256 512")
- `-i NUM` - Iterations per test (default: 3)
- `--slurm` - Use SLURM for execution
- `-p PART` - SLURM partition (default: ja)

**Example:**
```bash
# Run baseline with SLURM
./npb_baseline.sh --slurm -p compute

# Run with custom scales
./npb_baseline.sh -s "4 16 64"
```

**Output:**
- `baseline_results.csv` - Execution times (CSV format)
- `baseline_results.json` - Execution times (JSON format)
- `*.out` - Individual benchmark outputs

### 3. npb_overhead.sh - Overhead Analysis

Runs NPB benchmarks with PerFlow sampler and calculates overhead.

**Usage:**
```bash
./npb_overhead.sh [-w WORKDIR] [-r RESULTSDIR] [-d BUILDDIR] [-f FREQ]
```

**Options:**
- `-w DIR` - NPB working directory
- `-r DIR` - Results directory
- `-d DIR` - PerFlow build directory (default: ../../build)
- `-f FREQ` - Sampling frequency in Hz (default: 1000)
- `--slurm` - Use SLURM for execution

**Example:**
```bash
# Run with higher sampling frequency
./npb_overhead.sh -f 5000

# Run with custom PerFlow build
./npb_overhead.sh -d /path/to/perflow/build
```

**Output:**
- `overhead_results.csv` - Overhead data (CSV format)
- `overhead_results.json` - Overhead data (JSON format)
- `sample_data/` - Performance sampling data

### 4. npb_hotspot_analysis.py - Hotspot Analysis

Analyzes performance sampling data to identify hotspots.

**Usage:**
```bash
python3 npb_hotspot_analysis.py <results_dir> [output_file]
```

**Features:**
- Parses PerFlow sample files (.pflw or .txt)
- Excludes `_start` and PAPI-related functions
- Uses exclusive sample count mode
- Calculates relative time percentages

**Example:**
```bash
# Analyze hotspots
python3 npb_hotspot_analysis.py ./npb_results

# Custom output file
python3 npb_hotspot_analysis.py ./npb_results hotspots.json
```

**Output:**
- `hotspot_analysis.json` - Hotspot data for all benchmarks

### 5. npb_visualize.py - Visualization Generation

Generates charts and reports from analysis results.

**Usage:**
```bash
python3 npb_visualize.py <results_dir>
```

**Generated Files:**
- `overhead_analysis.png` - Line chart of overhead vs. process scale
- `hotspot_analysis_*.png` - Stacked bar charts for each benchmark
- `hotspot_analysis_combined.png` - Combined overview
- `summary_report.txt` - Text summary report

**Requirements:**
- matplotlib (optional, for visualizations)
- numpy (optional, for visualizations)

**Example:**
```bash
# Generate all visualizations
python3 npb_visualize.py ./npb_results
```

### 6. run_npb_benchmark.sh - Main Orchestration Script

One-click execution of the complete benchmark workflow.

**Usage:**
```bash
./run_npb_benchmark.sh [OPTIONS]
```

**Key Options:**
- `-b BENCHMARKS` - Benchmarks to run
- `-c CLASS` - Problem class
- `-s SCALES` - Process scales
- `-i NUM` - Iterations per test
- `-f FREQ` - Sampling frequency
- `--slurm` - Use SLURM
- `--skip-*` - Skip specific tasks
- `--only-*` - Run only specific task
- `--cleanup` - Remove intermediate files

**Examples:**
```bash
# Full run with defaults
./run_npb_benchmark.sh

# Custom configuration
./run_npb_benchmark.sh -b "cg ep" -c C -s "4 16 64" -i 5

# Skip baseline (already completed)
./run_npb_benchmark.sh --skip-baseline

# Only regenerate visualizations
./run_npb_benchmark.sh --only-visualize

# Full run with cleanup
./run_npb_benchmark.sh --cleanup
```

## Configuration

### Environment Variables

All scripts support environment variable configuration:

```bash
# Working directories
export NPB_WORK_DIR=/path/to/npb_workspace
export NPB_RESULTS_DIR=/path/to/results
export PERFLOW_BUILD_DIR=/path/to/perflow/build

# Benchmark configuration
export NPB_BENCHMARKS="cg ep ft"
export NPB_CLASS="C"
export NPB_PROCESS_SCALES="1 4 16 64"
export NUM_ITERATIONS=3

# PerFlow configuration
export PERFLOW_SAMPLING_FREQ=1000

# SLURM configuration
export USE_SLURM=1
export SLURM_PARTITION=compute

# Run the benchmark
./run_npb_benchmark.sh
```

### Default Values

| Parameter | Default Value |
|-----------|---------------|
| Benchmarks | bt cg ep ft is lu mg sp |
| Class | C |
| Process Scales | 1 4 16 64 128 256 512 |
| Iterations | 3 |
| Sampling Frequency | 1000 Hz |
| Use SLURM | 0 (disabled) |
| SLURM Partition | ja |

## NPB Benchmarks

The following NPB-MPI benchmarks are supported:

| Benchmark | Description | Language |
|-----------|-------------|----------|
| **bt** | Block Tri-diagonal solver | Fortran |
| **cg** | Conjugate Gradient | Fortran |
| **ep** | Embarrassingly Parallel | Fortran |
| **ft** | Fast Fourier Transform | Fortran |
| **is** | Integer Sort | C |
| **lu** | Lower-Upper Gauss-Seidel solver | Fortran |
| **mg** | Multi-Grid | Fortran |
| **sp** | Scalar Penta-diagonal solver | Fortran |

### Problem Classes

| Class | Problem Size | Memory Requirements |
|-------|--------------|---------------------|
| **S** | Small | ~10 MB |
| **W** | Workstation | ~100 MB |
| **A** | Standard | ~1 GB |
| **B** | Large | ~4 GB |
| **C** | Extra Large | ~16 GB |
| **D** | Huge | ~64 GB |
| **E** | Extreme | ~256 GB |

## Requirements

### System Requirements

- **Operating System**: Linux (tested on Ubuntu 20.04+)
- **MPI**: OpenMPI, MPICH, or Intel MPI
- **Compilers**: gfortran, gcc, mpicc, mpif77
- **Python**: 3.6+ (for analysis scripts)
- **Git**: For cloning NPB repository

### PerFlow Requirements

- PerFlow must be built with MPI sampler support
- Library location: `build/lib/libperflow_mpi_sampler.so`

### Optional Requirements

For visualization:
```bash
pip3 install matplotlib numpy
```

## Output Structure

After running the complete benchmark:

```
npb_results/
├── baseline/
│   ├── baseline_results.csv
│   ├── baseline_results.json
│   └── *.out
├── overhead/
│   ├── overhead_results.csv
│   ├── overhead_results.json
│   ├── sample_data/
│   │   ├── cg_C_np4_iter1/
│   │   │   ├── perflow_mpi_rank_0.pflw
│   │   │   ├── perflow_mpi_rank_1.pflw
│   │   │   └── ...
│   │   └── ...
│   └── *.out
├── hotspot_analysis.json
├── overhead_analysis.png
├── hotspot_analysis_*.png
├── hotspot_analysis_combined.png
└── summary_report.txt
```

## Analysis Methodology

### Overhead Calculation

Overhead percentage is calculated using:

```
overhead% = (time_with_sampler - baseline_time) / baseline_time × 100%
```

### Hotspot Analysis

1. **Sample Collection**: PerFlow collects call stack samples during execution
2. **Function Exclusion**: Filters out `_start`, `__libc_start_main`, and PAPI functions
3. **Exclusive Mode**: Uses leaf function in call stack (self time)
4. **Percentage Calculation**: `(function_samples / total_samples) × 100%`

## Troubleshooting

### NPB Compilation Failures

**Issue**: Fortran or C compiler not found

**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install gfortran gcc openmpi-bin libopenmpi-dev

# Check compiler availability
which mpicc mpif77
```

### PerFlow Sampler Not Found

**Issue**: `libperflow_mpi_sampler.so` not found

**Solution**:
```bash
# Build PerFlow
cd /path/to/PerFlow
mkdir -p build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)

# Verify library
ls -la build/lib/libperflow_mpi_sampler.so
```

### SLURM Execution Issues

**Issue**: `srun` command fails

**Solution**:
- Ensure SLURM is installed: `which srun`
- Check partition exists: `sinfo`
- Use `-p` to specify correct partition
- Or disable SLURM: remove `--slurm` flag

### Python Dependencies Missing

**Issue**: `matplotlib` not found

**Solution**:
```bash
# Install matplotlib and numpy
pip3 install matplotlib numpy

# Or skip visualization
./run_npb_benchmark.sh --skip-visualize
```

## Performance Tips

1. **Sampling Frequency**: Higher frequencies (5000-10000 Hz) provide more detailed hotspot data but increase overhead
2. **Process Scales**: Adjust based on available resources
3. **Iterations**: Multiple iterations (3-5) improve statistical reliability
4. **SLURM**: Use for large-scale tests on HPC clusters

## Contributing

When adding new features:
1. Follow existing script structure
2. Add comprehensive help messages
3. Update this README
4. Test with various configurations

## References

- **NPB**: https://github.com/llnl/NPB
- **PerFlow**: https://github.com/yuyangJin/PerFlow
- **NAS Parallel Benchmarks**: https://www.nas.nasa.gov/software/npb.html

## License

Copyright 2024 PerFlow Authors  
Licensed under the Apache License, Version 2.0

## Support

For issues or questions:
- Check [TESTING.md](../TESTING.md) in parent directory
- Review script help messages: `./script.sh -h`
- Open an issue on GitHub
