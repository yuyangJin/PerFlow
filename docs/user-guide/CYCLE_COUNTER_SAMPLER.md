# Cycle Counter-Based MPI Sampler

The cycle counter-based MPI sampler provides a high-precision alternative to the POSIX timer-based sampler, utilizing the ARMv9 cycle counter register (`cntvct_el0`) for precise timing on ARM architectures.

## Overview

This sampler combines the ARM cycle counter with POSIX timer signals to achieve precise sampling based on fixed cycle intervals. It automatically detects the target architecture and falls back to pure POSIX timer-based sampling on non-ARM systems.

### Key Features

- **High-precision timing** using ARM cycle counter (cntvct_el0)
- **Automatic fallback** to POSIX timer on non-ARM architectures
- **Configurable timer method** via environment variable
- **Drop-in replacement** for other PerFlow samplers
- **Identical output formats** compatible with existing analysis tools

## Quick Start

### 1. Build the Cycle Counter Sampler

```bash
cd /path/to/PerFlow
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)
```

This will create `lib/libperflow_mpi_sampler_cycle_counter.so`

### 2. Run Your MPI Application

```bash
# Basic usage with automatic timer selection
LD_PRELOAD=/path/to/libperflow_mpi_sampler_cycle_counter.so \
    mpirun -n 4 ./your_mpi_application

# Force cycle counter mode (ARM only)
LD_PRELOAD=/path/to/libperflow_mpi_sampler_cycle_counter.so \
    PERFLOW_TIMER_METHOD=cycle \
    mpirun -n 4 ./your_mpi_application

# Force POSIX timer mode
LD_PRELOAD=/path/to/libperflow_mpi_sampler_cycle_counter.so \
    PERFLOW_TIMER_METHOD=posix \
    mpirun -n 4 ./your_mpi_application

# Custom sampling frequency
LD_PRELOAD=/path/to/libperflow_mpi_sampler_cycle_counter.so \
    PERFLOW_SAMPLING_FREQ=500 \
    mpirun -n 4 ./your_mpi_application
```

### 3. Analyze Results

The sampler produces the same output files as other PerFlow samplers:
- `perflow_mpi_rank_N.pflw` - Binary sample data
- `perflow_mpi_rank_N.txt` - Human-readable sample data  
- `perflow_mpi_rank_N.libmap` - Binary library map
- `perflow_mpi_rank_N.libmap.txt` - Human-readable library map

## Configuration Options

### Environment Variables

| Variable | Description | Default | Options |
|----------|-------------|---------|---------|
| `PERFLOW_SAMPLING_FREQ` | Sampling frequency in Hz | 1000 | 10-10000 |
| `PERFLOW_OUTPUT_DIR` | Directory for output files | /tmp | Any writable path |
| `PERFLOW_TIMER_METHOD` | Timer method selection | auto | `auto`, `cycle`, `posix` |

### Timer Method Selection

- **`auto`** (default): Automatically detect ARM architecture and use cycle counter if available, otherwise fall back to POSIX timer
- **`cycle`**: Force cycle counter mode (falls back to POSIX on non-ARM)
- **`posix`**: Force POSIX timer mode (works on all platforms)

## Architecture Support

### Primary: ARM64 (ARMv9/ARMv8)

On ARM64 architectures, the sampler uses:
- `cntvct_el0`: Virtual counter register for reading cycles
- `cntfrq_el0`: Counter frequency register for timing calculations

The cycle counter provides nanosecond-level precision for sampling triggers.

### Fallback: All Other Architectures

On x86_64 and other non-ARM architectures:
- Uses `clock_gettime(CLOCK_MONOTONIC)` for timing
- Provides microsecond-level precision
- Functionally identical to the timer-based sampler

## How It Works

### Cycle Counter Mode (ARM)

```
1. POSIX timer fires every 100 microseconds (check interval)
2. Signal handler reads ARM cycle counter (cntvct_el0)
3. Compares elapsed cycles against threshold
4. If threshold exceeded: capture call stack and record sample
5. Reset counter and continue
```

### POSIX Timer Mode (Fallback)

```
1. POSIX timer fires at sampling frequency (e.g., every 1ms at 1000 Hz)
2. Signal handler immediately captures call stack
3. Records sample and continues
```

## Performance Comparison

### Cycle Counter vs POSIX Timer

| Aspect | Cycle Counter (ARM) | POSIX Timer |
|--------|---------------------|-------------|
| Precision | Sub-microsecond | Microsecond |
| Overhead | Low (efficient counter read) | Very low |
| Jitter | Minimal | Subject to scheduler |
| Frequency scaling | Counter is independent | Clock-based |
| Multi-core sync | System-wide counter | Per-process timer |

### Recommended Sampling Frequencies

| Frequency | Use Case | Overhead | Data Quality |
|-----------|----------|----------|--------------|
| 100 Hz | Quick profiling | ~0.1-0.2% | Basic hotspots |
| 500 Hz | General purpose | ~0.5-1% | Good detail |
| 1000 Hz (default) | Detailed profiling | ~1-2% | High detail |
| 5000 Hz | Maximum detail | ~5-10% | Very high detail |

## Switching Between Samplers

All PerFlow samplers produce identical output formats:

```bash
# Hardware PMU sampler (PAPI-based, if available)
LD_PRELOAD=/path/to/libperflow_mpi_sampler.so mpirun -n 4 ./app

# POSIX timer-based sampler
LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so mpirun -n 4 ./app

# Cycle counter-based sampler (ARM optimized)
LD_PRELOAD=/path/to/libperflow_mpi_sampler_cycle_counter.so mpirun -n 4 ./app
```

## Troubleshooting

### Cycle counter not detected

**Problem**: On ARM, sampler falls back to POSIX timer despite `PERFLOW_TIMER_METHOD=cycle`

**Solutions**:
- Verify you're running on ARM64 architecture: `uname -m` should show `aarch64`
- Check if the counter registers are accessible (some virtualized environments may block them)
- The fallback is automatic and your profiling will still work correctly

### High overhead at high frequencies

**Problem**: Application runs slowly with high sampling frequency (>5000 Hz)

**Solutions**:
- Reduce sampling frequency to 500-1000 Hz for most use cases
- Use `PERFLOW_TIMER_METHOD=cycle` on ARM for lower overhead
- Profile I/O-bound applications at lower frequencies

### Output files in wrong location

**Problem**: Cannot find output files after profiling

**Solutions**:
- Check `PERFLOW_OUTPUT_DIR` is set and writable
- Default output location is `/tmp/`
- Files are named `perflow_mpi_rank_N.pflw` where N is the MPI rank

## Examples

### Example 1: ARM Server Profiling

```bash
#!/bin/bash
# profile_arm.sh - Profiling on ARM64 server with cycle counter

export PERFLOW_OUTPUT_DIR="./arm_profile_$(date +%Y%m%d_%H%M%S)"
export PERFLOW_SAMPLING_FREQ=1000
export PERFLOW_TIMER_METHOD=cycle  # Explicitly use cycle counter

mkdir -p "$PERFLOW_OUTPUT_DIR"

LD_PRELOAD=/path/to/libperflow_mpi_sampler_cycle_counter.so \
    mpirun -n 4 ./my_arm_application

echo "Profiling complete. Results in: $PERFLOW_OUTPUT_DIR"
```

### Example 2: Cross-Platform Script

```bash
#!/bin/bash
# profile_crossplatform.sh - Works on any architecture

export PERFLOW_OUTPUT_DIR="./profile_results"
export PERFLOW_SAMPLING_FREQ=500
export PERFLOW_TIMER_METHOD=auto  # Auto-detect best method

mkdir -p "$PERFLOW_OUTPUT_DIR"

# Works on ARM (uses cycle counter) or x86 (uses POSIX timer)
LD_PRELOAD=/path/to/libperflow_mpi_sampler_cycle_counter.so \
    mpirun -n 4 ./my_application

echo "Profiling complete"
```

### Example 3: Compare Timer Methods

```bash
#!/bin/bash
# compare_methods.sh - Compare cycle counter vs POSIX timer (ARM only)

for method in cycle posix; do
    echo "Testing method: $method"
    
    output_dir="results_${method}"
    mkdir -p "$output_dir"
    
    export PERFLOW_OUTPUT_DIR="$output_dir"
    export PERFLOW_SAMPLING_FREQ=1000
    export PERFLOW_TIMER_METHOD=$method
    
    /usr/bin/time -v \
        LD_PRELOAD=/path/to/libperflow_mpi_sampler_cycle_counter.so \
        mpirun -n 4 ./my_application 2>&1 | tee "$output_dir/timing.txt"
    
    echo "---"
done
```

## Technical Details

### ARM Cycle Counter Registers

The sampler uses the following ARM registers:

| Register | Description | Usage |
|----------|-------------|-------|
| `cntvct_el0` | Virtual counter value | Read current cycle count |
| `cntfrq_el0` | Counter frequency | Calculate timing intervals |

These registers are accessible from user space on most ARM systems without special privileges.

### Signal Safety

The signal handler implementation follows signal-safety requirements:
- No dynamic memory allocation in handler
- No standard I/O operations in handler
- Uses atomic operations for counter synchronization
- libunwind is signal-safe for call stack capture

### Memory Usage

- StaticHashMap: 10,000 entries capacity (~1-2 MB)
- CallStack: 100 frames maximum depth per sample
- Library map: Variable (typically <100KB)
- Total per-process footprint: ~2-3 MB

## Additional Resources

- [Timer-Based Sampler Guide](TIMER_SAMPLER_USAGE.md) - Alternative POSIX timer sampler
- [Performance Analysis](TIMER_BASED_SAMPLER.md) - Detailed performance characteristics
- [PerFlow Documentation](../README.md) - Main documentation

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Verify architecture with `uname -m`
3. Check sampler output messages for timer method in use
4. Open an issue on GitHub with:
   - Output of `uname -a`
   - Sampler initialization messages
   - MPI implementation and version
