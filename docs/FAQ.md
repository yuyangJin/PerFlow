# Frequently Asked Questions (FAQ)

## General Questions

### What is PerFlow?
PerFlow is a high-performance profiling tool for parallel MPI applications. It provides ultra-low overhead sampling (<0.5% @ 1kHz), symbol resolution for function names and source locations, and comprehensive online analysis capabilities including workload balance analysis and hotspot detection.

### Who should use PerFlow?
PerFlow is designed for:
- HPC developers optimizing parallel applications
- Performance engineers analyzing scalability
- Researchers profiling scientific codes
- System administrators monitoring production workloads

### How is PerFlow different from other profilers?
PerFlow offers:
- **Ultra-low overhead**: <0.5% with hardware PMU sampling
- **Production-ready**: Safe for use in production environments
- **Platform-independent option**: Timer-based sampler works everywhere
- **Online analysis**: Real-time performance tree generation
- **MPI-aware**: Designed specifically for parallel applications

## Installation & Setup

### What are the minimum requirements?
- C++17 compiler (GCC 7+, Clang 5+)
- CMake 3.14+
- MPI (OpenMPI, MPICH, or Intel MPI)
- libunwind

Optional but recommended: PAPI (for PMU sampling), zlib (compression), GraphViz (visualization)

### Do I need root access to use PerFlow?
**No, not for the timer-based sampler.** The timer sampler works without special privileges.

**Yes, for hardware PMU sampler** you need either:
- Root access, OR
- PMU permissions configured (`/proc/sys/kernel/perf_event_paranoid`), OR
- CAP_SYS_ADMIN capability

### Can I use PerFlow in containers or VMs?
**Yes!** Use the timer-based sampler (`libperflow_mpi_sampler_timer.so`), which works everywhere without special permissions. The hardware PMU sampler may not be available in containers/VMs unless PMU passthrough is configured.

### Does PerFlow work on ARM architectures?
Yes, PerFlow is designed for ARMv9 supercomputing systems and works on ARM architectures. Both timer and PMU samplers are supported.

### What MPI implementations are supported?
PerFlow supports all major MPI implementations:
- OpenMPI
- MPICH
- Intel MPI
- Cray MPI
- And others that follow the MPI standard

## Usage & Configuration

### Which sampling mode should I use?
**Start with timer-based sampler** - It's easier to set up and works everywhere.

**Use hardware PMU sampler** when:
- You need the absolute lowest overhead (<0.5%)
- You have PMU access permissions
- You're running on bare metal (not containers/VMs)

### What sampling frequency should I use?
**Recommended starting point: 1000 Hz (1kHz)**

Adjust based on your needs:
- **100-500 Hz**: Long-running applications, minimal overhead
- **1000 Hz**: Default, good balance
- **2000-5000 Hz**: Short runs, detailed profiling
- **>5000 Hz**: Expert use only, check overhead

### How much overhead does PerFlow add?
- **Hardware PMU sampler**: <0.5% @ 1kHz
- **Timer-based sampler**: ~1-2% @ 1kHz

Overhead increases with higher sampling frequencies.

### How much disk space do I need?
Approximate formula:
```
File size ≈ (samples_per_second × runtime_seconds × 500 bytes) / rank
```

Example: 1000 Hz, 60 seconds, 4 ranks = ~120 MB (30 MB/rank)

With zlib compression: ~25-40 MB (~70% reduction)

### Can I profile applications with threads?
Yes, PerFlow works with threaded MPI applications. Each thread's call stacks are captured independently.

### Does PerFlow support Fortran applications?
Yes! PerFlow has comprehensive Fortran MPI support with multiple name mangling conventions (gfortran, ifort, etc.). See [Fortran MPI Support](../developer-guide/FORTRAN_MPI_SUPPORT.md) for details.

## Analysis & Results

### Why are my function names showing as `<unknown>`?
This means symbol resolution couldn't find the function names. Solutions:
1. **Compile with debug symbols**: Add `-g` flag when compiling
2. **Use comprehensive symbol resolution**: Set `Strategy::kAddr2LineOnly`
3. **Check shared libraries**: Ensure they're not stripped

### What's the difference between inclusive and exclusive time?
- **Inclusive time**: Total time spent in a function including all its callees
- **Exclusive time**: Time spent only in that function, not its callees

Use inclusive time to find major hotspots, exclusive time to find where actual work is done.

### How do I interpret imbalance factor?
Imbalance factor measures workload distribution:
- **0-10%**: Well balanced
- **10-30%**: Moderate imbalance, may impact scaling
- **>30%**: Significant imbalance, likely performance bottleneck

### Can I analyze multiple runs together?
Yes! The analysis module can load and compare sample files from different runs. You can aggregate data or compare performance across runs.

### How do I generate visualizations?
```bash
# Using online_analysis_example
./examples/online_analysis_example /tmp/samples /tmp/output

# Programmatically
analysis.export_visualization("/tmp/tree.pdf");
```

Requires GraphViz to be installed.

## Troubleshooting

### PerFlow isn't generating sample files. What's wrong?
Common causes:
1. **Output directory doesn't exist**: Create it with `mkdir -p /tmp/samples`
2. **Wrong LD_PRELOAD path**: Use absolute path or verify relative path
3. **Application crashes**: Check if app runs without PerFlow
4. **MPI_Init not called**: PerFlow starts after MPI_Init

See [Troubleshooting Guide](../user-guide/TROUBLESHOOTING.md) for more.

### I'm getting "Permission denied" errors with PMU sampler
Either:
1. **Switch to timer sampler** (recommended, no permissions needed)
2. **Configure PMU permissions**: `sudo sh -c 'echo -1 > /proc/sys/kernel/perf_event_paranoid'`

### My application crashes with SIGSEGV when using PerFlow
Try:
1. **Reduce stack depth**: `PERFLOW_MAX_STACK_DEPTH=32`
2. **Test without PerFlow**: Verify app works normally
3. **Check for conflicts**: Ensure no other LD_PRELOAD libraries
4. **Enable sanitizers**: Build with `PERFLOW_ENABLE_SANITIZERS=ON`

### The overhead is higher than expected
Solutions:
1. **Reduce sampling frequency**: Try 500 Hz instead of 1000 Hz
2. **Switch to PMU sampler**: Much lower overhead than timer
3. **Reduce stack depth**: `PERFLOW_MAX_STACK_DEPTH=64`
4. **Use faster storage**: Local SSD instead of network storage

### Analysis is very slow
Optimizations:
1. **Enable symbol caching**: Should be enabled by default
2. **Use context-free mode**: `TreeBuildMode::kContextFree`
3. **Limit symbol resolution**: Only resolve top hotspots

## Advanced Topics

### Can I use PerFlow programmatically?
Yes! PerFlow provides C++ APIs for:
- Building performance trees from sample data
- Performing balance and hotspot analysis
- Custom analysis and queries
- Visualization generation

See [Online Analysis API](../api-reference/ONLINE_ANALYSIS_API.md) for details.

### What are context-free vs. context-aware trees?
- **Context-free** (default): Merges all instances of same function regardless of calling context
- **Context-aware**: Distinguishes functions by their full calling context (call site)

Context-free is more compact and better for high-level overview. Context-aware provides detailed call graph information.

### Can I extend PerFlow with custom samplers?
Yes! The sampling framework is designed for extensibility. Implement your own sampler by:
1. Setting up a sample trigger mechanism
2. Calling `CallStack::capture()` to capture stacks
3. Storing in `StaticHashMap`
4. Exporting via `DataExporter`

See [Architecture Overview](../developer-guide/ARCHITECTURE.md) for details.

### How does symbol resolution work?
PerFlow uses a multi-stage approach:
1. **Address → Library + Offset**: LibraryMap resolves addresses to library-relative offsets
2. **Offset → Symbol**: SymbolResolver uses dladdr (fast) or addr2line (comprehensive)
3. **Caching**: Results cached for performance (>90% hit rate)

See [Symbol Resolution Guide](../api-reference/SYMBOL_RESOLUTION.md) for details.

### Can I profile GPU code?
Not currently. PerFlow focuses on CPU profiling for MPI applications. GPU profiling support is a potential future enhancement.

### Does PerFlow work with hybrid MPI+OpenMP codes?
Yes, PerFlow works with hybrid parallel codes. Both MPI processes and OpenMP threads are captured in call stacks.

## Performance & Scalability

### What's the maximum number of MPI ranks supported?
There's no hard limit. PerFlow has been tested with hundreds of ranks. Each rank generates its own sample file independently.

### How does PerFlow scale?
**Sampling overhead is per-rank**, so overhead doesn't increase with scale. Analysis is post-process and can be parallelized or distributed.

### Can I sample at different frequencies for different ranks?
No, currently all ranks use the same frequency. This is a potential future enhancement.

### What's the memory overhead per rank?
Typical usage: 10-50 MB per rank

Breakdown:
- Base overhead: ~1-2 MB
- Per unique call stack: ~1 KB
- Grows with application diversity (number of unique execution paths)

## Comparison with Other Tools

### How does PerFlow compare to gprof?
- **PerFlow**: Sampling-based, ultra-low overhead, MPI-aware, works on production code
- **gprof**: Instrumentation-based, higher overhead, not MPI-aware, requires recompilation

### How does PerFlow compare to Valgrind/Callgrind?
- **PerFlow**: <1% overhead, production-safe, real-time analysis
- **Valgrind**: 10-50x slowdown, not suitable for production, detailed but slow

### How does PerFlow compare to Intel VTune?
- **PerFlow**: Open-source, MPI-focused, online analysis, platform-independent option
- **VTune**: Commercial, comprehensive but complex, excellent Intel hardware support

### How does PerFlow compare to TAU/HPCToolkit?
- **PerFlow**: Lighter weight, simpler setup, faster analysis, focused on online analysis
- **TAU/HPCToolkit**: More comprehensive, established ecosystem, broader tool support

## Contributing & Support

### How can I contribute to PerFlow?
See [Contributing Guidelines](../developer-guide/contributing.md) for:
- Reporting bugs and requesting features
- Submitting pull requests
- Code style and testing requirements

### Where can I get help?
- **Documentation**: Browse [docs/](../README.md)
- **Examples**: Check [examples/](../../examples/README.md)
- **Issues**: Search or create [GitHub Issues](https://github.com/yuyangJin/PerFlow/issues)
- **Troubleshooting**: See [Troubleshooting Guide](../user-guide/TROUBLESHOOTING.md)

### How do I report a bug?
Open a [GitHub Issue](https://github.com/yuyangJin/PerFlow/issues) with:
- PerFlow version
- Operating system and version
- MPI implementation and version
- Steps to reproduce
- Error messages and logs
- Output of build configuration

### Is there a mailing list or chat?
Currently, we use GitHub Issues for all support and discussions. This may expand in the future.

## Licensing & Citation

### What license is PerFlow under?
Apache License 2.0 - permissive open-source license allowing commercial use.

### How do I cite PerFlow in my research?
See the [README](../../README.md) for BibTeX citation format.

### Can I use PerFlow in commercial projects?
Yes! The Apache 2.0 license allows commercial use.

## Future Development

### What features are planned?
Potential areas for enhancement:
- GPU profiling support
- Network communication analysis
- Memory allocation tracking
- Real-time monitoring dashboard
- Cloud-native integration

### How can I request a feature?
Open a [GitHub Issue](https://github.com/yuyangJin/PerFlow/issues) with the `enhancement` label describing your use case and proposed feature.

### Is PerFlow actively maintained?
Yes! PerFlow is actively maintained with regular updates, bug fixes, and improvements.

---

**Didn't find your question?** Check the [Troubleshooting Guide](../user-guide/TROUBLESHOOTING.md) or [open an issue](https://github.com/yuyangJin/PerFlow/issues).
