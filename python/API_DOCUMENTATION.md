# PerFlow Python API Documentation

Complete API reference for the PerFlow Python frontend.

## Table of Contents

- [C++ Bindings API](#cpp-bindings-api)
  - [Enumerations](#enumerations)
  - [Core Classes](#core-classes)
  - [Analysis Classes](#analysis-classes)
  - [Traversal Classes](#traversal-classes)
- [Python Frontend API](#python-frontend-api)
  - [Dataflow Module](#dataflow-module)
  - [Analysis Module](#analysis-module)
  - [Workflows Module](#workflows-module)

---

## C++ Bindings API

The C++ bindings provide direct access to the high-performance backend.

### Enumerations

#### TreeBuildMode

Defines how call stacks are aggregated into a tree.

```python
perflow.TreeBuildMode.CONTEXT_FREE   # Merge nodes with same function
perflow.TreeBuildMode.CONTEXT_AWARE  # Distinguish by calling context
```

#### SampleCountMode

Defines how samples are counted during tree building.

```python
perflow.SampleCountMode.EXCLUSIVE  # Only track self samples (leaf nodes)
perflow.SampleCountMode.INCLUSIVE  # Only track total samples (with children)
perflow.SampleCountMode.BOTH       # Track both inclusive and exclusive
```

#### ColorScheme

Defines the coloring strategy for visualization.

```python
perflow.ColorScheme.GRAYSCALE  # Grayscale based on sample count
perflow.ColorScheme.HEATMAP    # Heat map (blue -> red)
perflow.ColorScheme.RAINBOW    # Rainbow colors
```

### Core Classes

#### ResolvedFrame

A single resolved stack frame with symbol information.

**Attributes:**
- `raw_address: int` - Original raw address
- `offset: int` - Offset within the library/binary
- `library_name: str` - Library or binary name
- `function_name: str` - Function name (if available)
- `filename: str` - Source file name (if available)
- `line_number: int` - Line number (if available, 0 otherwise)

**Example:**
```python
frame = node.frame()
print(f"{frame.function_name} in {frame.library_name}")
if frame.filename:
    print(f"  at {frame.filename}:{frame.line_number}")
```

#### TreeNode

Represents a vertex in the performance tree.

**Methods:**
- `frame() -> ResolvedFrame` - Get the resolved frame
- `sampling_counts() -> List[int]` - Get sampling counts for all processes
- `execution_times() -> List[float]` - Get execution times (microseconds)
- `total_samples() -> int` - Get total samples across all processes
- `self_samples() -> int` - Get self samples (samples at this leaf)
- `children() -> List[TreeNode]` - Get children nodes
- `parent() -> Optional[TreeNode]` - Get parent node
- `find_child(frame: ResolvedFrame) -> Optional[TreeNode]` - Find child (context-free)
- `find_child_context_aware(frame: ResolvedFrame) -> Optional[TreeNode]` - Find child (context-aware)
- `get_call_count(child: TreeNode) -> int` - Get call count to a specific child

**Example:**
```python
node = tree.root()
print(f"Total samples: {node.total_samples()}")
print(f"Self samples: {node.self_samples()}")

for child in node.children():
    print(f"  Child: {child.frame().function_name}")
```

#### PerformanceTree

Aggregates call stack samples into a tree structure. Thread-safe for concurrent insertions.

**Constructor:**
```python
tree = perflow.PerformanceTree(
    mode=perflow.TreeBuildMode.CONTEXT_FREE,
    count_mode=perflow.SampleCountMode.EXCLUSIVE
)
```

**Methods:**
- `root() -> TreeNode` - Get the root node
- `process_count() -> int` - Get the number of processes
- `set_process_count(count: int)` - Set the number of processes
- `build_mode() -> TreeBuildMode` - Get the tree build mode
- `set_build_mode(mode: TreeBuildMode)` - Set the tree build mode
- `sample_count_mode() -> SampleCountMode` - Get the sample count mode
- `set_sample_count_mode(mode: SampleCountMode)` - Set the sample count mode
- `insert_call_stack(frames: List[ResolvedFrame], process_id: int, count: int = 1, time_us: float = 0.0)` - Insert a call stack
- `total_samples() -> int` - Get total number of samples in the tree
- `clear()` - Clear the tree

**Example:**
```python
tree = perflow.PerformanceTree(
    mode=perflow.TreeBuildMode.CONTEXT_AWARE,
    count_mode=perflow.SampleCountMode.BOTH
)
tree.set_process_count(4)

# Insert call stacks...
# tree.insert_call_stack(frames, process_id, count, time_us)

print(f"Total samples: {tree.total_samples()}")
```

#### TreeBuilder

Constructs performance trees from sample data.

**Constructor:**
```python
builder = perflow.TreeBuilder(
    mode=perflow.TreeBuildMode.CONTEXT_FREE,
    count_mode=perflow.SampleCountMode.EXCLUSIVE
)
```

**Methods:**
- `tree() -> PerformanceTree` - Get the performance tree
- `build_mode() -> TreeBuildMode` - Get the tree build mode
- `set_build_mode(mode: TreeBuildMode)` - Set the tree build mode
- `sample_count_mode() -> SampleCountMode` - Get the sample count mode
- `set_sample_count_mode(mode: SampleCountMode)` - Set the sample count mode
- `build_from_files(sample_files: List[Tuple[str, int]], time_per_sample: float = 1000.0) -> int` - Build from files (sequential)
- `build_from_files_parallel(sample_files: List[Tuple[str, int]], time_per_sample: float = 1000.0, num_threads: int = 0) -> int` - Build from files (parallel)
- `load_library_maps(libmap_files: List[Tuple[str, int]]) -> int` - Load library maps
- `clear()` - Clear all data

**Example:**
```python
builder = perflow.TreeBuilder()

# Load library maps for symbol resolution
libmap_files = [
    ("/path/to/rank_0.libmap", 0),
    ("/path/to/rank_1.libmap", 1),
]
builder.load_library_maps(libmap_files)

# Build tree from sample files in parallel
sample_files = [
    ("/path/to/rank_0.pflw", 0),
    ("/path/to/rank_1.pflw", 1),
]
success_count = builder.build_from_files_parallel(sample_files, num_threads=4)

tree = builder.tree()
print(f"Loaded {success_count} files, total samples: {tree.total_samples()}")
```

### Analysis Classes

#### BalanceAnalysisResult

Contains workload balance information.

**Attributes:**
- `mean_samples: float` - Mean samples per process
- `std_dev_samples: float` - Standard deviation of samples
- `min_samples: float` - Minimum samples
- `max_samples: float` - Maximum samples
- `imbalance_factor: float` - (max - min) / mean
- `most_loaded_process: int` - Process with most samples
- `least_loaded_process: int` - Process with least samples
- `process_samples: List[float]` - Samples per process

#### HotspotInfo

Describes a performance hotspot.

**Attributes:**
- `function_name: str` - Function name
- `library_name: str` - Library name
- `source_location: str` - Source file and line number
- `total_samples: int` - Total samples (inclusive)
- `percentage: float` - Percentage of total (inclusive)
- `self_samples: int` - Self samples (exclusive)
- `self_percentage: float` - Percentage of total (exclusive)

#### BalanceAnalyzer

Analyzes workload distribution across processes.

**Static Methods:**
- `analyze(tree: PerformanceTree) -> BalanceAnalysisResult` - Analyze workload balance

**Example:**
```python
result = perflow.BalanceAnalyzer.analyze(tree)

print(f"Mean samples: {result.mean_samples:.2f}")
print(f"Imbalance factor: {result.imbalance_factor:.4f}")
print(f"Most loaded: Process {result.most_loaded_process} ({result.max_samples} samples)")
print(f"Least loaded: Process {result.least_loaded_process} ({result.min_samples} samples)")
```

#### HotspotAnalyzer

Identifies performance bottlenecks.

**Static Methods:**
- `find_hotspots(tree: PerformanceTree, top_n: int = 10) -> List[HotspotInfo]` - Find hotspots (exclusive/self time)
- `find_self_hotspots(tree: PerformanceTree, top_n: int = 10) -> List[HotspotInfo]` - Alias for find_hotspots
- `find_total_hotspots(tree: PerformanceTree, top_n: int = 10) -> List[HotspotInfo]` - Find hotspots (inclusive/total time)

**Example:**
```python
# Find top 10 hotspots by self time (exclusive)
hotspots = perflow.HotspotAnalyzer.find_self_hotspots(tree, 10)

for i, h in enumerate(hotspots, 1):
    print(f"{i}. {h.function_name}")
    print(f"   Self: {h.self_percentage:.2f}% ({h.self_samples} samples)")
    print(f"   Total: {h.percentage:.2f}% ({h.total_samples} samples)")
    if h.source_location:
        print(f"   Source: {h.source_location}")
```

### Traversal Classes

#### TreeVisitor

Base class for tree traversal visitors.

**Methods:**
- `visit(node: TreeNode, depth: int) -> bool` - Called when visiting a node. Return True to continue, False to stop.

**Example:**
```python
class MyVisitor(perflow.TreeVisitor):
    def __init__(self):
        super().__init__()
        self.hotspots = []
    
    def visit(self, node, depth):
        if node.self_samples() > 1000:
            self.hotspots.append(node)
        return True  # Continue traversal

visitor = MyVisitor()
perflow.TreeTraversal.depth_first_preorder(tree.root(), visitor)
print(f"Found {len(visitor.hotspots)} hotspots")
```

#### TreeTraversal

Provides various tree traversal algorithms.

**Static Methods:**
- `depth_first_preorder(root: TreeNode, visitor: TreeVisitor, max_depth: int = 0)` - DFS pre-order
- `depth_first_postorder(root: TreeNode, visitor: TreeVisitor, max_depth: int = 0)` - DFS post-order
- `breadth_first(root: TreeNode, visitor: TreeVisitor, max_depth: int = 0)` - BFS level-order
- `find_nodes(root: TreeNode, predicate: Callable[[TreeNode], bool]) -> List[TreeNode]` - Find all matching nodes
- `find_first(root: TreeNode, predicate: Callable[[TreeNode], bool]) -> Optional[TreeNode]` - Find first match
- `get_path_to_node(root: TreeNode, target: TreeNode) -> List[TreeNode]` - Get path from root to node
- `get_leaf_nodes(root: TreeNode) -> List[TreeNode]` - Get all leaf nodes
- `get_nodes_at_depth(root: TreeNode, target_depth: int) -> List[TreeNode]` - Get nodes at depth
- `get_siblings(node: TreeNode) -> List[TreeNode]` - Get sibling nodes
- `get_tree_depth(root: TreeNode) -> int` - Calculate tree depth
- `count_nodes(root: TreeNode) -> int` - Count total nodes

**Predicates:**
- `perflow.samples_above(threshold: int)` - Nodes with samples > threshold
- `perflow.self_samples_above(threshold: int)` - Nodes with self samples > threshold
- `perflow.function_name_equals(name: str)` - Nodes matching function name
- `perflow.function_name_contains(substring: str)` - Nodes containing substring
- `perflow.library_name_equals(name: str)` - Nodes in specific library
- `perflow.is_leaf()` - Leaf nodes

**Example:**
```python
# Find all nodes with more than 100 samples
nodes = perflow.TreeTraversal.find_nodes(
    tree.root(),
    perflow.samples_above(100)
)

# Find all nodes in a specific library
math_nodes = perflow.TreeTraversal.find_nodes(
    tree.root(),
    perflow.library_name_equals("libm.so")
)

# Get all leaf nodes
leaves = perflow.TreeTraversal.get_leaf_nodes(tree.root())

# Get nodes at depth 3
level3 = perflow.TreeTraversal.get_nodes_at_depth(tree.root(), 3)
```

---

## Python Frontend API

The Python frontend provides high-level abstractions for composing analysis workflows.

### Dataflow Module

#### Task (Abstract Base Class)

Base class for analysis tasks.

**Constructor:**
```python
class MyTask(perflow.Task):
    def __init__(self, param1, name=None):
        super().__init__(name or "MyTask")
        self.param1 = param1
```

**Methods:**
- `execute(**inputs) -> Any` - Execute the task (abstract, must implement)
- `get_cache_key(**inputs) -> str` - Generate cache key
- `invalidate_cache()` - Invalidate cached result

**Example:**
```python
class FilterTask(perflow.Task):
    def __init__(self, threshold, name=None):
        super().__init__(name or "FilterTask")
        self.threshold = threshold
    
    def execute(self, tree=None, **inputs):
        nodes = perflow.TreeTraversal.find_nodes(
            tree.root(),
            perflow.samples_above(self.threshold)
        )
        return nodes
```

#### Node

Represents a node in the dataflow graph.

**Methods:**
- `add_input(name: str, edge: Edge)` - Add input edge
- `add_output(edge: Edge)` - Add output edge
- `can_execute(executed_nodes: Set[Node]) -> bool` - Check if ready
- `execute(results: Dict[Node, Any]) -> Any` - Execute the task
- `reset()` - Reset execution state

#### Edge

Represents an edge in the dataflow graph.

**Constructor:**
```python
edge = perflow.Edge(source_node, target_node, name="input_param")
```

#### Graph

Dataflow graph for performance analysis workflows.

**Constructor:**
```python
graph = perflow.Graph(name="MyWorkflow")
```

**Methods:**
- `add_node(task: Task) -> Node` - Add a task node
- `add_edge(source: Node, target: Node, name: str = None) -> Edge` - Add edge
- `topological_sort() -> List[Node]` - Get execution order
- `get_independent_groups(sorted_nodes: List[Node]) -> List[List[Node]]` - Group for parallel execution
- `execute(lazy: bool = False, inputs: Dict[Node, Any] = None) -> Dict[Node, Any]` - Execute workflow
- `get_output_nodes() -> List[Node]` - Get final output nodes
- `visualize() -> str` - Generate DOT representation
- `save_visualization(filepath: str)` - Save graph visualization

**Attributes:**
- `nodes: List[Node]` - All nodes in the graph
- `edges: List[Edge]` - All edges in the graph
- `cache: Dict[str, Any]` - Result cache
- `cache_enabled: bool` - Enable/disable caching
- `parallel_enabled: bool` - Enable/disable parallel execution

**Example:**
```python
graph = perflow.Graph(name="CustomAnalysis")

# Add nodes
load_node = graph.add_node(LoadTreeTask(sample_files))
balance_node = graph.add_node(BalanceAnalysisTask())
hotspot_node = graph.add_node(HotspotAnalysisTask(top_n=10))

# Connect nodes
graph.add_edge(load_node, balance_node, "tree")
graph.add_edge(load_node, hotspot_node, "tree")

# Visualize
graph.save_visualization("workflow.dot")

# Execute
results = graph.execute()
```

### Analysis Module

Pre-built analysis tasks for common operations.

#### LoadTreeTask

Task to load a performance tree from sample files.

**Constructor:**
```python
task = perflow.analysis.LoadTreeTask(
    sample_files=[(path, rank_id), ...],
    libmap_files=[(path, rank_id), ...],  # Optional
    time_per_sample=1000.0,
    build_mode=perflow.TreeBuildMode.CONTEXT_FREE,
    count_mode=perflow.SampleCountMode.EXCLUSIVE,
    parallel=True,
    name="LoadTree"
)
```

#### BalanceAnalysisTask

Task to analyze workload balance.

**Constructor:**
```python
task = perflow.analysis.BalanceAnalysisTask(name="BalanceAnalysis")
```

**Inputs:** `tree` (PerformanceTree)
**Output:** BalanceAnalysisResult

#### HotspotAnalysisTask

Task to find performance hotspots.

**Constructor:**
```python
task = perflow.analysis.HotspotAnalysisTask(
    top_n=10,
    mode="self",  # or "total"
    name="HotspotAnalysis"
)
```

**Inputs:** `tree` (PerformanceTree)
**Output:** List[HotspotInfo]

#### ExportVisualizationTask

Task to export tree visualization.

**Constructor:**
```python
task = perflow.analysis.ExportVisualizationTask(
    output_path="/path/to/output.pdf",
    color_scheme=perflow.ColorScheme.HEATMAP,
    max_depth=0,
    name="ExportVisualization"
)
```

**Inputs:** `tree` (PerformanceTree)
**Output:** str (output path)

#### ReportTask

Task to generate analysis reports.

**Constructor:**
```python
task = perflow.analysis.ReportTask(
    output_path="/path/to/report.txt",
    format="text",  # "text", "json", or "html"
    name="GenerateReport"
)
```

**Inputs:** Any analysis results
**Output:** str (output path)

### Workflows Module

Pre-built workflows for common use cases.

#### create_basic_analysis_workflow

Create a basic analysis workflow.

```python
workflow = perflow.workflows.create_basic_analysis_workflow(
    sample_files=[(path, rank_id), ...],
    output_dir="/path/to/output",
    libmap_files=[(path, rank_id), ...],  # Optional
    top_n=10,
    parallel=True
)

results = workflow.execute()
```

**Generates:**
- Performance tree visualization (PDF)
- Balance analysis report
- Hotspot analysis report
- Combined text report

#### create_comparative_analysis_workflow

Create a comparative analysis workflow for multiple datasets.

```python
workflow = perflow.workflows.create_comparative_analysis_workflow(
    sample_file_sets=[
        [(path1, rank), ...],
        [(path2, rank), ...],
    ],
    labels=["Dataset1", "Dataset2"],
    output_dir="/path/to/output",
    libmap_files=None
)

results = workflow.execute()
```

#### create_hotspot_focused_workflow

Create a hotspot-focused workflow.

```python
workflow = perflow.workflows.create_hotspot_focused_workflow(
    sample_files=[(path, rank_id), ...],
    output_dir="/path/to/output",
    libmap_files=None,
    sample_threshold=100
)

results = workflow.execute()
```

**Generates:**
- Self-time hotspots (exclusive)
- Total-time hotspots (inclusive)
- Detailed HTML report

---

## Complete Examples

### Example 1: Custom Workflow with Multiple Analysis Tasks

```python
import perflow
from perflow.dataflow import Graph
from perflow.analysis import (
    LoadTreeTask, BalanceAnalysisTask, HotspotAnalysisTask,
    ExportVisualizationTask, ReportTask
)

# Sample files
sample_files = [
    ("/data/rank_0.pflw", 0),
    ("/data/rank_1.pflw", 1),
    ("/data/rank_2.pflw", 2),
    ("/data/rank_3.pflw", 3),
]

# Create workflow graph
graph = Graph(name="FullAnalysis")

# Task 1: Load tree
load_task = LoadTreeTask(sample_files, parallel=True)
load_node = graph.add_node(load_task)

# Task 2: Balance analysis
balance_task = BalanceAnalysisTask()
balance_node = graph.add_node(balance_task)
graph.add_edge(load_node, balance_node, "tree")

# Task 3: Self-time hotspots
self_hotspot_task = HotspotAnalysisTask(top_n=10, mode="self")
self_node = graph.add_node(self_hotspot_task)
graph.add_edge(load_node, self_node, "tree")

# Task 4: Total-time hotspots
total_hotspot_task = HotspotAnalysisTask(top_n=10, mode="total")
total_node = graph.add_node(total_hotspot_task)
graph.add_edge(load_node, total_node, "tree")

# Task 5: Export visualization
viz_task = ExportVisualizationTask("/output/tree.pdf")
viz_node = graph.add_node(viz_task)
graph.add_edge(load_node, viz_node, "tree")

# Execute
results = graph.execute()

# Access results
balance = results[balance_node]
self_hotspots = results[self_node]
total_hotspots = results[total_node]

print(f"Imbalance factor: {balance.imbalance_factor:.4f}")
print(f"\nTop self-time hotspots:")
for i, h in enumerate(self_hotspots[:5], 1):
    print(f"  {i}. {h.function_name}: {h.self_percentage:.2f}%")
```

### Example 2: Using TreeTraversal for Custom Analysis

```python
import perflow

# Load tree
builder = perflow.TreeBuilder()
builder.build_from_files_parallel(sample_files)
tree = builder.tree()

# Custom visitor to collect statistics
class StatsVisitor(perflow.TreeVisitor):
    def __init__(self):
        super().__init__()
        self.total_nodes = 0
        self.leaf_nodes = 0
        self.hotspot_nodes = 0
        self.depth_histogram = {}
    
    def visit(self, node, depth):
        self.total_nodes += 1
        
        if not node.children():
            self.leaf_nodes += 1
        
        if node.self_samples() > 100:
            self.hotspot_nodes += 1
        
        self.depth_histogram[depth] = self.depth_histogram.get(depth, 0) + 1
        
        return True  # Continue

# Traverse tree
visitor = StatsVisitor()
perflow.TreeTraversal.depth_first_preorder(tree.root(), visitor)

print(f"Total nodes: {visitor.total_nodes}")
print(f"Leaf nodes: {visitor.leaf_nodes}")
print(f"Hotspot nodes: {visitor.hotspot_nodes}")
print(f"Depth histogram: {visitor.depth_histogram}")
```

---

For more examples, see the `python/examples/` directory in the repository.
