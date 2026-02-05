# Troubleshooting Guide

This guide helps you diagnose and resolve common issues when using PerFlow.

## Table of Contents
- [Installation Issues](#installation-issues)
- [Runtime Issues](#runtime-issues)
- [Analysis Issues](#analysis-issues)
- [Performance Issues](#performance-issues)
- [Environment-Specific Issues](#environment-specific-issues)

---

## Installation Issues

### CMake Cannot Find PAPI

**Symptoms**:
```
-- PAPI not found - skipping MPI sampler build
```

**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install libpapi-dev

# RHEL/CentOS/Fedora
sudo dnf install papi-devel

# Verify installation
pkg-config --modversion papi
```

**Alternative**: Use timer-based sampler (doesn't require PAPI)

---

### CMake Cannot Find MPI

**Symptoms**:
```
-- MPI not found - skipping MPI sampler build
```

**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install libopenmpi-dev openmpi-bin

# RHEL/CentOS/Fedora
sudo dnf install openmpi-devel
module load mpi/openmpi-x86_64  # May be needed on RHEL

# Verify installation
which mpirun
mpirun --version
```

---

### libunwind Not Found

**Symptoms**:
```
/usr/bin/ld: cannot find -lunwind
```

**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install libunwind-dev

# RHEL/CentOS/Fedora
sudo dnf install libunwind-devel

# macOS
brew install libunwind
```

---

## Runtime Issues

### No Sample Files Generated

**Symptoms**: Output directory is empty after running application

**Diagnostic Steps**:
```bash
# 1. Verify output directory exists
ls -ld /tmp/samples
# If not: mkdir -p /tmp/samples

# 2. Check LD_PRELOAD is set correctly
echo $LD_PRELOAD
# Should point to libperflow_mpi_sampler*.so

# 3. Verify library can be loaded
ldd $LD_PRELOAD

# 4. Enable debug mode
PERFLOW_DEBUG=1 \
LD_PRELOAD=... \
PERFLOW_OUTPUT_DIR=/tmp/samples \
mpirun -n 2 ./your_app
```

**Common Causes**:
1. **Output directory doesn't exist** → Create it first
2. **Wrong LD_PRELOAD path** → Use absolute path or relative from correct directory
3. **Library dependencies missing** → Check ldd output
4. **Application crashes before MPI_Init** → PerFlow only starts after MPI_Init
5. **Permissions issue** → Check directory write permissions

---

### "Permission denied" PMU Access

**Symptoms** (Hardware PMU sampler):
```
Error: Cannot access PMU events
perf_event_open failed: Permission denied
```

**Solutions**:

**Option 1: Temporary (requires root)**
```bash
sudo sh -c 'echo -1 > /proc/sys/kernel/perf_event_paranoid'
```

**Option 2: Permanent (requires root)**
```bash
# Add to /etc/sysctl.conf
kernel.perf_event_paranoid = -1

# Apply
sudo sysctl -p
```

**Option 3: Use Timer Sampler Instead**
```bash
# Switch to timer-based sampler (no special permissions needed)
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 4 ./your_app
```

---

### Segmentation Fault During Sampling

**Symptoms**: Application crashes with SIGSEGV during execution

**Diagnostic Steps**:
```bash
# 1. Run with reduced stack depth
PERFLOW_MAX_STACK_DEPTH=32 \
LD_PRELOAD=... \
mpirun -n 2 ./your_app

# 2. Check for conflicts with other LD_PRELOAD libraries
echo $LD_PRELOAD  # Should only contain PerFlow library

# 3. Run without PerFlow to verify application works
mpirun -n 2 ./your_app

# 4. Build PerFlow with sanitizers
cd build
cmake .. -DPERFLOW_ENABLE_SANITIZERS=ON
make clean && make
```

**Common Causes**:
1. **Stack overflow** → Reduce PERFLOW_MAX_STACK_DEPTH
2. **Conflicting signal handlers** → Check if app uses SIGPROF (timer sampler) or SIGIO (PMU sampler)
3. **Memory corruption in application** → Test without PerFlow
4. **Thread-safety issues** → Check if app uses threads with MPI

---

### "Cannot allocate memory"

**Symptoms**:
```
Error: Cannot allocate memory for sample buffer
```

**Solutions**:
```bash
# 1. Reduce stack depth
PERFLOW_MAX_STACK_DEPTH=64

# 2. Check available memory
free -h

# 3. Reduce number of MPI ranks per node
mpirun -n 2 --map-by node ...  # Fewer ranks per node
```

---

## Analysis Issues

### Analysis Tools Cannot Find Sample Files

**Symptoms**:
```
Error: Cannot open file /tmp/samples/rank_0.pflw
```

**Solutions**:
```bash
# 1. Verify files exist
ls -lh /tmp/samples/*.pflw

# 2. Check file permissions
chmod 644 /tmp/samples/*.pflw

# 3. Verify file format
hexdump -C /tmp/samples/rank_0.pflw | head -4
# Should start with: 50 46 4c 57 (ASCII: PFLW)
```

---

### "No symbols found" in Analysis

**Symptoms**: All function names show as `<unknown>` or hex addresses

**Solutions**:

**1. Compile with debug symbols**
```bash
# Add -g flag
mpicxx -g -O2 -o my_app my_app.cpp

# Verify debug symbols present
nm my_app | head -10
```

**2. Use comprehensive symbol resolution**
```cpp
// In your analysis code
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAddr2LineOnly,  // Use addr2line
    true  // Enable caching
);
```

**3. Ensure shared libraries aren't stripped**
```bash
# Check if libraries are stripped
file /path/to/libmylib.so
# Should NOT say "stripped"

# If stripped, rebuild without stripping
strip -K '*' libmylib.so  # Don't strip, or
# Rebuild library with debug symbols
```

---

### Imbalance Analysis Shows 0.0

**Symptoms**: Balance analysis reports 0% imbalance when imbalance exists

**Causes**:
1. **Application is truly balanced** → Expected
2. **Sampling frequency too low** → Increase PERFLOW_SAMPLING_FREQ
3. **Sampling duration too short** → Run application longer

**Solutions**:
```bash
# Increase sampling frequency for short runs
PERFLOW_SAMPLING_FREQ=2000 \
LD_PRELOAD=... \
mpirun -n 4 ./your_app

# Ensure application runs long enough (>10 seconds recommended)
```

---

### Tree Visualization Fails

**Symptoms**:
```
Error: Cannot generate PDF visualization
dot: command not found
```

**Solution**:
```bash
# Install GraphViz
# Ubuntu/Debian
sudo apt-get install graphviz

# RHEL/CentOS/Fedora
sudo dnf install graphviz

# macOS
brew install graphviz

# Verify installation
which dot
dot -V
```

---

## Performance Issues

### High Overhead (>5%)

**Symptoms**: Application runs much slower with PerFlow

**Solutions**:

**1. Reduce sampling frequency**
```bash
# Try lower frequency
PERFLOW_SAMPLING_FREQ=500  # Instead of 1000
```

**2. Switch to hardware PMU sampler**
```bash
# Much lower overhead than timer sampler
LD_PRELOAD=lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
mpirun -n 4 ./your_app
```

**3. Reduce stack depth**
```bash
PERFLOW_MAX_STACK_DEPTH=32  # Instead of 128
```

**4. Disable compression**
```bash
PERFLOW_ENABLE_COMPRESSION=0  # If it was enabled
```

**5. Use faster storage**
```bash
# Use local SSD instead of network storage
PERFLOW_OUTPUT_DIR=/local/ssd/samples
```

---

### Very Large Sample Files

**Symptoms**: Sample files are gigabytes in size

**Solutions**:

**1. Enable compression**
```bash
PERFLOW_ENABLE_COMPRESSION=1
```

**2. Reduce sampling frequency**
```bash
PERFLOW_SAMPLING_FREQ=500  # Instead of 1000
```

**3. Profile shorter durations**
```bash
# Add time limit to your application
timeout 60 mpirun -n 4 ./your_app
```

**4. Clean old sample files**
```bash
# Remove old samples before new runs
rm -f /tmp/samples/*.pflw
```

---

### Slow Analysis Processing

**Symptoms**: Analysis takes very long time

**Solutions**:

**1. Use context-free mode** (default, faster)
```cpp
analysis.builder().set_build_mode(TreeBuildMode::kContextFree);
```

**2. Limit symbol resolution scope**
```cpp
// Only resolve hotspots, not all nodes
auto hotspots = analysis.find_hotspots(10);
for (auto& h : hotspots) {
    // Resolve symbols only for top 10 hotspots
}
```

**3. Use symbol caching**
```cpp
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback,
    true  // Enable caching - important!
);
```

---

## Environment-Specific Issues

### Container/Docker Issues

**Symptoms**: PerFlow doesn't work in containers

**Solutions**:

**1. Use timer sampler** (PMU may not be available)
```bash
docker run ... \
  -v /path/to/PerFlow/build/lib:/perflow \
  -e LD_PRELOAD=/perflow/libperflow_mpi_sampler_timer.so \
  -e PERFLOW_OUTPUT_DIR=/tmp/samples \
  -e PERFLOW_SAMPLING_FREQ=1000 \
  my_mpi_container
```

**2. Mount output directory**
```bash
docker run -v /host/samples:/tmp/samples ...
```

**3. Grant PMU access (if needed)**
```bash
docker run --privileged ...  # Or
docker run --cap-add=SYS_ADMIN ...
```

---

### Slurm/PBS/Job Scheduler Issues

**Symptoms**: PerFlow doesn't work in HPC job scripts

**Solutions**:

**Slurm Example**
```bash
#!/bin/bash
#SBATCH -N 4
#SBATCH -n 16
#SBATCH --time=01:00:00

module load openmpi

export LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so
export PERFLOW_OUTPUT_DIR=$SLURM_SUBMIT_DIR/samples_$SLURM_JOB_ID
export PERFLOW_SAMPLING_FREQ=1000

mkdir -p $PERFLOW_OUTPUT_DIR

srun ./my_mpi_app
```

**PBS Example**
```bash
#!/bin/bash
#PBS -l nodes=4:ppn=4
#PBS -l walltime=01:00:00

cd $PBS_O_WORKDIR
module load openmpi

export LD_PRELOAD=/path/to/libperflow_mpi_sampler_timer.so
export PERFLOW_OUTPUT_DIR=$PBS_O_WORKDIR/samples_$PBS_JOBID
export PERFLOW_SAMPLING_FREQ=1000

mkdir -p $PERFLOW_OUTPUT_DIR

mpirun ./my_mpi_app
```

---

### macOS-Specific Issues

**Symptoms**: Various issues on macOS

**Known Limitations**:
- Hardware PMU sampler may not work (use timer sampler)
- Some signal handling differences

**Solutions**:
```bash
# Always use timer sampler on macOS
LD_PRELOAD=build/lib/libperflow_mpi_sampler_timer.dylib \  # Note: .dylib
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 4 ./your_app
```

---

## Debugging Tips

### Enable Verbose Logging

```bash
PERFLOW_DEBUG=1 \
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 2 ./your_app 2>&1 | tee perflow_debug.log
```

### Verify Library Loading

```bash
# Check if PerFlow library is loaded
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
mpirun -n 1 bash -c 'cat /proc/self/maps | grep perflow'
```

### Minimal Test Case

```bash
# Test with simple MPI program
cd PerFlow/build

# Compile simple test
cat > test.c << 'EOF'
#include <mpi.h>
#include <unistd.h>
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    sleep(5);  // Run for 5 seconds
    MPI_Finalize();
    return 0;
}
EOF

mpicc test.c -o test

# Run with PerFlow
mkdir -p /tmp/test_samples
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/test_samples \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 2 ./test

# Check results
ls -lh /tmp/test_samples/
```

---

## Getting Help

If you've tried these solutions and still have issues:

1. **Check Documentation**
   - [Getting Started Guide](GETTING_STARTED.md)
   - [Configuration Guide](CONFIGURATION.md)
   - [API Documentation](../ONLINE_ANALYSIS_API.md)

2. **Search Issues**
   - Check [GitHub Issues](https://github.com/yuyangJin/PerFlow/issues) for similar problems

3. **Report a Bug**
   - Open a new issue with:
     - PerFlow version
     - Operating system and version
     - MPI implementation and version
     - Steps to reproduce
     - Error messages and logs
     - Output of `cmake` and `make`

4. **Provide Debug Information**
   ```bash
   # Include this information in bug reports
   uname -a
   mpicxx --version
   cmake --version
   ldd build/lib/libperflow_mpi_sampler_timer.so
   
   # Run with debug enabled
   PERFLOW_DEBUG=1 mpirun -n 2 ./your_app 2>&1 | tee debug.log
   ```

---

## Common Error Messages

| Error Message | Likely Cause | Solution |
|--------------|--------------|----------|
| `perf_event_open: Permission denied` | No PMU access | Use timer sampler or fix permissions |
| `Cannot open output file` | Directory doesn't exist | Create output directory |
| `Symbol resolution failed` | No debug symbols | Compile with `-g` flag |
| `Cannot allocate memory` | Insufficient memory | Reduce stack depth or ranks |
| `dot: command not found` | GraphViz not installed | Install graphviz package |
| `PAPI_library_init: -1` | PAPI initialization failed | Check PAPI installation |
| `MPI_Init not called` | Application structure issue | PerFlow requires MPI_Init |

---

## See Also

- [Getting Started Guide](GETTING_STARTED.md)
- [Configuration Guide](CONFIGURATION.md)
- [Timer Sampler Documentation](../TIMER_SAMPLER_USAGE.md)
- [Testing Guide](../../TESTING.md)
