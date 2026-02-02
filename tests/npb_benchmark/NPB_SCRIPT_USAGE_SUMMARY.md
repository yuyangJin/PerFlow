# NPB Benchmark Test Scripts - Usage Summary

## Overview

This comprehensive testing framework evaluates PerFlow's MPI sampler performance using NAS Parallel Benchmarks (NPB 3.4 MPI). It provides automated end-to-end benchmarking from NPB compilation to final visualization and reporting.

## Quick Start

### Prerequisites
Build PerFlow first:
```bash
cd /path/to/PerFlow
mkdir -p build && cd build
cmake ..
make -j$(nproc)
# This creates: build/examples/npb_hotspot_analyzer
```

### One-Command Execution
```bash
cd tests/npb_benchmark
./run_npb_benchmark.sh
```

This single command will:
1. Clone and compile all NPB benchmarks
2. Run baseline performance measurements
3. Run overhead analysis with PerFlow sampler
4. Perform hotspot analysis using PerFlow's C++ analysis module
5. Generate visualizations and comprehensive reports

## Script Components

### 1. run_npb_benchmark.sh - Main Orchestration Script

**Purpose**: One-click execution of the complete benchmark workflow.

**Basic Usage**:
```bash
# Full benchmark with defaults
./run_npb_benchmark.sh

# Custom benchmarks and problem class
./run_npb_benchmark.sh -b "cg ep ft" -c B

# With SLURM
./run_npb_benchmark.sh --slurm -p compute

# Custom process scales and sampling frequency
./run_npb_benchmark.sh -s "4 16 64" -f 5000

# Skip specific tasks
./run_npb_benchmark.sh --skip-baseline

# Run only specific tasks
./run_npb_benchmark.sh --only-visualize
```

**Key Options**:
- `-w DIR` - Working directory (default: ./npb_workspace)
- `-r DIR` - Results directory (default: ./npb_results)
- `-d DIR` - PerFlow build directory (default: ../../build)
- `-b BENCHMARKS` - Benchmarks list (default: "bt cg ep ft is lu mg sp")
- `-c CLASS` - Problem class A-F (default: C)
- `-s SCALES` - Process scales (default: "1 4 16 64 128 256 512")
- `-i NUM` - Iterations per test (default: 3)
- `-f FREQ` - Sampling frequency Hz (default: 1000)
- `--slurm` - Use SLURM
- `-p PARTITION` - SLURM partition (default: ja)
- `--cleanup` - Remove intermediate files

### 2. npb_setup.sh - NPB Compilation Setup

**Purpose**: Clones and compiles NPB 3.4 MPI benchmarks.

**Usage**:
```bash
./npb_setup.sh                      # Basic usage
./npb_setup.sh -w /scratch/npb -c B # Custom directory and class
./npb_setup.sh -b "cg ep ft"        # Specific benchmarks
./npb_setup.sh --force              # Force recompilation
```

**What It Does**:
- Clones NPB 3.4 MPI from GitHub
- Generates `make.def` with:
  - `-fallow-argument-mismatch` for modern gfortran
  - Absolute BINDIR: `${WORK_DIR}/MPI/bin`
- Compiles and verifies all benchmarks

**Output**: Binaries in `${WORK_DIR}/NPB/NPB3.4/NPB3.4-MPI/bin/`

### 3. npb_baseline.sh - Baseline Performance Measurement

**Purpose**: Measures baseline performance without PerFlow sampler.

**Usage**:
```bash
./npb_baseline.sh                  # Basic usage
./npb_baseline.sh -s "4 16 64"     # Custom scales
./npb_baseline.sh --slurm -p ja    # With SLURM
./npb_baseline.sh -i 10            # More iterations
```

**Important**: 
- **BT/SP automatically use square scales**: `[1, 4, 9, 16, 36, 64, 121, 256, 484]`
- **Other benchmarks use standard scales**: `[1, 4, 16, 64, 128, 256, 512]`

**Output Files**:
- `baseline_results.csv` - CSV format
- `baseline_results.json` - JSON format

### 4. npb_overhead.sh - Overhead Analysis

**Purpose**: Runs benchmarks with PerFlow sampler and measures overhead.

**Usage**:
```bash
./npb_overhead.sh                  # Basic usage
./npb_overhead.sh -f 5000          # Higher sampling frequency
./npb_overhead.sh --slurm -p ja    # With SLURM
```

**What It Does**:
- Runs with PerFlow sampler via `LD_PRELOAD`
- Collects `.pflw` sample files
- Calculates overhead: `(time_with_sampler - baseline) / baseline × 100%`
- Formats overhead to exactly 2 decimal places

**Output Files**:
- `overhead_results.csv` - Overhead data
- `overhead_results.json` - JSON format
- `sample_data/{benchmark}_{class}_np{N}_iter{M}/` - Sample directories

### 5. npb_hotspot_analysis.py - Hotspot Analysis

**Purpose**: Analyzes performance sampling data using PerFlow's C++ analyzer.

**Usage**:
```bash
python3 npb_hotspot_analysis.py              # Auto-detect samples
python3 npb_hotspot_analysis.py -r ./results # Custom results dir
python3 npb_hotspot_analysis.py -b "cg ep"   # Filter benchmarks
python3 npb_hotspot_analysis.py -s "4 16"    # Filter scales
python3 npb_hotspot_analysis.py -t 50        # Show top 50 hotspots
```

**How It Works**:
1. Scans `${RESULTS_DIR}/overhead/sample_data/` for subdirectories
2. Invokes C++ analyzer: `build/examples/npb_hotspot_analyzer`
3. Aggregates results across all runs
4. Applies exclusion rules:
   - Excludes: `_start`, `__libc_start_main`, `main`
   - Excludes: PAPI functions (`PAPI_*`, `papi_*`)
   - Excludes: PerFlow internals (`perflow_*`)
   - Uses **exclusive sample count** (leaf functions only)

**Output**: `hotspot_analysis.json`

### 6. npb_visualize.py - Visualization Generation

**Purpose**: Generates visualizations and reports.

**Usage**:
```bash
python3 npb_visualize.py              # Basic usage
python3 npb_visualize.py -r ./results # Custom results dir
```

**Generated Files**:
- `overhead_analysis.png` - Overhead line chart
- `hotspot_analysis_{benchmark}.png` - Individual hotspot charts
- `hotspot_analysis_combined.png` - Combined overview
- `summary_report.txt` - Text summary

**Note**: Requires matplotlib (optional, gracefully skips if unavailable)

## Directory Structure

After complete run:

```
npb_workspace/
└── NPB/NPB3.4/NPB3.4-MPI/bin/
    ├── bt.C.x, cg.C.x, ep.C.x, ...

npb_results/
├── baseline/
│   ├── baseline_results.csv
│   ├── baseline_results.json
│   └── *.out
├── overhead/
│   ├── overhead_results.csv
│   ├── overhead_results.json
│   ├── *.out
│   └── sample_data/
│       └── {benchmark}_{class}_np{N}_iter{M}/
│           └── perflow_mpi_rank_*.pflw
├── hotspot_analysis.json
├── overhead_analysis.png
├── hotspot_analysis_*.png
└── summary_report.txt
```

## Common Use Cases

### Quick Test
```bash
./run_npb_benchmark.sh -b "cg ep" -s "4 16" -i 1
```

### Full Production Run
```bash
./run_npb_benchmark.sh \
  -b "bt cg ep ft is lu mg sp" \
  -c C \
  -i 5 \
  --cleanup
```

### SLURM Cluster
```bash
./run_npb_benchmark.sh --slurm -p compute -s "64 128 256 512"
```

### High-Frequency Sampling
```bash
./run_npb_benchmark.sh -f 10000 -b "cg ep"
```

### Reanalyze Existing Data
```bash
./run_npb_benchmark.sh --only-hotspot   # Regenerate hotspot analysis
./run_npb_benchmark.sh --only-visualize # Regenerate visualizations
```

## Process Scale Constraints

### Standard Benchmarks (CG, EP, FT, IS, LU, MG)
Power-of-2 scales: `1, 4, 16, 64, 128, 256, 512, ...`

### Square-Constrained (BT, SP)
Perfect squares (n²): `1, 4, 9, 16, 36, 64, 121, 256, 484, ...`

**Scripts automatically handle this!** If you specify standard scales for BT/SP, they're automatically converted to square scales.

## Troubleshooting

### NPB Compilation Fails
```bash
# Ensure MPI compilers available
which mpicc mpif77
# Load MPI module if needed
module load openmpi
```

### Hotspot Analyzer Not Found
```bash
# Build PerFlow first
cd /path/to/PerFlow/build
cmake .. && make -j$(nproc)
ls examples/npb_hotspot_analyzer  # Verify
```

### SLURM Jobs Fail
```bash
sinfo   # Check partitions
squeue  # Check queue
# Update: -p <correct_partition>
```

### Matplotlib Not Available
```bash
pip install matplotlib numpy  # Optional, for visualizations
```

## Performance Tips

### Sampling Frequency
- **1000 Hz**: Recommended default (good balance)
- **5000-10000 Hz**: Detailed analysis (higher overhead)
- **100-500 Hz**: Low overhead (coarser granularity)

### Iteration Count
- **1-3**: Quick testing
- **5-10**: Production (better statistics)

### Process Scales
- Start small (4, 16, 64) to verify setup
- Scale up incrementally

## Key Features

✓ Automatic scale detection (BT/SP use square scales)
✓ Comprehensive error handling
✓ SLURM integration
✓ Modular design (run individual tasks)
✓ Multiple output formats (CSV, JSON, PNG, TXT)
✓ PerFlow C++ analysis module integration
✓ Flexible configuration (CLI, env vars, defaults)

## Environment Variables

All options can be set via environment variables:

```bash
export NPB_WORK_DIR="/scratch/npb"
export NPB_RESULTS_DIR="/scratch/results"
export PERFLOW_BUILD_DIR="/path/to/perflow/build"
export NPB_BENCHMARKS="cg ep ft"
export NPB_CLASS="B"
export NPB_PROCESS_SCALES="4 16 64"
export NPB_PROCESS_SCALES_SQUARE="1 4 9 16 36 64 121 256 484"
export NUM_ITERATIONS="5"
export PERFLOW_SAMPLING_FREQ="5000"
export USE_SLURM="1"
export SLURM_PARTITION="compute"

./run_npb_benchmark.sh
```

## For More Information

- **Detailed Help**: Run any script with `-h` flag
- **Implementation Details**: See README.md
- **Source Code**: `/tests/npb_benchmark/`

## Summary

The NPB Benchmark Test Suite provides a comprehensive, automated framework for evaluating PerFlow's MPI sampler with minimal manual intervention. Perfect for performance analysis, overhead measurement, and hotspot identification across multiple NPB benchmarks and process scales.
