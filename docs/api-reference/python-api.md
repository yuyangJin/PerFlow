# PerFlow Python API Reference

This document provides comprehensive documentation for the PerFlow Python bindings,
which expose the C++ performance analysis library to Python.

## Overview

PerFlow provides two levels of Python APIs:

1. **Low-level API** (this document): Direct bindings to C++ classes like `TreeBuilder`, `HotspotAnalyzer`, etc.
2. **Dataflow API** (see [Dataflow API Reference](dataflow-api.md)): High-level dataflow-based programming framework for composing analysis workflows.

For most use cases, we recommend using the **Dataflow API** as it provides:
- Fluent, user-friendly workflow composition
- Automatic parallel execution of independent tasks
- Result caching and lazy evaluation
- Pre-built analysis nodes

## Installation

### Building from Source

```bash
# Clone the repository
git clone https://github.com/yuyangJin/PerFlow.git
cd PerFlow

# Initialize submodules (required for pybind11)
git submodule add https://github.com/pybind/pybind11.git third_party/pybind11

# Build with Python bindings
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_PYTHON=ON
make -j4

# The module will be built in python/perflow/
```

### Using pip (Development Mode)

```bash
cd PerFlow/python
pip install -e .
```

## Quick Start

### Using Dataflow API (Recommended)

```python
from perflow.dataflow import WorkflowBuilder

# Create and execute a workflow in one fluent chain
results = (
    WorkflowBuilder("MyAnalysis")
    .load_data([('rank_0.pflw', 0), ('rank_1.pflw', 1)])
    .find_hotspots(top_n=10)
    .analyze_balance()
    .execute()
)

# Access results
for node_id, output in results.items():
    if 'hotspots' in output:
        for h in output['hotspots']:
            print(f"{h.function_name}: {h.self_percentage:.1f}%")
```

### Using Low-level API

```python
import perflow

# Build a performance tree from sample files
builder = perflow.TreeBuilder()
builder.load_library_maps([('rank_0.libmap', 0), ('rank_1.libmap', 1)])
builder.build_from_files([('rank_0.pflw', 0), ('rank_1.pflw', 1)])

# Get the tree
tree = builder.tree

# Find performance hotspots
hotspots = perflow.HotspotAnalyzer.find_hotspots(tree, top_n=10)
for h in hotspots:
    print(f"{h.function_name}: {h.self_percentage:.1f}%")

# Analyze workload balance
balance = perflow.BalanceAnalyzer.analyze(tree)
print(f"Imbalance factor: {balance.imbalance_factor:.2f}")
```

## Low-level API Reference

### Enumerations

#### `TreeBuildMode`
Defines how call stacks are aggregated into a tree.

| Value | Description |
|-------|-------------|
| `ContextFree` | Nodes with same function are merged regardless of calling context |
| `ContextAware` | Nodes are distinguished by their full calling context |

#### `SampleCountMode`
Defines how samples are counted during tree building.

| Value | Description |
|-------|-------------|
| `Exclusive` | Only track self samples (samples at leaf nodes) |
| `Inclusive` | Only track total samples (all samples including children) |
| `Both` | Track both inclusive and exclusive samples |

### Classes

#### `ResolvedFrame`
Represents a resolved stack frame with symbol information.

**Attributes:**
- `raw_address: int` - Original raw address
- `offset: int` - Offset within the library/binary
- `library_name: str` - Library or binary name
- `function_name: str` - Function name (if available)
- `filename: str` - Source file name (if available)
- `line_number: int` - Line number (if available)

#### `TreeNode`
Represents a vertex in the performance tree.

**Properties:**
- `frame: ResolvedFrame` - The resolved frame information
- `function_name: str` - The function name
- `library_name: str` - The library name
- `total_samples: int` - Total samples across all processes
- `self_samples: int` - Self samples (samples at this leaf)
- `sampling_counts: List[int]` - Per-process sampling counts
- `execution_times: List[float]` - Per-process execution times (microseconds)
- `parent: TreeNode` - Parent node (None if root)
- `children: List[TreeNode]` - List of child nodes
- `child_count: int` - Number of children
- `is_leaf: bool` - True if this is a leaf node
- `is_root: bool` - True if this is the root node
- `depth: int` - Depth in the tree (root = 0)
- `total_execution_time: float` - Total execution time across all processes

**Methods:**
- `sampling_count(process_id: int) -> int` - Get sampling count for a specific process
- `execution_time(process_id: int) -> float` - Get execution time for a specific process
- `siblings() -> List[TreeNode]` - Get all sibling nodes
- `get_path() -> List[str]` - Get path from root to this node as function names
- `find_child_by_name(func_name: str) -> TreeNode` - Find child by function name
- `get_call_count(child: TreeNode) -> int` - Get call count to a specific child

#### `PerformanceTree`
Aggregates call stack samples into a tree structure.

**Constructor:**
```python
tree = PerformanceTree(
    mode: TreeBuildMode = TreeBuildMode.ContextFree,
    count_mode: SampleCountMode = SampleCountMode.Exclusive
)
```

**Properties:**
- `root: TreeNode` - The root node
- `process_count: int` - Number of processes
- `total_samples: int` - Total number of samples
- `build_mode: TreeBuildMode` - Tree build mode
- `sample_count_mode: SampleCountMode` - Sample count mode
- `node_count: int` - Total number of nodes
- `max_depth: int` - Maximum depth of the tree
- `all_nodes: List[TreeNode]` - All nodes in the tree
- `leaf_nodes: List[TreeNode]` - All leaf nodes

**Methods:**
- `clear()` - Clear all data from the tree
- `insert_call_stack(frames: List[ResolvedFrame], process_id: int, count: int = 1, time_us: float = 0.0)` - Insert a call stack
- `nodes_at_depth(depth: int) -> List[TreeNode]` - Get all nodes at a specific depth
- `find_nodes_by_name(func_name: str) -> List[TreeNode]` - Find nodes by function name
- `find_nodes_by_library(lib_name: str) -> List[TreeNode]` - Find nodes by library name
- `filter_by_samples(min_samples: int) -> List[TreeNode]` - Filter nodes by sample count
- `filter_by_self_samples(min_self_samples: int) -> List[TreeNode]` - Filter by self samples
- `traverse_preorder(visitor: Callable[[TreeNode], bool])` - Pre-order traversal
- `traverse_postorder(visitor: Callable[[TreeNode], bool])` - Post-order traversal
- `traverse_levelorder(visitor: Callable[[TreeNode], bool])` - Level-order traversal

#### `TreeBuilder`
Constructs performance trees from sample data files.

**Constructor:**
```python
builder = TreeBuilder(
    mode: TreeBuildMode = TreeBuildMode.ContextFree,
    count_mode: SampleCountMode = SampleCountMode.Exclusive
)
```

**Properties:**
- `tree: PerformanceTree` - The performance tree
- `build_mode: TreeBuildMode` - The build mode
- `sample_count_mode: SampleCountMode` - The sample count mode

**Methods:**
- `set_build_mode(mode: TreeBuildMode)` - Set the tree build mode
- `set_sample_count_mode(mode: SampleCountMode)` - Set the sample count mode
- `build_from_file(sample_file: str, process_id: int, time_per_sample: float = 1000.0) -> bool` - Build from a single file
- `build_from_files(sample_files: List[Tuple[str, int]], time_per_sample: float = 1000.0) -> int` - Build from multiple files
- `load_library_maps(libmap_files: List[Tuple[str, int]]) -> int` - Load library maps
- `clear()` - Clear all data

#### `BalanceAnalysisResult`
Contains workload balance information.

**Attributes:**
- `mean_samples: float` - Mean samples per process
- `std_dev_samples: float` - Standard deviation
- `min_samples: float` - Minimum samples
- `max_samples: float` - Maximum samples
- `imbalance_factor: float` - (max - min) / mean
- `most_loaded_process: int` - Process with most samples
- `least_loaded_process: int` - Process with least samples
- `process_samples: List[float]` - Per-process sample counts

#### `HotspotInfo`
Describes a performance hotspot.

**Attributes:**
- `function_name: str` - Function name
- `library_name: str` - Library name
- `source_location: str` - Source file location
- `total_samples: int` - Total/inclusive samples
- `percentage: float` - Percentage of total samples
- `self_samples: int` - Self/exclusive samples
- `self_percentage: float` - Percentage of self samples

#### `BalanceAnalyzer`
Analyzes workload distribution across processes.

**Static Methods:**
- `analyze(tree: PerformanceTree) -> BalanceAnalysisResult` - Analyze workload balance

#### `HotspotAnalyzer`
Identifies performance bottlenecks.

**Static Methods:**
- `find_hotspots(tree: PerformanceTree, top_n: int = 10) -> List[HotspotInfo]` - Find top hotspots by self time
- `find_self_hotspots(tree: PerformanceTree, top_n: int = 10) -> List[HotspotInfo]` - Alias for find_hotspots
- `find_total_hotspots(tree: PerformanceTree, top_n: int = 10) -> List[HotspotInfo]` - Find hotspots by total time

#### `ParallelFileReader`
Provides parallel file reading for sample data.

**Constructor:**
```python
reader = ParallelFileReader(num_threads: int = 0)  # 0 = auto-detect
```

**Properties:**
- `thread_count: int` - Number of threads used

**Methods:**
- `set_progress_callback(callback: Callable[[int, int], None])` - Set progress callback
- `read_files_parallel(sample_files, tree, converter, time_per_sample) -> List[FileReadResult]` - Read files in parallel

### Convenience Functions

#### `perflow.analyze_samples()`
```python
results = perflow.analyze_samples(
    sample_files: List[Tuple[str, int]],  # List of (filepath, process_id)
    libmap_files: List[Tuple[str, int]] = None,  # Optional library maps
    top_n: int = 10,
    mode: TreeBuildMode = TreeBuildMode.ContextFree
) -> dict
```

Returns a dictionary with:
- `tree`: The PerformanceTree object
- `hotspots`: List of HotspotInfo objects
- `balance`: BalanceAnalysisResult object

#### `perflow.print_hotspots()`
```python
perflow.print_hotspots(
    tree: PerformanceTree,
    top_n: int = 10,
    show_inclusive: bool = False
)
```

Prints hotspot analysis results in a formatted table.

#### `perflow.print_balance()`
```python
perflow.print_balance(tree: PerformanceTree)
```

Prints workload balance analysis results.

#### `perflow.traverse_tree()`
```python
perflow.traverse_tree(
    tree: PerformanceTree,
    visitor: Callable[[TreeNode], bool],
    order: str = 'preorder'  # 'preorder', 'postorder', or 'levelorder'
)
```

Traverse the tree with a visitor function.

## Examples

### Basic Usage

```python
import perflow

# Create a tree builder
builder = perflow.TreeBuilder()

# Build tree from sample files
builder.build_from_files([
    ('rank_0.pflw', 0),
    ('rank_1.pflw', 1),
    ('rank_2.pflw', 2),
    ('rank_3.pflw', 3),
])

tree = builder.tree

# Print hotspots
perflow.print_hotspots(tree, top_n=10)

# Print balance
perflow.print_balance(tree)
```

### Custom Tree Traversal

```python
import perflow

# Build tree...
tree = perflow.PerformanceTree()
# ...insert data...

# Find all functions with more than 1000 samples
def find_hot_functions(node):
    if node.total_samples > 1000:
        print(f"{node.function_name}: {node.total_samples} samples")
    return True  # Continue traversal

tree.traverse_preorder(find_hot_functions)
```

### Filtering Nodes

```python
import perflow

tree = builder.tree

# Get only leaf nodes (actual execution points)
leaves = tree.leaf_nodes
print(f"Found {len(leaves)} leaf functions")

# Get nodes with significant self time
hot_nodes = tree.filter_by_self_samples(min_self_samples=100)
for node in hot_nodes:
    print(f"{node.function_name}: {node.self_samples} self samples")

# Find all functions from a specific library
math_functions = tree.find_nodes_by_library("libm.so.6")
```

### Parallel File Loading

```python
import perflow

# For large numbers of files, use parallel loading
reader = perflow.ParallelFileReader(num_threads=8)

# Progress callback
def on_progress(completed, total):
    print(f"Progress: {completed}/{total} files")

reader.set_progress_callback(on_progress)

tree = perflow.PerformanceTree()
converter = perflow.OffsetConverter()

results = reader.read_files_parallel(
    sample_files=[('rank_0.pflw', 0), ('rank_1.pflw', 1)],
    tree=tree,
    converter=converter,
    time_per_sample=1000.0
)

for r in results:
    print(f"{r.filepath}: {'OK' if r.success else 'FAILED'}")
```
