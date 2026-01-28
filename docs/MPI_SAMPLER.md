# MPI Performance Sampler

## Overview

The MPI Performance Sampler is a performance profiling tool for MPI applications using the PAPI (Performance API) library. It uses dynamic instrumentation via `LD_PRELOAD` to automatically collect performance data without modifying the target application.

## Architecture

### Components

1. **src/sampler/mpi_sampler.cpp** - Main sampler implementation
   - PAPI initialization and event setup
   - Overflow-based sampling using hardware performance counters
   - **Call stack capture** with two available methods:
     - **Method 1 (default)**: libunwind-based unwinding (from `demo/sampling/sampler.cpp`)
     - **Method 2 (alternative)**: Frame pointer unwinding (from `include/sampling/pmu_sampler.h`)
   - Data export in PerFlow binary format

2. **tests/mpi_sampler_test.cpp** - Test application
   - Matrix multiplication workload
   - Vector operations
   - MPI collective operations (Bcast, Reduce, Gather, Scatter, Allreduce)

### Key Features

- **Dynamic Instrumentation**: Uses constructor/destructor attributes for automatic initialization
- **Hardware Events Monitored**: 
  - PAPI_TOT_CYC: Total CPU cycles
  - PAPI_TOT_INS: Total instructions
  - PAPI_L1_DCM: L1 data cache misses
- **Configurable Sampling**: Default 1000 Hz, configurable via `PERFLOW_SAMPLING_FREQ`
- **Thread-Safe**: Uses signal-safe operations in overflow handler
- **Data Export**: Outputs to `/tmp/perflow_mpi_rank_<N>.pflw` files

## Building

### Prerequisites

```bash
sudo apt-get install libpapi-dev libopenmpi-dev libunwind-dev
```

### Build Commands

```bash
mkdir build && cd build
cmake ..
make perflow_mpi_sampler
make mpi_sampler_test
```

## Usage

### Basic Usage

```bash
# Run with sampler via LD_PRELOAD
LD_PRELOAD=/path/to/libperflow_mpi_sampler.so \
  PERFLOW_OUTPUT_DIR=/tmp \
  mpirun -n 4 ./your_mpi_program
```

### Environment Variables

- `PERFLOW_SAMPLING_FREQ`: Sampling frequency in Hz (default: 1000)
- `PERFLOW_OUTPUT_DIR`: Output directory for sample files (default: /tmp)

### Running the Test

```bash
cd build
make run_mpi_sampler_test
```

Or manually:

```bash
LD_PRELOAD=./lib/libperflow_mpi_sampler.so \
  PERFLOW_OUTPUT_DIR=/tmp \
  mpirun -n 4 ./tests/mpi_sampler_test
```

## Output Format

The sampler generates binary files in PerFlow format:
- `/tmp/perflow_mpi_rank_0.pflw`
- `/tmp/perflow_mpi_rank_1.pflw`
- etc.

Each file contains:
- Call stacks captured at sample points
- Sample counts for each unique call stack
- Metadata (timestamp, event counts, etc.)

## Implementation Details

### Constructor/Destructor Hooks

The sampler uses GCC's constructor/destructor attributes for automatic initialization:

```cpp
static void initialize_sampler() __attribute__((constructor));
static void finalize_sampler() __attribute__((destructor));
```

This allows the sampler to:
- Initialize before `main()` executes
- Clean up and export data after `main()` returns

### Overflow-Based Sampling

The sampler uses PAPI's overflow mechanism:
1. Configure overflow on PAPI_TOT_CYC at specified threshold
2. PAPI triggers signal when counter overflows
3. Signal handler captures call stack and records sample
4. Counter automatically resets and sampling continues

### Call Stack Capture Methods

The sampler provides two methods for capturing call stacks:

#### Method 1: libunwind (Default, Recommended)

Based on `GetBacktrace()` and `my_backtrace()` from `demo/sampling/sampler.cpp`:
- Uses the libunwind library for safe and reliable stack unwinding
- Signal-safe and works correctly in signal handler context
- No segmentation faults
- Captures deeper call stacks (up to kMaxCallStackDepth frames)
- Requires `libunwind-dev` package

This is the **default method** and is automatically selected when building.

#### Method 2: Frame Pointer Unwinding (Alternative)

Based on `captureCallStack()` from `include/sampling/pmu_sampler.h`:
- Uses `__builtin_frame_address()` and `__builtin_return_address()`
- Limited to compile-time constant depth (up to 16 frames)
- **WARNING**: Can cause segmentation faults in some contexts
- Only recommended for testing or specific use cases

To switch to Method 2, define `USE_PMU_SAMPLER_CALLSTACK` before compiling:
```bash
cmake .. -DCMAKE_CXX_FLAGS="-DUSE_PMU_SAMPLER_CALLSTACK"
```

### Signal Safety

The overflow handler is carefully designed to be signal-safe:
- No dynamic memory allocation
- No stdio operations
- Uses pre-allocated data structures
- Minimal operations to reduce overhead

## Environment Limitations

### Virtualized Environments

PAPI requires access to hardware performance counters, which may be restricted in:
- Docker containers
- Cloud VMs (AWS, Azure, GCP)
- CI/CD environments

To check if PAPI is available:

```bash
papi_avail
cat /proc/sys/kernel/perf_event_paranoid
```

If `perf_event_paranoid` is > 1, you may need root access or kernel parameter changes:

```bash
# Allow non-root users to sample
sudo sysctl -w kernel.perf_event_paranoid=1
```

### Known Issues

1. **Azure VMs**: Some VM types don't expose performance counters to guests
2. **Hyper-V**: May require specific configuration
3. **Nested Virtualization**: Often blocks PMU access

## Testing

### Unit Tests

```bash
cd build/tests
./perflow_tests
```

### Integration Test

The MPI sampler test performs:
1. 100x100 matrix multiplication (10 iterations)
2. Vector operations on 10,000 elements (100 iterations)
3. MPI collectives with various data sizes (10 iterations each)

Expected behavior:
- Sampler initializes on all ranks
- Performance data collected during computations
- Data exported to individual rank files on finalization

## Troubleshooting

### "Permission denied" errors

```bash
# Check permissions
cat /proc/sys/kernel/perf_event_paranoid

# Fix (requires root)
sudo sysctl -w kernel.perf_event_paranoid=1
```

### "Event does not exist" errors

Some PAPI events may not be available on all systems. The sampler will:
1. Try to add each event individually
2. Continue with whatever events are available
3. Fail only if no events can be added

### Library not found

```bash
# Verify library was built
ls -l build/lib/libperflow_mpi_sampler.so

# Check library dependencies
ldd build/lib/libperflow_mpi_sampler.so
```

## Performance Impact

Sampling overhead depends on frequency:
- 100 Hz: ~0.1-0.5% overhead
- 1000 Hz: ~1-2% overhead
- 10000 Hz: ~5-10% overhead

The overhead comes from:
1. Signal handling (~100-500 CPU cycles per sample)
2. Call stack unwinding (~200-1000 cycles)
3. Hash map update (~50-200 cycles)

## Future Enhancements

1. Support for more PAPI events
2. Dynamic frequency adjustment
3. Per-thread sampling
4. Integration with symbol resolution
5. Real-time data export
6. Support for PAPI native events
7. Fallback to timer-based sampling when PMU unavailable

## References

- PAPI Documentation: https://icl.utk.edu/papi/
- PerFlow Sampling Framework: See `include/sampling/`
- Linux perf_event: https://man7.org/linux/man-pages/man2/perf_event_open.2.html
