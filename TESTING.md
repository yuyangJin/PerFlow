# MPI Sampler Testing Guide

This document provides comprehensive information about testing the PerFlow MPI performance sampler.

## Overview

The MPI sampler test suite consists of multiple test programs designed to validate the sampler's functionality across different workload patterns commonly found in HPC applications.

## Test Programs

### 1. Basic MPI Sampler Test (`mpi_sampler_test`)

**Purpose**: Basic validation of sampler functionality

**Workload Characteristics**:
- Matrix multiplication (1000×1000)
- Vector operations
- MPI collective operations (Bcast, Reduce, Allreduce, Gather, Scatter)

**Expected Behavior**:
- Generates samples during computation and communication
- Creates output files for each rank
- Completes in ~5-10 seconds

### 2. Compute-Intensive Test (`test_compute_intensive`)

**Purpose**: Test sampler with CPU-bound computations

**Workload Characteristics**:
- Dense matrix multiplication (500×500)
- FFT-like butterfly operations
- Trigonometric operations (sin, cos, tan, sqrt)
- Jacobi iterative solver

**Expected Behavior**:
- High PAPI_FP_OPS and PAPI_TOT_INS counts
- Low MPI communication counters
- Consistent sampling across all ranks
- Completes in ~10-15 seconds

**Validation Points**:
- Verify high computational activity in samples
- Check that samples are distributed across different call stacks
- Ensure minimal MPI-related samples

### 3. Communication-Intensive Test (`test_comm_intensive`)

**Purpose**: Test sampler with MPI communication-heavy workloads

**Workload Characteristics**:
- Ring communication pattern (neighbor exchange)
- Mesh communication (all-to-all exchanges)
- Collective operations (Bcast, Reduce, Allreduce, Gather, Scatter, Alltoall)
- Non-blocking communication with computation overlap
- Alltoall operations

**Expected Behavior**:
- High MPI call counts
- Communication time measurements
- Overlap of computation and communication visible
- Requires at least 2 processes
- Completes in ~10-15 seconds

**Validation Points**:
- Verify high MPI activity in samples
- Check that different MPI operations are captured
- Ensure communication patterns are visible

### 4. Memory-Intensive Test (`test_memory_intensive`)

**Purpose**: Test sampler with memory allocation/deallocation patterns

**Workload Characteristics**:
- Frequent memory allocations and deallocations
- Sequential (streaming) memory access
- Random memory access patterns
- Cache-friendly operations (row-major access)
- Cache-unfriendly operations (column-major access)
- Memory copy operations

**Expected Behavior**:
- High cache miss counters (PAPI_L1_DCM, PAPI_L2_DCM)
- Memory bandwidth measurements
- Stable operation despite frequent memory operations
- Cache-unfriendly should show higher miss rate than cache-friendly
- Completes in ~15-20 seconds

**Important Note**: Carefully designed to avoid deadlocks since sampler may disable sampling during malloc operations.

**Validation Points**:
- Verify memory-related performance counters
- Compare cache miss rates between friendly and unfriendly patterns
- Ensure stable operation with no crashes

### 5. Hybrid Workload Test (`test_hybrid`)

**Purpose**: Test sampler with mixed computation/communication patterns

**Workload Characteristics**:
- Alternating phases of computation and communication
- Overlapped computation and communication (MPI_Irecv + computation)
- Load imbalance scenarios (rank 0 does more work)
- Stencil-like patterns with neighbor communication
- Pipeline pattern (sequential processing)

**Expected Behavior**:
- Balanced collection of both computation and communication events
- Correct attribution of time to different phases
- Load imbalance visible in per-rank statistics
- Completes in ~15-20 seconds

**Validation Points**:
- Verify both computation and communication are captured
- Check for load imbalance in sample distributions
- Ensure overlapped operations are properly attributed

### 6. Stress Test (`test_stress`)

**Purpose**: Test sampler under high load conditions

**Workload Characteristics**:
- Mixed operations at high frequency
- Long-running operations (30+ seconds total)
- Memory-intensive cycles
- Varying sampling frequencies (configurable via environment)

**Expected Behavior**:
- Sampler stability under high frequency
- Memory usage remains stable
- No performance degradation of applications
- Completes in ~30 seconds

**Validation Points**:
- Verify sampler stability under load
- Check for memory leaks
- Ensure consistent performance across duration

## Building Tests

### Prerequisites

- MPI implementation (OpenMPI, MPICH, or Intel MPI)
- PAPI library (version 7.1.0 or newer)
- CMake 3.14 or newer
- C++17 compatible compiler
- libunwind (for call stack capture)

### Build Instructions

```bash
# From repository root
mkdir -p build
cd build

# Configure with tests enabled
cmake .. -DPERFLOW_BUILD_TESTS=ON

# Build all tests
make -j$(nproc)

# Build specific test
make test_compute_intensive
```

After building, test executables are located in `build/tests/`.

## Running Tests

### Using CMake Targets

```bash
# Run individual test
cd build
make run_test_compute_intensive

# Run all MPI tests
make run_all_mpi_tests
```

### Manual Execution

```bash
# Run with LD_PRELOAD
cd build
LD_PRELOAD=lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp \
mpirun -n 4 tests/test_compute_intensive
```

### Using the Test Script

```bash
# From repository root
./tests/run_all_tests.sh

# With custom settings
./tests/run_all_tests.sh -n 8 -f 10000 -v

# Options:
#   -n NUM      Number of MPI processes (default: 4)
#   -o DIR      Output directory (default: /tmp/perflow_test_results)
#   -f FREQ     Sampling frequency in Hz (default: 1000)
#   -b DIR      Build directory (default: build)
#   -v          Verbose mode
#   --no-validation  Skip validation script
```

## Environment Variables

### PERFLOW_OUTPUT_DIR
Directory where sampler output files are written.
- Default: `/tmp`
- Example: `PERFLOW_OUTPUT_DIR=/tmp/perflow_results`

### PERFLOW_SAMPLING_FREQ
Sampling frequency in Hz (samples per second).
- Default: `1000`
- Recommended range: 100-10000
- Example: `PERFLOW_SAMPLING_FREQ=5000`

### Example with Different Frequencies

```bash
# Low frequency (100 Hz)
PERFLOW_SAMPLING_FREQ=100 ./tests/run_all_tests.sh -n 4

# Medium frequency (1000 Hz)
PERFLOW_SAMPLING_FREQ=1000 ./tests/run_all_tests.sh -n 4

# High frequency (10000 Hz)
PERFLOW_SAMPLING_FREQ=10000 ./tests/run_all_tests.sh -n 4
```

## Output Files

Each test run generates the following files:

### Per-Rank Files

- `perflow_mpi_rank_N.pflw`: Binary format with performance samples
- `perflow_mpi_rank_N.txt`: Human-readable text format with call stacks

### Test Result Files (when using run_all_tests.sh)

- `<test_name>_output.txt`: Standard output from test
- `<test_name>_error.txt`: Standard error from test  
- `<test_name>_rank_N.pflw`: Sampler output files
- `validation_report.txt`: Validation results

## Validation

### Using the Validation Script

```bash
# Validate results from a test run
python3 tests/validate_sampler_results.py /tmp/perflow_test_results
```

The validation script checks:
- All expected output patterns are present
- Sample files are generated for all ranks
- Minimum sample counts are met
- Files contain valid data

### Manual Validation

Check for the following in output files:

1. **Output Files**: Each rank should produce `.pflw` and `.txt` files
2. **Sample Counts**: Text files should show multiple unique call stacks
3. **Performance Counters**: Counters (TOT_CYC, TOT_INS, L1_DCM) should be non-zero
4. **Completion Messages**: Tests should print "completed successfully"

## Troubleshooting

### Common Issues

#### 1. Tests Fail to Build

**Problem**: Missing dependencies
**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install libopenmpi-dev openmpi-bin libpapi-dev libunwind-dev

# Check if libraries are found
cmake .. -DPERFLOW_BUILD_TESTS=ON 2>&1 | grep -E "PAPI|MPI"
```

#### 2. No Output Files Generated

**Problem**: Sampler not loaded or environment not set
**Solution**:
```bash
# Verify LD_PRELOAD is set
echo $LD_PRELOAD

# Check if sampler library exists
ls -la build/lib/libperflow_mpi_sampler.so

# Ensure output directory is writable
ls -ld /tmp
```

#### 3. Segmentation Faults

**Problem**: Call stack capture issues
**Solution**:
- Check if libunwind is installed
- Try rebuilding with debug symbols: `cmake .. -DCMAKE_BUILD_TYPE=Debug`
- Run with gdb: `mpirun -n 1 gdb --args ./test_program`

#### 4. Low Sample Counts

**Problem**: Sampling frequency too low or test runs too quickly
**Solution**:
```bash
# Increase sampling frequency
PERFLOW_SAMPLING_FREQ=10000 mpirun -n 4 ./test_program

# Or modify test parameters in source code
```

#### 5. MPI Errors

**Problem**: Insufficient resources or MPI configuration issues
**Solution**:
```bash
# Check MPI installation
mpirun --version

# Use fewer processes
mpirun -n 2 ./test_program

# Disable CPU binding
mpirun --bind-to none -n 4 ./test_program
```

## Performance Considerations

### Sampling Overhead

The sampler introduces minimal overhead:
- At 1000 Hz: < 2% overhead for most applications
- At 10000 Hz: ~5-10% overhead depending on workload
- Memory usage: ~10-50 MB per process

### Recommendations

1. **For Development**: Use 1000 Hz for good balance
2. **For Detailed Analysis**: Use 5000-10000 Hz
3. **For Production**: Use 100-500 Hz for minimal overhead

## Acceptance Criteria

Tests are considered passing when:

- [ ] All test programs compile without errors
- [ ] All tests pass with at least 4 MPI processes
- [ ] Sampler collects expected data patterns for each workload type
- [ ] No memory leaks detected (valgrind clean - optional)
- [ ] Test execution time < 5 minutes for the full suite
- [ ] All validation checks pass

## Memory Leak Detection (Optional)

To check for memory leaks:

```bash
# Run single rank with valgrind
LD_PRELOAD=build/lib/libperflow_mpi_sampler.so \
valgrind --leak-check=full --show-leak-kinds=all \
mpirun -n 1 build/tests/test_compute_intensive
```

Note: MPI and PAPI may report some "still reachable" memory, which is usually acceptable.

## Contributing

When adding new tests:

1. Follow the existing test structure
2. Add test program to `tests/CMakeLists.txt`
3. Update `TEST_EXPECTATIONS` in `validate_sampler_results.py`
4. Document the test in this file
5. Ensure test completes in reasonable time (< 60 seconds)

## References

- PAPI Documentation: http://icl.utk.edu/papi/
- MPI Standard: https://www.mpi-forum.org/
- PerFlow GitHub: https://github.com/yuyangJin/PerFlow

## Support

For issues or questions:
- Open an issue on GitHub
- Check existing documentation in `docs/`
- Review source code comments in test files
