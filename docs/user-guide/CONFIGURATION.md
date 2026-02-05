# Configuration Guide

This guide covers all configuration options available in PerFlow for fine-tuning your performance profiling.

## Environment Variables

All PerFlow configuration is done through environment variables. These are set before running your MPI application with LD_PRELOAD.

### Required Variables

#### PERFLOW_OUTPUT_DIR
**Type**: String (directory path)  
**Required**: Yes  
**Description**: Directory where sample data files will be written

Each MPI rank will create a file named `rank_N.pflw` in this directory.

```bash
PERFLOW_OUTPUT_DIR=/tmp/samples
```

⚠️ **Important**: The directory must exist and be writable before running your application.

### Sampling Configuration

#### PERFLOW_SAMPLING_FREQ
**Type**: Integer (Hz)  
**Required**: Timer sampler only  
**Default**: 1000  
**Range**: 100 - 10000  
**Description**: Sampling frequency in samples per second

Higher frequencies provide more detailed profiles but increase overhead.

```bash
PERFLOW_SAMPLING_FREQ=5000  # 5kHz sampling
```

**Recommended values**:
- **100-500 Hz**: Long-running applications, minimal overhead
- **1000 Hz**: Default, good balance (1ms between samples)
- **2000-5000 Hz**: Short runs, detailed profiling
- **>5000 Hz**: Expert use only, check overhead

💡 **Tip**: For hardware PMU sampler, frequency is configured in code, not via environment variables.

### Call Stack Configuration

#### PERFLOW_MAX_STACK_DEPTH
**Type**: Integer  
**Default**: 128  
**Range**: 16 - 256  
**Description**: Maximum number of frames to capture in each call stack

Deeper stacks provide more context but use more memory and processing time.

```bash
PERFLOW_MAX_STACK_DEPTH=64  # Limit to 64 frames
```

**When to adjust**:
- **Decrease (32-64)**: Very deep call hierarchies, memory constrained
- **Increase (256)**: Need full context for deeply nested code

### Compression Configuration

#### PERFLOW_ENABLE_COMPRESSION
**Type**: Boolean (0 or 1)  
**Default**: 0 (disabled)  
**Description**: Enable zlib compression for sample data files

Reduces disk I/O and file size by ~60-80% but adds CPU overhead.

```bash
PERFLOW_ENABLE_COMPRESSION=1  # Enable compression
```

**When to enable**:
- ✅ Slow disk I/O (network storage, HDDs)
- ✅ Limited disk space
- ✅ Long-running applications with many samples

**When to disable**:
- ✅ Fast local SSDs
- ✅ CPU-constrained systems
- ✅ Real-time analysis requirements

### Debug and Logging

#### PERFLOW_DEBUG
**Type**: Boolean (0 or 1)  
**Default**: 0 (disabled)  
**Description**: Enable debug logging to stderr

```bash
PERFLOW_DEBUG=1  # Enable debug output
```

⚠️ **Warning**: Debug output can be verbose and impact performance. Use only for troubleshooting.

## Sampler Selection

### Hardware PMU Sampler

**Library**: `libperflow_mpi_sampler.so`  
**Requirements**: PAPI library, PMU access  
**Overhead**: <0.5% @ 1kHz  
**Use case**: Production profiling on bare metal

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
mpirun -n 4 ./your_app
```

**PMU Access Requirements**:
- Run as root, OR
- Set `/proc/sys/kernel/perf_event_paranoid` to -1 or 0, OR
- Grant CAP_SYS_ADMIN capability

```bash
# Temporary (requires root)
sudo sh -c 'echo -1 > /proc/sys/kernel/perf_event_paranoid'

# Permanent (requires root, edit /etc/sysctl.conf)
kernel.perf_event_paranoid = -1
```

### Timer-Based Sampler

**Library**: `libperflow_mpi_sampler_timer.so`  
**Requirements**: POSIX timers (always available)  
**Overhead**: ~1-2% @ 1kHz  
**Use case**: Containers, VMs, or when PMU access is unavailable

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 4 ./your_app
```

## Configuration Examples

### Example 1: Default Configuration
Balanced settings for most use cases:

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 8 ./my_mpi_app
```

### Example 2: High-Frequency Profiling
Detailed profiling for short runs:

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=5000 \
PERFLOW_MAX_STACK_DEPTH=64 \
mpirun -n 4 ./short_test
```

### Example 3: Long-Running with Compression
Optimized for long runs with limited disk space:

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=500 \
PERFLOW_ENABLE_COMPRESSION=1 \
mpirun -n 16 ./long_simulation
```

### Example 4: Debug Mode
Troubleshooting configuration issues:

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=100 \
PERFLOW_DEBUG=1 \
mpirun -n 2 ./test_app 2>&1 | tee debug.log
```

### Example 5: Hardware PMU (Production)
Lowest overhead for production profiling:

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/data/perflow/run_001 \
mpirun -n 64 ./production_app
```

## Performance Considerations

### Sampling Frequency vs. Overhead

| Frequency | Overhead (Timer) | Overhead (PMU) | Use Case |
|-----------|------------------|----------------|----------|
| 100 Hz    | <0.5%           | <0.1%          | Production, long runs |
| 500 Hz    | ~0.5-1%         | <0.3%          | Standard profiling |
| 1000 Hz   | ~1-2%           | <0.5%          | Default, balanced |
| 2000 Hz   | ~2-3%           | ~0.8%          | Detailed analysis |
| 5000 Hz   | ~5-8%           | ~1.5%          | Short runs, expert use |
| 10000 Hz  | ~10-15%         | ~3%            | Not recommended |

### Memory Usage

**Per-rank memory usage** (approximate):
- Base overhead: ~1-2 MB
- Per unique call stack: ~1 KB (average)
- Typical usage: 10-50 MB per rank

**Calculation**:
```
Memory ≈ (num_unique_stacks × 1KB) + 2MB
```

💡 **Tip**: Applications with diverse execution paths will have more unique call stacks.

### Disk Space

**Sample file sizes** (uncompressed):
```
File size ≈ (samples_per_second × runtime_seconds × 500 bytes) / rank
```

**Example**: 
- 1000 Hz sampling
- 60 second run
- 4 ranks
- File size: ~120 MB (30 MB per rank)

With compression: ~25-40 MB total (~70% reduction)

## Online Analysis Configuration

When using the online analysis module programmatically:

```cpp
#include "analysis/online_analysis.h"

OnlineAnalysis analysis;

// Set tree build mode
analysis.builder().set_build_mode(TreeBuildMode::kContextFree);  // Default
// or
analysis.builder().set_build_mode(TreeBuildMode::kContextAware);  // Full context

// Configure symbol resolution
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback,  // Strategy
    true  // Enable caching
);

// Build tree with configuration
analysis.builder().build_from_files(
    {
        {"/tmp/samples/rank_0.pflw", 0},
        {"/tmp/samples/rank_1.pflw", 1}
    },
    1000.0  // Sampling frequency for time estimation
);
```

### Tree Build Modes

**Context-Free Mode** (default):
- Merges nodes with same function/library
- More compact trees
- Better for high-level overview
- Recommended for most use cases

**Context-Aware Mode**:
- Distinguishes nodes by calling context
- Larger trees with full call path information
- Better for understanding call relationships
- Use when you need precise call site information

### Symbol Resolution Strategies

```cpp
// Fast: Only exported symbols (dladdr)
SymbolResolver::Strategy::kDlAddrOnly

// Comprehensive: All symbols (addr2line)
SymbolResolver::Strategy::kAddr2LineOnly

// Automatic: Try dladdr first, fallback to addr2line
SymbolResolver::Strategy::kAutoFallback  // Recommended
```

## Troubleshooting Configuration

### Problem: No sample files generated

**Check**:
1. Output directory exists and is writable
2. LD_PRELOAD path is correct
3. Application actually runs (not immediate crash)

```bash
# Verify setup
mkdir -p /tmp/samples
ls -ld /tmp/samples
export LD_PRELOAD=$(pwd)/build/lib/libperflow_mpi_sampler_timer.so
echo $LD_PRELOAD
ldd $LD_PRELOAD  # Check library dependencies
```

### Problem: High overhead

**Solutions**:
1. Reduce sampling frequency
2. Switch to hardware PMU sampler
3. Reduce max stack depth
4. Disable compression (if enabled)

```bash
# Lower overhead configuration
PERFLOW_SAMPLING_FREQ=500 \
PERFLOW_MAX_STACK_DEPTH=64 \
PERFLOW_ENABLE_COMPRESSION=0
```

### Problem: Missing symbols in analysis

**Solutions**:
1. Compile with debug symbols (`-g`)
2. Use addr2line strategy for comprehensive resolution
3. Ensure shared libraries are not stripped

```bash
# Compile with debug info
mpicxx -g -O2 -o my_app my_app.cpp
```

## Best Practices

✅ **DO**:
- Start with default settings (1kHz, timer sampler)
- Profile representative workloads
- Test on small scale before large runs
- Monitor disk usage for long runs
- Use compression for network storage

❌ **DON'T**:
- Use extremely high frequencies (>5kHz) without testing
- Profile on systems without enough disk space
- Enable debug mode in production
- Forget to create output directory
- Use PMU sampler without proper permissions

## See Also

- [Getting Started Guide](GETTING_STARTED.md) - Installation and first steps
- [Troubleshooting Guide](TROUBLESHOOTING.md) - Common issues and solutions
- [Timer Sampler Usage](../TIMER_SAMPLER_USAGE.md) - Detailed timer sampler documentation
- [Online Analysis API](../ONLINE_ANALYSIS_API.md) - Programmatic analysis
