# MPI Performance Sampler API Documentation

## Overview

The MPI Performance Sampler is a **dynamic instrumentation** profiler for MPI programs using the PAPI (Performance Application Programming Interface) library. It is designed to be used with `LD_PRELOAD` to collect hardware performance counter data from **any MPI program without source code modifications**.

## Key Concept: Dynamic Instrumentation

This sampler uses the `LD_PRELOAD` mechanism to inject profiling code into any MPI program at runtime. The sampler is built as a shared library (`libperflow_sampler.so`) that automatically:

1. Initializes when loaded via `LD_PRELOAD`
2. Starts sampling when enabled via environment variables
3. Collects performance data during program execution
4. Writes output files when the program exits

**No changes to your application source code are required.**

## Features

- **Dynamic instrumentation via LD_PRELOAD**: Profile any MPI program without recompilation
- **Configurable sampling frequency** (default: 1000 Hz)
- **Multiple hardware events**: Tracks PAPI_TOT_CYC (cycles), PAPI_TOT_INS (instructions), PAPI_L1_DCM (L1 data cache misses)
- **Call stack capture**: Optional call stack sampling for context
- **MPI-aware**: Properly handles multi-process MPI programs
- **Thread-safe**: Safe for multi-threaded applications
- **Signal-safe overflow handling**: Uses lock-free data structures
- **Multiple output formats**: Text, binary, and compressed

## Prerequisites

- PAPI library (version 7.1.0 or newer)
- MPI implementation (OpenMPI, MPICH, or Intel MPI)
- C++14 compliant compiler

## Installation

```bash
mkdir build && cd build
cmake ..
make
```

This will produce `libperflow_sampler.so` in the build directory.

## Usage

### Dynamic Instrumentation with LD_PRELOAD (Recommended)

Profile any MPI program by setting environment variables and using `LD_PRELOAD`:

```bash
# Set environment variables for the sampler
export PERFLOW_ENABLE_SAMPLING=1
export PERFLOW_SAMPLING_FREQ=1000      # Sampling frequency in Hz
export PERFLOW_OUTPUT_PATH=./samples   # Output file prefix
export PERFLOW_CALLSTACK=1             # Enable call stack (1=yes, 0=no)
export PERFLOW_COMPRESS=0              # Compress output (1=yes, 0=no)

# Run your MPI program with LD_PRELOAD
mpirun -np 4 -x LD_PRELOAD=/path/to/libperflow_sampler.so ./my_mpi_program
```

The sampler will:
1. Automatically start when the library is loaded
2. Collect performance samples during execution
3. Write output files when the program exits (one per MPI rank)

### OpenMPI Example

```bash
export PERFLOW_ENABLE_SAMPLING=1
export PERFLOW_OUTPUT_PATH=./profile_data

mpirun -np 4 \
  -x PERFLOW_ENABLE_SAMPLING \
  -x PERFLOW_OUTPUT_PATH \
  -x LD_PRELOAD=/path/to/libperflow_sampler.so \
  ./my_mpi_program
```

### MPICH Example

```bash
export PERFLOW_ENABLE_SAMPLING=1
export PERFLOW_OUTPUT_PATH=./profile_data
export LD_PRELOAD=/path/to/libperflow_sampler.so

mpiexec -np 4 -env LD_PRELOAD $LD_PRELOAD ./my_mpi_program
```

### CMake Test Target

If you build the project, you can use the provided test targets:

```bash
# Run test with dynamic instrumentation
make run_mpi_sampler_test

# Run test without instrumentation (for comparison)
make run_mpi_test_uninstrumented
```

## Output Format

### Text Format (.txt)

```
# PerFlow Performance Samples
# Rank: 0
# Total Samples: 1234
# Columns: timestamp, cycles, instructions, l1_dcm, thread_id, stack_depth
#
1234567890,1000000,1500000,1234,139812345,8,0x7fff1234,0x7fff5678,...
```

### Binary Format (.bin)

```
Header:
  magic:       4 bytes (0x50455246 = "PERF")
  version:     4 bytes
  num_samples: 4 bytes
  rank:        4 bytes

Per sample:
  timestamp:     8 bytes
  event_values:  24 bytes (3 x int64)
  thread_id:     4 bytes
  stack_size:    4 bytes
  stack:         stack_size x 8 bytes
```

## Error Codes

| Code | Description |
|------|-------------|
| 0    | Success |
| -1   | PAPI initialization failed |
| -2   | Event set setup failed |
| -3   | Data collection initialization failed |
| -4   | Sampler not initialized |

## Configuration Options

| Environment Variable | Description | Default |
|---------------------|-------------|---------|
| PERFLOW_ENABLE_SAMPLING | Enable automatic sampling | 0 (disabled) |
| PERFLOW_SAMPLING_FREQ | Sampling frequency in Hz | 1000 |
| PERFLOW_OUTPUT_PATH | Output file path prefix | ./perflow_samples |
| PERFLOW_CALLSTACK | Enable call stack capture | 1 |
| PERFLOW_COMPRESS | Enable compressed output | 0 |

## Hardware Events

The sampler tracks three hardware events by default:

1. **PAPI_TOT_CYC**: Total CPU cycles
2. **PAPI_TOT_INS**: Total instructions executed
3. **PAPI_L1_DCM**: L1 data cache misses

These events can be used to compute:
- **IPC (Instructions Per Cycle)**: PAPI_TOT_INS / PAPI_TOT_CYC
- **Cache miss rate**: PAPI_L1_DCM / PAPI_TOT_INS

## Best Practices

1. **Sampling frequency**: Start with 1000 Hz. Increase for shorter programs, decrease for long-running applications.

2. **Call stack depth**: Deeper call stacks provide more context but increase overhead and output size.

3. **Multi-threaded programs**: The sampler is thread-safe but sampling happens only on the main thread by default.

4. **Overhead**: Typical overhead is <1% for 1000 Hz sampling. Monitor with `PAPI_TOT_CYC` values.

## Troubleshooting

### "PAPI library version mismatch"
Ensure the PAPI library matches the headers used during compilation.

### "Failed to add event"
Some hardware events may not be available on your processor. Check `papi_avail` for supported events.

### No samples collected
- Verify `PERFLOW_ENABLE_SAMPLING=1` is set
- Check that the program runs long enough for samples
- Ensure PAPI is properly initialized

## License

MIT License - See LICENSE file for details.
