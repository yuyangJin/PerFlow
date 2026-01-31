# Online Analysis Module API Documentation

## Overview

The PerFlow Online Analysis Module provides comprehensive tools for analyzing performance profiling data from MPI applications. It supports building performance trees from sample data, performing various analyses, and visualizing results.

## Core Components

### 1. Performance Tree (`performance_tree.h`)

#### TreeNode
Represents a vertex in the performance tree, corresponding to a function or program structure.

**Key Features:**
- Stores resolved frame information (function name, library, source location)
- Tracks per-process sampling counts and execution times
- Maintains parent-child relationships
- Records call counts between nodes (edge weights)

**Example Usage:**
```cpp
#include "analysis/performance_tree.h"

using namespace perflow::analysis;

// Create a tree node
perflow::sampling::ResolvedFrame frame;
frame.function_name = "compute_kernel";
frame.library_name = "myapp";

TreeNode node(frame);
node.set_process_count(4);  // 4 MPI processes
node.add_sample(0, 100, 10000.0);  // Process 0: 100 samples, 10ms
```

#### PerformanceTree
Thread-safe tree structure for aggregating call stack samples.

**Tree Build Modes:**
- **Context-Free (default)**: Nodes with the same function name and library are merged, regardless of their calling context. This produces a more compact tree showing aggregate behavior.
- **Context-Aware**: Nodes are distinguished by their full calling context, including the specific call site (offset). Same functions called from different locations create separate tree nodes.

**Key Methods:**
```cpp
// Create tree with context-free mode (default)
PerformanceTree tree_free;

// Create tree with context-aware mode
PerformanceTree tree_aware(TreeBuildMode::kContextAware);

// Or set mode after construction
tree.set_build_mode(TreeBuildMode::kContextAware);

tree.set_process_count(4);

// Insert a call stack (frames from bottom to top: main -> compute -> kernel)
std::vector<ResolvedFrame> frames = {main_frame, compute_frame, kernel_frame};
tree.insert_call_stack(frames, process_id, sample_count, execution_time_us);

// Get total samples
uint64_t total = tree.total_samples();
```

**Context-Free vs Context-Aware Example:**
```cpp
// Two call stacks: main->func@0x2000 and main->func@0x3000
// (same function "func", different call sites)

// Context-Free: Creates 1 node for "func" (merged)
//   [root] -> main -> func (total: 15 samples)

// Context-Aware: Creates 2 separate nodes (not merged)
//   [root] -> main -> func@0x2000 (10 samples)
//                  -> func@0x3000 (5 samples)
```

// Access root node
auto root = tree.root();
```

### 2. Tree Builder (`tree_builder.h`)

Constructs performance trees from sample data files.

**Example:**
```cpp
#include "analysis/tree_builder.h"

// Create builder with context-free mode (default)
TreeBuilder builder;

// Or create with context-aware mode
TreeBuilder builder_aware(TreeBuildMode::kContextAware);

// Can also set mode after construction
builder.set_build_mode(TreeBuildMode::kContextAware);

// Load library maps for address resolution
std::vector<std::pair<std::string, uint32_t>> libmaps = {
    {"/path/to/rank0.libmap", 0},
    {"/path/to/rank1.libmap", 1}
};
builder.load_library_maps(libmaps);

// Build tree from sample files
std::vector<std::pair<std::string, uint32_t>> samples = {
    {"/path/to/rank0.pflw", 0},
    {"/path/to/rank1.pflw", 1}
};
size_t loaded = builder.build_from_files(samples, 1000.0);  // 1ms per sample

// Access the tree
const PerformanceTree& tree = builder.tree();
```

**When to Use Context-Aware Mode:**
- Analyzing recursive functions with different call paths
- Identifying performance variations based on calling context
- Debugging performance issues where the same function behaves differently depending on how it's called
- Building more detailed calling-context trees for advanced analysis

// Build tree from sample files
std::vector<std::pair<std::string, uint32_t>> samples = {
    {"/path/to/rank0.pflw", 0},
    {"/path/to/rank1.pflw", 1}
};
size_t loaded = builder.build_from_files(samples, 1000.0);  // 1ms per sample

// Access the tree
const PerformanceTree& tree = builder.tree();
```

### 3. Analyzers (`analyzers.h`)

#### Balance Analyzer
Analyzes workload distribution across processes.

**Example:**
```cpp
#include "analysis/analyzers.h"

BalanceAnalysisResult result = BalanceAnalyzer::analyze(tree);

std::cout << "Mean samples: " << result.mean_samples << "\n";
std::cout << "Std deviation: " << result.std_dev_samples << "\n";
std::cout << "Imbalance factor: " << result.imbalance_factor << "\n";
std::cout << "Most loaded: Process " << result.most_loaded_process << "\n";
std::cout << "Least loaded: Process " << result.least_loaded_process << "\n";
```

**Output Fields:**
- `mean_samples`: Average samples per process
- `std_dev_samples`: Standard deviation
- `min_samples`, `max_samples`: Range of sample counts
- `imbalance_factor`: (max - min) / mean
- `most_loaded_process`, `least_loaded_process`: Process IDs
- `process_samples`: Per-process sample counts

#### Hotspot Analyzer
Identifies performance bottlenecks.

**Example:**
```cpp
// Find top 10 hotspots by total time (inclusive)
auto hotspots = HotspotAnalyzer::find_hotspots(tree, 10);

for (const auto& hotspot : hotspots) {
    std::cout << hotspot.function_name << " in " << hotspot.library_name << "\n";
    std::cout << "  Total: " << hotspot.total_samples << " samples ("
              << hotspot.percentage << "%)\n";
    std::cout << "  Self: " << hotspot.self_samples << " samples ("
              << hotspot.self_percentage << "%)\n";
}

// Find top 10 hotspots by self time (exclusive)
auto self_hotspots = HotspotAnalyzer::find_self_hotspots(tree, 10);
```

### 4. Tree Serialization (`tree_serializer.h`)

Export and import performance trees.

**Export Example:**
```cpp
#include "analysis/tree_serializer.h"

// Export as binary
TreeSerializer::export_tree(tree, "/output/dir", "mytree", false);
// Creates: /output/dir/mytree.ptree

// Export as compressed
TreeSerializer::export_tree(tree, "/output/dir", "mytree", true);
// Creates: /output/dir/mytree.ptree.gz

// Export as human-readable text
TreeSerializer::export_tree_text(tree, "/output/dir", "mytree");
// Creates: /output/dir/mytree.ptree.txt
```

### 5. Tree Visualization (`tree_visualizer.h`)

Generate visual representations of performance trees.

**Example:**
```cpp
#include "analysis/tree_visualizer.h"

// Generate DOT file
TreeVisualizer::generate_dot(tree, "/tmp/tree.dot", 
                             ColorScheme::kHeatmap, 10);

// Generate PDF (requires GraphViz)
TreeVisualizer::generate_pdf(tree, "/tmp/tree.pdf",
                             ColorScheme::kHeatmap, 10);
```

**Color Schemes:**
- `ColorScheme::kGrayscale`: Grayscale based on sample count
- `ColorScheme::kHeatmap`: Blue → Green → Yellow → Red (cold to hot)
- `ColorScheme::kRainbow`: Rainbow colors based on sample percentage

### 6. Directory Monitor (`directory_monitor.h`)

Monitor directories for new or updated performance data files.

**Example:**
```cpp
#include "analysis/directory_monitor.h"

DirectoryMonitor monitor("/path/to/samples", 1000);  // Poll every 1000ms

monitor.set_callback([](const std::string& path, FileType type, bool is_new) {
    if (type == FileType::kSampleData) {
        std::cout << (is_new ? "New" : "Updated") << " sample file: " 
                  << path << "\n";
        // Process the file...
    }
});

monitor.start();
// ... do other work ...
monitor.stop();
```

**File Types:**
- `FileType::kSampleData`: `.pflw` sample files
- `FileType::kLibraryMap`: `.libmap` library mapping files
- `FileType::kPerformanceTree`: `.ptree` tree files
- `FileType::kText`: `.txt` text files

### 7. Online Analysis (`online_analysis.h`)

High-level API combining all analysis features.

**Complete Example:**
```cpp
#include "analysis/online_analysis.h"

OnlineAnalysis analysis;

// Optional: Monitor directory for new files
analysis.set_monitor_directory("/path/to/samples", 1000);
analysis.start_monitoring();

// Build tree from existing files (or let monitor handle it)
// ... add sample files ...

// Perform analyses
auto balance = analysis.analyze_balance();
auto hotspots = analysis.find_hotspots(10);
auto self_hotspots = analysis.find_self_hotspots(10);

// Export results
analysis.export_tree("/output/dir", "results", false);
analysis.export_tree_text("/output/dir", "results");
analysis.export_visualization("/output/dir/tree.pdf");

analysis.stop_monitoring();
```

## Thread Safety

All core components are thread-safe:
- `PerformanceTree`: Uses mutex for concurrent insertions
- `DirectoryMonitor`: Thread-safe file monitoring
- Analyzers: Stateless, safe for concurrent use

## Best Practices

1. **Process Count**: Always set the process count before inserting call stacks
2. **Library Maps**: Load library maps before building trees for better symbol resolution
3. **Memory Usage**: For large datasets, consider processing in batches
4. **Visualization**: Limit tree depth for large trees to improve readability
5. **Monitoring**: Use appropriate poll intervals to balance responsiveness and overhead

## Example: Complete Workflow

```cpp
#include "analysis/online_analysis.h"

int main() {
    // Create online analysis instance
    OnlineAnalysis analysis;
    
    // Load library maps
    analysis.builder().load_library_maps({
        {"/data/rank0.libmap", 0},
        {"/data/rank1.libmap", 1},
        {"/data/rank2.libmap", 2},
        {"/data/rank3.libmap", 3}
    });
    
    // Build tree from sample files
    analysis.builder().build_from_files({
        {"/data/rank0.pflw", 0},
        {"/data/rank1.pflw", 1},
        {"/data/rank2.pflw", 2},
        {"/data/rank3.pflw", 3}
    }, 1000.0);
    
    // Analyze balance
    auto balance = analysis.analyze_balance();
    std::cout << "Load imbalance: " << balance.imbalance_factor << "\n";
    
    // Find hotspots
    auto hotspots = analysis.find_hotspots(10);
    std::cout << "\nTop 10 Hotspots:\n";
    for (size_t i = 0; i < hotspots.size(); ++i) {
        std::cout << i+1 << ". " << hotspots[i].function_name 
                  << " (" << hotspots[i].percentage << "%)\n";
    }
    
    // Export results
    analysis.export_tree("/output", "analysis", false);
    analysis.export_visualization("/output/tree.pdf", 
                                   ColorScheme::kHeatmap, 15);
    
    return 0;
}
```

## Integration with MPI Sampler

The online analysis module is designed to work with PerFlow's MPI sampler:

1. **Run MPI Application with Sampler:**
   ```bash
   LD_PRELOAD=libperflow_mpi_sampler.so \
   PERFLOW_OUTPUT_DIR=/data/samples \
   mpirun -n 4 ./myapp
   ```

2. **Analyze Results:**
   ```bash
   ./online_analysis_example /data/samples /output
   ```

3. **View Results:**
   - Text tree: `/output/performance_tree.ptree.txt`
   - Binary tree: `/output/performance_tree.ptree`
   - Visualization: `/output/performance_tree.pdf`

## Limitations and Future Work

### Current Limitations:
- **TreeBuilder File I/O**: Lambda capture issue in `data.for_each()` causes segfaults when building trees from files. The issue is related to capturing member variables through references. Workaround: Manual tree construction works fine. See TODO in `tests/test_tree_builder.cpp`.
- No Python bindings yet (C++ only)
- No web-based real-time visualization
- Plugin architecture not yet implemented
- Directory monitoring callback `handle_file()` is a placeholder (automatic file processing not implemented)

### Planned Features:
- Python bindings using pybind11
- Web-based real-time dashboard
- Plugin system for custom analysis
- Result caching for repeated queries
- Parallel file processing with thread pools

## See Also

- [TESTING.md](../TESTING.md) - Testing guide for the sampler
- [README.md](../README.md) - Main project documentation
- Example programs in `examples/` directory
