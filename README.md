# PerFlow 2.0

A programmable and fast performance analysis tool for parallel programs with online analysis capabilities.

## Features

### Sampling-Based Profiling
- Low-overhead MPI application profiling using PAPI
- Configurable sampling frequencies (100Hz - 10kHz)
- Call stack capture with libunwind
- Per-process sample data collection

### Symbol Resolution (New!)
- **Function Name Resolution**: Resolve memory addresses to function names
- **Source Location Mapping**: Get source file paths and line numbers
- **Multiple Strategies**: Fast dladdr or comprehensive addr2line with auto-fallback
- **Symbol Caching**: High-performance caching for repeated lookups
- **Backward Compatible**: Optional feature, existing code works unchanged

### Online Analysis Module
- **Performance Tree Generation**: Aggregate call stack samples into hierarchical trees
- **Balance Analysis**: Identify workload distribution and load imbalance across processes
- **Hotspot Analysis**: Detect performance bottlenecks with inclusive and exclusive time analysis
- **Directory Monitoring**: Real-time monitoring of sample data files
- **Tree Visualization**: Generate PDF visualizations with customizable color schemes
- **Data Export/Import**: Efficient tree serialization for post-processing

## Documentation

- [Symbol Resolution Guide](docs/SYMBOL_RESOLUTION.md) - Complete symbol resolution reference
- [Online Analysis API Documentation](docs/ONLINE_ANALYSIS_API.md) - Complete API reference
- [Call Stack Offset Conversion](docs/CALL_STACK_OFFSET_CONVERSION.md) - Address resolution details
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
#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"

// Example 1: Online analysis with symbol resolution
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

// Example 2: Post-analysis with symbol resolution
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback, true);
OffsetConverter converter(resolver);

// Load library map and convert with symbols
LibraryMap lib_map;
lib_map.parse_current_process();
converter.add_map_snapshot(0, lib_map);

CallStack<> stack = /* captured stack */;
auto resolved = converter.convert(stack, 0, true);

// Print with function names and source locations
for (const auto& frame : resolved) {
    std::cout << frame.library_name << " + 0x" 
              << std::hex << frame.offset << std::dec;
    if (!frame.function_name.empty()) {
        std::cout << " [" << frame.function_name << "]";
    }
    if (!frame.filename.empty()) {
        std::cout << " at " << frame.filename << ":" << frame.line_number;
    }
    std::cout << "\n";
}
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