# PerFlow 2.0

A programmable and fast performance analysis tool for parallel programs with online analysis capabilities.

## Features

### Sampling-Based Profiling
- Low-overhead MPI application profiling using PAPI
- Configurable sampling frequencies (100Hz - 10kHz)
- Call stack capture with libunwind
- Per-process sample data collection

### Online Analysis Module (New!)
- **Performance Tree Generation**: Aggregate call stack samples into hierarchical trees
- **Balance Analysis**: Identify workload distribution and load imbalance across processes
- **Hotspot Analysis**: Detect performance bottlenecks with inclusive and exclusive time analysis
- **Directory Monitoring**: Real-time monitoring of sample data files
- **Tree Visualization**: Generate PDF visualizations with customizable color schemes
- **Data Export/Import**: Efficient tree serialization for post-processing

## Documentation

- [Online Analysis API Documentation](docs/ONLINE_ANALYSIS_API.md) - Complete API reference
- [Testing Guide](TESTING.md) - Comprehensive testing documentation

## Quick Start

### Building

```bash
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)
```

### Running MPI Sampler

```bash
LD_PRELOAD=lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
mpirun -n 4 ./your_mpi_app
```

### Analyzing Results

```bash
./examples/online_analysis_example /tmp/samples /tmp/output
```

## Online Analysis Example

```cpp
#include "analysis/online_analysis.h"

OnlineAnalysis analysis;

// Build tree from sample files
analysis.builder().build_from_files({
    {"/data/rank0.pflw", 0},
    {"/data/rank1.pflw", 1}
}, 1000.0);

// Analyze workload balance
auto balance = analysis.analyze_balance();
std::cout << "Imbalance: " << balance.imbalance_factor << "\n";

// Find hotspots
auto hotspots = analysis.find_hotspots(10);
for (const auto& h : hotspots) {
    std::cout << h.function_name << ": " << h.percentage << "%\n";
}

// Export visualization
analysis.export_visualization("/tmp/tree.pdf");
```

## Requirements

- C++17 compiler
- CMake 3.14+
- MPI (OpenMPI, MPICH, Intel MPI)
- PAPI library
- libunwind
- zlib (optional, for compression)
- GraphViz (optional, for visualization)

## License

Apache License 2.0