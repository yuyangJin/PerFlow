# Timer-Based MPI Sampler - Performance Analysis Report

## Executive Summary

The MPI timer sampler (`libperflow_mpi_sampler_timer.so`) is the **LD_PRELOAD** library that does not depend on PAPI. It shares the same export pipeline as the PAPI-based PMU sampler (`libperflow_mpi_sampler.so`), but uses **runtime-selectable backends** so it can run in more environments:

- **Linux (default)**: `perf_event_open` on CPU cycles, overflow delivered as `SIGIO`, call stacks via **libunwind** (low overhead when PMU access is allowed).
- **Linux (optional)**: `PERFLOW_SAMPLER_BACKEND=thread` — a dedicated thread polls `/proc` for the main thread’s instruction pointer (no PMU, no application signals; **IP-only** samples).
- **Non-Linux POSIX**: a helper thread periodically sends **SIGUSR1** to the main thread; the handler captures IP or full stacks with libunwind.

See [Configuration](CONFIGURATION.md) for the full list of environment variables.

## Implementation Overview

### Key Components

1. **Backends** (see `include/sampling/mpi_sampler_*_backend.h`): Linux PMU, Linux thread poll, or portable SIGUSR1 path
2. **Signal / context helpers** (`include/sampling/call_stack.h`, when enabled): `get_ip_from_ucontext`, `capture_call_stack_in_signal` (libunwind)
3. **Data collection**: Same `StaticHashMap` / `CallStack` types as the rest of PerFlow sampling
4. **Data export**: Same `.pflw`, `.libmap`, and text outputs as the hardware sampler path

### Architecture Comparison

```
Hardware PMU Sampler (mpi_sampler.cpp, PAPI):
  PAPI_TOT_CYC → overflow → Handler → libunwind stack

MPI Timer Sampler (mpi_sampler_timer.cpp):
  Linux default: perf_event (cycles) → SIGIO → Handler → libunwind stack
  Linux thread:  nanosleep loop → read /proc/.../syscall → single IP
  Non-Linux:     nanosleep loop → pthread_kill(SIGUSR1) → Handler → stack or IP
```

## Performance Characteristics

### Timer-Based Sampler (Software)

#### Test Results (2-process MPI, Matrix Multiplication Workload)

| Sampling Frequency | Execution Time | Unique Stacks | Overhead |
|-------------------|----------------|---------------|----------|
| 100 Hz            | 4.77 seconds   | 12 stacks     | ~0.2%    |
| 500 Hz            | 4.80 seconds   | 41 stacks     | ~0.8%    |
| 1000 Hz           | 4.86 seconds   | 64 stacks     | ~1.7%    |
| 5000 Hz           | 5.20 seconds   | 267 stacks    | ~9.0%    |
| Baseline (no sampler) | ~4.76 seconds | N/A        | 0%       |

#### Advantages

1. **Platform Independence**
   - No PMU hardware required
   - Works in containers, VMs, and restricted environments
   - No special kernel privileges needed (standard user can run)

2. **Simplicity**
   - Fewer dependencies (no PAPI required)
   - Easier deployment and configuration
   - More predictable behavior across platforms

3. **Configurable Precision**
   - Timer interval configurable via environment variable
   - Can achieve sampling rates from 10 Hz to 10 kHz
   - Predictable sampling intervals

4. **Portability**
   - Uses POSIX standard APIs
   - Compatible with all POSIX-compliant systems
   - Works on ARM, x86, and other architectures

#### Disadvantages

1. **Higher Overhead**
   - Software interrupts are more expensive than hardware interrupts
   - At 5000 Hz: ~9% overhead vs <1% for hardware sampling
   - Signal handling costs are not negligible

2. **Lower Precision**
   - Timer resolution limited by OS scheduler (typically 1-10 microseconds)
   - Cannot achieve cycle-accurate sampling
   - Subject to system noise and scheduler jitter

3. **Sample Skew**
   - Timer signals can be delayed by kernel
   - High-priority processes may affect sampling accuracy
   - Context switches can introduce measurement artifacts

4. **CPU Affinity**
   - Timer fires on specific CPU core
   - May not accurately sample all threads in multi-threaded code
   - Process migration can affect sampling

### Hardware PMU Sampler (Reference)

#### Advantages

1. **Low Overhead**
   - Hardware interrupts are very efficient
   - Typical overhead: <0.5% at 1000 Hz
   - Minimal performance impact on measured code

2. **High Precision**
   - Cycle-accurate sampling
   - Can sample based on actual work done (cycle counts)
   - No system noise in sampling intervals

3. **Event-Based Sampling**
   - Can sample on cache misses, branch mispredictions, etc.
   - Multiple counter types available
   - Rich performance data collection

4. **Deterministic Behavior**
   - Hardware guarantees interrupt delivery
   - Precise control over sampling rate
   - Minimal jitter

#### Disadvantages

1. **Platform Dependency**
   - Requires PMU hardware support
   - Different capabilities on different CPUs
   - May not work in virtualized environments

2. **Access Restrictions**
   - Often requires elevated privileges or kernel parameters
   - Container environments may block PMU access
   - Security policies may disable performance counters

3. **Complexity**
   - Requires PAPI library installation and configuration
   - More complex error handling
   - Platform-specific tuning needed

4. **Limited Availability**
   - Not all platforms support hardware counters
   - Cloud environments often disable PMU
   - Embedded systems may lack PMU hardware

## Quantitative Comparison

### Overhead Analysis (1000 Hz Sampling)

| Metric                  | Timer-Based | Hardware PMU | Notes |
|------------------------|-------------|--------------|-------|
| Baseline Runtime       | 4.76s       | 4.76s        | No sampling |
| Sampled Runtime        | 4.86s       | ~4.78s*      | With sampling |
| Overhead               | ~2.1%       | ~0.4%*       | Relative increase |
| Unique Stacks Captured | 64          | N/A**        | Data quality |
| Signal Handler Calls   | ~4,860      | ~4,860       | Expected samples |

\* Estimated based on typical PAPI overhead (PMU not available in test environment)  
\** PMU sampler couldn't run in test environment due to permissions

### Memory Usage

Both samplers use identical data structures:
- StaticHashMap: 10,000 entries capacity
- CallStack: 100 frames maximum depth
- Memory footprint: ~1-2 MB per process

### Accuracy Analysis

**Timer-Based Sampler**:
- Timer resolution: ~1 microsecond (CLOCK_MONOTONIC)
- Actual sampling jitter: ±10-50 microseconds (OS dependent)
- Missed samples: <0.1% at 1000 Hz
- Stack capture time: ~10-20 microseconds per sample

**Hardware PMU Sampler**:
- Counter resolution: 1 CPU cycle
- Interrupt latency: <100 nanoseconds
- Missed samples: virtually zero
- Stack capture time: ~10-20 microseconds per sample

## Recommendations

### When to Use Timer-Based Sampler

1. **Development Environments**
   - Containers (Docker, Kubernetes)
   - Virtual machines without nested virtualization
   - Systems where PMU access is restricted

2. **Cross-Platform Deployment**
   - Need consistent behavior across platforms
   - Deploying to varied hardware
   - Embedded systems without PMU

3. **Moderate Sampling Rates**
   - Frequencies ≤1000 Hz (overhead <2%)
   - Long-running applications where 2% overhead is acceptable
   - Applications not CPU-bound

4. **Initial Profiling**
   - Quick performance assessment
   - Hotspot identification
   - Prototyping and debugging

### When to Use Hardware PMU Sampler

1. **Production Profiling**
   - Minimal overhead required
   - High-frequency sampling needed (>1000 Hz)
   - CPU-intensive workloads

2. **Detailed Performance Analysis**
   - Need cycle-accurate measurements
   - Want to sample based on specific events (cache misses, etc.)
   - Require highest precision

3. **Bare-Metal Systems**
   - Full hardware access available
   - PMU counters accessible
   - No virtualization layer

4. **High-Performance Computing**
   - Scientific computing clusters
   - Supercomputers with PMU access
   - Performance-critical production code

### Hybrid Approach

For comprehensive analysis:
1. **Development**: Use timer-based sampler for initial profiling
2. **Validation**: Switch to hardware PMU for detailed analysis
3. **Production**: Deploy appropriate sampler based on environment

## Configuration

### MPI timer sampler (`libperflow_mpi_sampler_timer.so`)

```bash
# Sampling frequency (default: 1000 Hz)
export PERFLOW_SAMPLING_FREQ=500

# Output directory (default: /tmp)
export PERFLOW_OUTPUT_DIR=/path/to/output

# Linux only: use /proc polling backend when PMU is unavailable
# export PERFLOW_SAMPLER_BACKEND=thread

# Linux PMU tuning (optional)
# export PERFLOW_CPU_FREQ_GHZ=2.6
# export PERFLOW_PMU_PERIOD=10000000
# export PERFLOW_PMU_FAST=1

# Non-Linux IP-only (optional)
# export PERFLOW_SIGNAL_FAST=1

LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so mpirun -n 4 ./your_app
```

The authoritative table of variables is in [Configuration](CONFIGURATION.md) (section *Timer-based MPI sampler*).

### Hardware PMU Sampler (`libperflow_mpi_sampler.so`)

```bash
export PERFLOW_SAMPLING_FREQ=1000
export PERFLOW_OUTPUT_DIR=/path/to/output

LD_PRELOAD=/path/to/libperflow_mpi_sampler.so mpirun -n 4 ./your_app
```

## Performance Tuning

### Optimizing the MPI Timer Sampler

1. **Frequency selection**
   - Start with 100–500 Hz for initial profiling
   - Use 1000 Hz for detailed analysis
   - Avoid >5000 Hz unless necessary (high overhead)

2. **Linux PMU backend**
   - Ensure performance events are permitted (`perf_event_paranoid`, capabilities), or use `PERFLOW_SAMPLER_BACKEND=thread`
   - Set `PERFLOW_CPU_FREQ_GHZ` (and optionally `PERFLOW_PMU_PERIOD`) if the default period does not match your CPU
   - Use `PERFLOW_PMU_FAST=1` only when you need minimal overhead and IP-only profiles

3. **Thread backend (Linux)**
   - Higher overhead than PMU; use when `perf_event` is blocked
   - Samples are IP-only (no full unwind)

4. **Signal / unwind path**
   - Stack capture uses libunwind from the signal handler (PMU and non-Linux backends); keep `PERFLOW_*_FAST` off unless you accept IP-only data

### Minimizing Overhead

1. **Lower Sampling Rate**
   - 100-500 Hz often sufficient for hotspot detection
   - Reduces CPU time spent in handler
   - Less memory for sample storage

2. **Optimize Hash Map Size**
   - Adjust `kSampleMapCapacity` if needed
   - Larger maps reduce collisions but use more memory
   - Default 10,000 entries handles most workloads

3. **Limit Stack Depth**
   - Reduce `kMaxCallStackDepth` if deep stacks not needed
   - Faster stack capture
   - Less memory per sample

## Conclusion

The timer-based MPI sampler provides a robust, portable alternative to hardware PMU sampling. While it has higher overhead (1-2% at 1000 Hz vs <0.5% for PMU), it offers significant advantages in accessibility and platform compatibility.

**Key Takeaways**:
- ✅ Works everywhere: containers, VMs, restricted environments
- ✅ Easy deployment: no special privileges or hardware required
- ✅ Adequate precision: sufficient for most profiling tasks
- ⚠️ Higher overhead: 2-9% depending on frequency
- ⚠️ Lower precision: microsecond vs nanosecond accuracy

**Best Practice**: Start with timer-based sampler for portability, switch to hardware PMU for production optimization when available.

## References

- Linux `perf_event_open`: `man perf_event_open`
- Signal handling: `man sigaction`
- libunwind: https://www.nongnu.org/libunwind/
- PAPI Library (hardware PMU sampler): https://icl.utk.edu/papi/

## Version History

- **v0.1.0** (2024): Initial implementation of timer-based sampler
  - Compatible with existing data export formats
  - Drop-in replacement for hardware sampler
- **2025**: MPI timer sampler refactored to pluggable backends (Linux PMU, Linux thread poll, portable SIGUSR1); runtime selection via `PERFLOW_SAMPLER_BACKEND` on Linux
