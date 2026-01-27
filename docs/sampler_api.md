# MPI Performance Sampler API Documentation

## Overview

The MPI Performance Sampler is a high-performance sampling-based profiler for MPI programs using the PAPI (Performance Application Programming Interface) library. It collects hardware performance counter data at configurable intervals using PAPI's overflow mechanism.

## Features

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

## Usage

### Automatic Sampling (Recommended)

The sampler can be automatically enabled by setting environment variables:

```bash
export PERFLOW_ENABLE_SAMPLING=1
export PERFLOW_SAMPLING_FREQ=1000      # Hz
export PERFLOW_OUTPUT_PATH=./samples   # Output prefix
export PERFLOW_CALLSTACK=1             # Enable call stack (1=yes, 0=no)
export PERFLOW_COMPRESS=0              # Compress output (1=yes, 0=no)

mpirun -np 4 ./my_mpi_program
```

The sampler will automatically start when the program loads and write output when it exits.

### Programmatic API (C)

```c
#include <perflow_sampler.h>

// Create sampler
void* sampler = perflow_sampler_create();

// Initialize (frequency in Hz, output path prefix)
int ret = perflow_sampler_init(sampler, 1000, "./my_output");

// Start sampling
ret = perflow_sampler_start(sampler);

// ... run your code ...

// Stop sampling
ret = perflow_sampler_stop(sampler);

// Get sample count
size_t count = perflow_sampler_get_sample_count(sampler);

// Write output files
ret = perflow_sampler_write_output(sampler);

// Cleanup
perflow_sampler_destroy(sampler);
```

### C++ API

```cpp
#include "sampling/sampler.h"

using namespace perflow::sampling;

// Create sampler
auto sampler = std::make_unique<MPISampler>();

// Configure
SamplerConfig config;
config.sampling_frequency = 1000;    // 1000 Hz
config.enable_call_stack = true;
config.output_path = "./my_output";

// Initialize and start
sampler->Initialize(config);
sampler->Start();

// ... run your code ...

// Stop and get results
sampler->Stop();
const auto& samples = sampler->GetSamples();

// Write output
sampler->WriteOutput();
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
