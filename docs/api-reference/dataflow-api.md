# PerFlow Dataflow-based Python Analysis Framework

This document provides comprehensive documentation for the PerFlow dataflow-based Python API, which allows users to compose performance analysis workflows as directed acyclic graphs (DAGs).

## Overview

The dataflow framework provides:
- **Graph abstraction**: Nodes represent analysis subtasks, edges represent data transfers
- **Optimization mechanisms**: Graph fusion, parallel execution, lazy evaluation, caching
- **User-friendly interface**: Fluent API and pre-built analysis nodes

## Quick Start

### Basic Workflow

```python
from perflow.dataflow import WorkflowBuilder

# Create and execute a workflow
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

### Manual Graph Construction

```python
from perflow.dataflow import (
    DataflowGraph,
    LoadDataNode,
    HotspotAnalysisNode,
    BalanceAnalysisNode,
)

# Create nodes
load_node = LoadDataNode(
    sample_files=[('rank_0.pflw', 0), ('rank_1.pflw', 1)],
    libmap_files=[('rank_0.libmap', 0), ('rank_1.libmap', 1)],
)
hotspot_node = HotspotAnalysisNode(top_n=10)
balance_node = BalanceAnalysisNode()

# Create graph and connect nodes
graph = DataflowGraph(name="MyWorkflow")
graph.add_node(load_node)
graph.add_node(hotspot_node)
graph.add_node(balance_node)
graph.connect(load_node, hotspot_node, 'tree')
graph.connect(load_node, balance_node, 'tree')

# Execute
results = graph.execute()
```

## API Reference

### Core Classes

#### `DataflowGraph`

The main class for composing analysis workflows.

```python
class DataflowGraph:
    def __init__(self, name: str = "DataflowGraph"):
        """Create an empty dataflow graph."""
    
    def add_node(self, node: DataflowNode) -> 'DataflowGraph':
        """Add a node to the graph."""
    
    def remove_node(self, node: DataflowNode) -> 'DataflowGraph':
        """Remove a node and its edges from the graph."""
    
    def connect(
        self,
        source: DataflowNode,
        target: DataflowNode,
        port: str,
        source_port: str = None,
        target_port: str = None
    ) -> 'DataflowGraph':
        """Connect two nodes with an edge."""
    
    def execute(
        self,
        executor: GraphExecutor = None,
        **kwargs
    ) -> Dict[str, Dict[str, Any]]:
        """Execute the dataflow graph."""
    
    def topological_sort(self) -> List[DataflowNode]:
        """Return nodes in execution order."""
    
    def get_parallel_groups(self) -> List[List[DataflowNode]]:
        """Group nodes that can execute in parallel."""
    
    def validate(self) -> bool:
        """Validate the graph structure."""
```

**Properties:**
- `name: str` - Human-readable name
- `nodes: List[DataflowNode]` - All nodes in the graph
- `edges: List[DataflowEdge]` - All edges in the graph

#### `DataflowNode`

Base class for analysis subtasks.

```python
class DataflowNode(ABC):
    def __init__(
        self,
        name: str,
        inputs: Dict[str, str] = None,
        outputs: Dict[str, str] = None
    ):
        """Initialize a dataflow node."""
    
    @abstractmethod
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Execute the analysis subtask."""
```

**Properties:**
- `name: str` - Human-readable name
- `node_id: str` - Unique identifier
- `state: NodeState` - Current execution state
- `inputs: Dict[str, str]` - Input port specifications
- `outputs: Dict[str, str]` - Output port specifications
- `result: Dict[str, Any]` - Execution result (after completion)

#### `DataflowEdge`

Represents a connection between nodes.

```python
class DataflowEdge:
    def __init__(
        self,
        source_node: DataflowNode,
        source_port: str,
        target_node: DataflowNode,
        target_port: str
    ):
        """Create a dataflow edge."""
```

**Properties:**
- `source_node: DataflowNode` - Source node
- `source_port: str` - Output port name
- `target_node: DataflowNode` - Target node
- `target_port: str` - Input port name

### Pre-built Analysis Nodes

#### `LoadDataNode`

Load performance sample data files.

```python
node = LoadDataNode(
    sample_files=[('rank_0.pflw', 0), ('rank_1.pflw', 1)],
    libmap_files=[('rank_0.libmap', 0), ('rank_1.libmap', 1)],  # optional
    mode='ContextFree',  # or 'ContextAware'
    count_mode='Both',   # or 'Exclusive', 'Inclusive'
    time_per_sample=1000.0,
    name="LoadData"
)
```

**Outputs:**
- `tree: PerformanceTree` - The built performance tree
- `builder: TreeBuilder` - The tree builder object

#### `HotspotAnalysisNode`

Find performance hotspots.

```python
node = HotspotAnalysisNode(
    top_n=10,
    mode='exclusive',  # or 'inclusive'
    name="HotspotAnalysis"
)
```

**Inputs:**
- `tree: PerformanceTree` - Tree to analyze

**Outputs:**
- `hotspots: List[HotspotInfo]` - Found hotspots
- `summary: dict` - Analysis summary

#### `BalanceAnalysisNode`

Analyze workload balance across processes.

```python
node = BalanceAnalysisNode(name="BalanceAnalysis")
```

**Inputs:**
- `tree: PerformanceTree` - Tree to analyze

**Outputs:**
- `balance: BalanceAnalysisResult` - Balance result
- `summary: dict` - Analysis summary

#### `FilterNode`

Filter tree nodes by criteria.

```python
node = FilterNode(
    min_samples=100,           # Minimum total samples
    max_samples=None,          # Maximum total samples
    min_self_samples=50,       # Minimum self samples
    function_pattern="compute_*",  # Function name pattern
    library_name="libmath.so", # Library name
    predicate=lambda n: n.depth < 5,  # Custom predicate
    name="Filter"
)
```

**Inputs:**
- `tree: PerformanceTree` - Tree to filter

**Outputs:**
- `nodes: List[TreeNode]` - Filtered nodes
- `count: int` - Number of matching nodes

#### `TreeTraversalNode`

Traverse tree and collect data.

```python
node = TreeTraversalNode(
    visitor=lambda n: n.function_name,  # Applied to each node
    order='preorder',  # 'preorder', 'postorder', or 'levelorder'
    stop_condition=lambda n: n.depth > 10,  # Stop traversal
    name="TreeTraversal"
)
```

**Inputs:**
- `tree: PerformanceTree` - Tree to traverse

**Outputs:**
- `results: list` - Collected results
- `visited_count: int` - Number of nodes visited

#### `TransformNode`

Apply custom transformations.

```python
node = TransformNode(
    transform=lambda inputs: {'result': inputs['value'] * 2},
    input_ports={'value': 'int'},
    output_ports={'result': 'int'},
    name="Transform"
)
```

#### `MergeNode`

Merge results from multiple nodes.

```python
node = MergeNode(
    input_ports=['hotspots', 'balance'],
    merge_function=lambda inputs: {...},  # Optional custom merge
    name="Merge"
)
```

**Outputs:**
- `merged: dict` - Merged results
- `keys: List[str]` - Input keys

#### `CustomNode`

Create custom analysis nodes.

```python
node = CustomNode(
    execute_fn=lambda inputs: {'result': compute(inputs)},
    input_ports={'tree': 'PerformanceTree'},
    output_ports={'result': 'Any'},
    name="Custom"
)
```

### Executors

#### `GraphExecutor`

Default sequential executor.

```python
executor = GraphExecutor()
results = graph.execute(executor=executor)

# Get execution times
times = executor.get_execution_times()
```

#### `ParallelExecutor`

Execute independent nodes in parallel.

```python
executor = ParallelExecutor(max_workers=4)

# Progress callback
def on_progress(completed, total, current):
    print(f"Progress: {completed}/{total} - {current}")

executor.set_progress_callback(on_progress)
results = graph.execute(executor=executor)
```

#### `CachingExecutor`

Cache results for repeated executions.

```python
executor = CachingExecutor(max_cache_size=100)

# First execution computes everything
results1 = graph.execute(executor=executor)

# Second execution uses cache
graph.reset()
results2 = graph.execute(executor=executor)

# Force recomputation
results3 = graph.execute(executor=executor, force_recompute=True)

# Cache statistics
stats = executor.get_cache_stats()
print(f"Hit rate: {stats['hit_rate']:.1%}")

# Clear cache
executor.clear_cache()
```

### WorkflowBuilder

Fluent API for building workflows.

```python
workflow = (
    WorkflowBuilder("MyAnalysis")
    .load_data(sample_files, libmap_files)
    .find_hotspots(top_n=10)
    .find_hotspots(top_n=10, mode='inclusive', name="InclusiveHotspots")
    .analyze_balance()
    .filter_nodes(min_samples=100)
    .traverse(visitor=lambda n: n.function_name)
    .custom(
        execute_fn=lambda inputs: {...},
        input_ports={...},
        output_ports={...}
    )
    .build()
)

# Execute with options
results = workflow.execute(
    parallel=True,   # Use ParallelExecutor
    caching=True,    # Use CachingExecutor
)
```

### Convenience Functions

```python
from perflow.dataflow.builder import (
    create_hotspot_workflow,
    create_balance_workflow,
    create_full_analysis_workflow,
)

# Quick hotspot analysis
graph = create_hotspot_workflow(sample_files, libmap_files, top_n=10)
results = graph.execute()

# Quick balance analysis
graph = create_balance_workflow(sample_files, libmap_files)
results = graph.execute()

# Full analysis (hotspots + balance)
graph = create_full_analysis_workflow(sample_files, libmap_files, top_n=10)
results = graph.execute()
```

## Advanced Usage

### Creating Custom Nodes

```python
from perflow.dataflow import DataflowNode

class LibraryStatsNode(DataflowNode):
    """Compute statistics per library."""
    
    def __init__(self, name="LibraryStats"):
        super().__init__(
            name=name,
            inputs={'tree': 'PerformanceTree'},
            outputs={'stats': 'dict', 'library_count': 'int'}
        )
    
    def execute(self, inputs):
        self.validate_inputs(inputs)
        tree = inputs['tree']
        
        stats = {}
        def visitor(node):
            lib = node.library_name
            if lib not in stats:
                stats[lib] = {'samples': 0, 'functions': 0}
            stats[lib]['samples'] += node.self_samples
            stats[lib]['functions'] += 1
            return True
        
        tree.traverse_preorder(visitor)
        
        return {
            'stats': stats,
            'library_count': len(stats)
        }

# Use in workflow
graph.add_node(LibraryStatsNode())
```

### Parallel Execution Optimization

The framework automatically identifies independent nodes:

```python
graph = DataflowGraph()
source = LoadDataNode(...)
analysis1 = HotspotAnalysisNode(mode='exclusive')
analysis2 = HotspotAnalysisNode(mode='inclusive')
analysis3 = BalanceAnalysisNode()

graph.add_node(source)
graph.add_node(analysis1)
graph.add_node(analysis2)
graph.add_node(analysis3)

# All analysis nodes depend only on source
graph.connect(source, analysis1, 'tree')
graph.connect(source, analysis2, 'tree')
graph.connect(source, analysis3, 'tree')

# View parallel groups
groups = graph.get_parallel_groups()
# groups[0]: [source]
# groups[1]: [analysis1, analysis2, analysis3]  # All run in parallel

# Execute with parallel executor
results = graph.execute(executor=ParallelExecutor(max_workers=3))
```

### Caching for Iterative Analysis

```python
executor = CachingExecutor()

# Repeated analysis with different parameters
for top_n in [5, 10, 20]:
    # Modify only the hotspot node
    hotspot_node._top_n = top_n
    
    graph.reset()
    results = graph.execute(executor=executor)
    
    # LoadDataNode result is cached, only HotspotAnalysisNode recomputes
    print(f"top_n={top_n}: {len(results[hotspot_node.node_id]['hotspots'])} hotspots")
```

## Best Practices

1. **Use WorkflowBuilder for simple workflows** - The fluent API is easier to read and write.

2. **Use manual graph construction for complex workflows** - When you need fine-grained control over connections.

3. **Use ParallelExecutor for large graphs** - Especially when you have many independent analysis tasks.

4. **Use CachingExecutor for iterative analysis** - When repeatedly analyzing with different parameters.

5. **Create reusable custom nodes** - Encapsulate common analysis patterns in custom node classes.

6. **Validate graphs before execution** - Call `graph.validate()` to catch configuration errors early.

## Examples

See `examples/dataflow_analysis_example.py` for comprehensive examples demonstrating:
- Basic workflow building
- Manual graph construction
- Parallel execution
- Caching optimization
- Custom nodes
- Tree traversal
