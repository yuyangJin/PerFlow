# Using the Timer-Based MPI Sampler

The timer-based MPI sampler provides a software-based alternative to the hardware PMU sampler for platforms without hardware performance counter support.

## Quick Start

### 1. Build the Timer-Based Sampler

```bash
cd /path/to/PerFlow
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)
```

This will create `lib/libperflow_mpi_sampler_timer.so`

### 2. Run Your MPI Application

```bash
# Basic usage with default settings (1000 Hz sampling)
LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so \
    mpirun -n 4 ./your_mpi_application

# Custom sampling frequency (500 Hz)
LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so \
    PERFLOW_SAMPLING_FREQ=500 \
    mpirun -n 4 ./your_mpi_application

# Custom output directory
LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so \
    PERFLOW_OUTPUT_DIR=/my/output/dir \
    mpirun -n 4 ./your_mpi_application
```

### 3. Analyze Results

The sampler produces the same output files as the hardware sampler:
- `perflow_mpi_rank_N.pflw` - Binary sample data
- `perflow_mpi_rank_N.txt` - Human-readable sample data
- `perflow_mpi_rank_N.libmap` - Binary library map
- `perflow_mpi_rank_N.libmap.txt` - Human-readable library map

Use PerFlow's analysis tools to visualize:

```bash
# Example: Use online analysis tool
./build/examples/online_analysis_example /my/output/dir /results
```

## Configuration Options

### Environment Variables

The MPI timer preload library (`libperflow_mpi_sampler_timer.so`) reads the following at runtime:

| Variable | Applies | Default | Description |
|----------|---------|---------|-------------|
| `PERFLOW_SAMPLING_FREQ` | All platforms | `1000` | Target sampling rate in Hz (interval or PMU period is derived from this). |
| `PERFLOW_OUTPUT_DIR` | All platforms | `/tmp` | Directory for `perflow_mpi_rank_*` sample and libmap files. Must exist if you set it. |
| `PERFLOW_SAMPLER_BACKEND` | **Linux only** | *(unset)* | `thread` selects the signal-free `/proc` polling backend (single IP per sample). Unset or `pmu` / `hw` uses the default Linux PMU backend (`perf_event` + `SIGIO` + libunwind). Unknown values produce a warning and fall back to PMU. |
| `PERFLOW_PMU_FAST` | Linux, PMU backend | off (`1` enables) | If `1`, record only the interrupted IP (skip libunwind stack walk). |
| `PERFLOW_PMU_PERIOD` | Linux, PMU backend | *(computed)* | Optional cycle period for `perf_event` (overrides Hz-based calculation). |
| `PERFLOW_CPU_FREQ_GHZ` | Linux, PMU backend | `2.6` | Used with `PERFLOW_SAMPLING_FREQ` to compute cycle period when `PERFLOW_PMU_PERIOD` is unset. |
| `PERFLOW_SIGNAL_FAST` | Non-Linux signal backend | off (`1` enables) | IP-only sampling (no libunwind) on the portable SIGUSR1 path. |

For a consolidated guide (including the hardware PMU sampler), see [Configuration](CONFIGURATION.md).

### Recommended Sampling Frequencies

| Frequency | Use Case | Overhead | Data Quality |
|-----------|----------|----------|--------------|
| 100 Hz | Quick profiling, low overhead | ~0.2% | Basic hotspots |
| 500 Hz | General purpose profiling | ~0.8% | Good detail |
| 1000 Hz (default) | Detailed profiling | ~2% | High detail |
| 5000 Hz | Maximum detail (use sparingly) | ~9% | Very high detail |

## Switching Between Samplers

Both samplers produce identical output formats and can be used interchangeably:

```bash
# Hardware PMU sampler (if available)
LD_PRELOAD=/path/to/libperflow_mpi_sampler.so mpirun -n 4 ./app

# Timer-based sampler (always works)
LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so mpirun -n 4 ./app
```

## Testing

Run the included test suite:

```bash
cd /path/to/PerFlow/build

# Test with timer-based sampler
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
    PERFLOW_OUTPUT_DIR=/tmp/test \
    mpirun -n 2 tests/mpi_sampler_test

# Verify output files
ls -l /tmp/test/perflow_mpi_rank_*
```

## Troubleshooting

### No samples collected

**Problem**: Output files are empty or contain no samples.

**Solutions**:
- Ensure the application runs long enough (>1 second)
- Increase sampling frequency
- Check that LD_PRELOAD is set correctly
- Verify output directory is writable

### High overhead

**Problem**: Application runs much slower with sampling enabled.

**Solutions**:
- Reduce sampling frequency (try 100 Hz)
- Check system load (other processes interfering)
- Ensure application is CPU-bound (sampling I/O-bound apps is inefficient)

### Permission issues (Linux PMU backend)

**Problem**: Messages such as `perf_event_open failed`, or no samples when using the default backend.

**Solutions**:
- Check `perf_event_paranoid` and user permissions for performance events; run on a host that allows `perf_event_open`, or use root / appropriate capabilities where policy allows.
- **Alternative**: use the thread backend (no PMU):  
  `export PERFLOW_SAMPLER_BACKEND=thread`

On non-Linux systems the library uses a SIGUSR1-based path; ensure your MPI runtime does not block that signal if you see empty profiles.

### MPI rank not captured

**Problem**: Warning message "MPI rank was not captured, using rank -1"

**Impact**: This is usually harmless. Rank is captured during `MPI_Init()` and some processes may not have it set before finalization. The data is still valid.

## Performance Tips

1. **Start with lower frequencies**: Use 100-500 Hz for initial profiling, then increase if needed

2. **Profile representative workloads**: Ensure your test runs long enough (>5 seconds) to collect meaningful samples

3. **Compare with baseline**: Run your application without sampling to understand overhead

4. **Use multiple frequencies**: Profile at different frequencies to understand sampling effects

## Comparison with Hardware PMU Sampler

| Feature | Timer-Based | Hardware PMU |
|---------|-------------|--------------|
| Platform support | All POSIX systems | PMU-enabled systems only |
| Privileges required | None | May require CAP_PERFMON or root |
| Overhead @ 1000 Hz | ~2% | ~0.5% |
| Precision | Microsecond | Nanosecond |
| Containers/VMs | ✅ Works | ⚠️ May not work |
| Dependencies | MPI, libunwind | MPI, PAPI, libunwind |

## Examples

### Example 1: Basic Profiling

```bash
#!/bin/bash
# profile_app.sh - Simple profiling script

export PERFLOW_OUTPUT_DIR="./profiling_results_$(date +%Y%m%d_%H%M%S)"
export PERFLOW_SAMPLING_FREQ=500

mkdir -p "$PERFLOW_OUTPUT_DIR"

LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so \
    mpirun -n 4 ./my_application

echo "Profiling complete. Results in: $PERFLOW_OUTPUT_DIR"
ls -lh "$PERFLOW_OUTPUT_DIR"
```

### Example 2: Frequency Comparison

```bash
#!/bin/bash
# compare_frequencies.sh - Compare different sampling frequencies

for freq in 100 500 1000 2000; do
    echo "Testing frequency: $freq Hz"
    
    output_dir="results_${freq}hz"
    mkdir -p "$output_dir"
    
    export PERFLOW_OUTPUT_DIR="$output_dir"
    export PERFLOW_SAMPLING_FREQ=$freq
    
    /usr/bin/time -v \
        LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so \
        mpirun -n 4 ./my_application 2>&1 | tee "$output_dir/timing.txt"
    
    echo "---"
done
```

### Example 3: Docker Container

```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    libopenmpi-dev \
    openmpi-bin \
    libunwind-dev

# Copy PerFlow sampler
COPY libperflow_mpi_sampler_timer.so /usr/local/lib/

# Set up environment
ENV LD_PRELOAD=/usr/local/lib/libperflow_mpi_sampler_timer.so
ENV PERFLOW_SAMPLING_FREQ=500
ENV PERFLOW_OUTPUT_DIR=/results

# Run application
CMD ["mpirun", "--allow-run-as-root", "-n", "4", "/app/my_mpi_app"]
```

## Additional Resources

- [Performance Analysis Report](TIMER_BASED_SAMPLER.md) - Detailed performance characteristics
- [PerFlow Documentation](../README.md) - Main documentation
- [Testing Guide](../TESTING.md) - Comprehensive testing information

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review the performance analysis report
3. Open an issue on GitHub with:
   - Output of `uname -a`
   - MPI implementation and version
   - Sampler output and error messages
