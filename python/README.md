# PerFlow Python Frontend

A flexible, programmable Python interface for PerFlow performance analysis with dataflow graph abstraction.

## Features

- **Python Bindings**: Direct access to high-performance C++ backend
- **Dataflow Graphs**: Compose analysis workflows as directed acyclic graphs (DAGs)
- **Pre-built Tasks**: Ready-to-use analysis tasks for common operations
- **Optimization**: Automatic parallel execution, lazy evaluation, and result caching
- **Extensible**: Easy to add custom analysis tasks

## Installation

### Prerequisites

- Python 3.7 or later
- C++17 compiler
- CMake 3.14+
- PerFlow dependencies (MPI, libunwind, etc.)

### Build

```bash
cd PerFlow
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)
```

The Python bindings will be built automatically if Python development libraries are found. The compiled module will be located at `build/python/perflow/_perflow_bindings.so`.

### Running Without Installation

To use the Python module without installing, set PYTHONPATH to include the build directory:

```bash
# From the PerFlow root directory
export PYTHONPATH=$PWD/build/python:$PYTHONPATH

# Now you can import perflow
python3 -c "import perflow; print('Import successful')"

# Run examples
cd python/examples
python3 dataflow_example.py
```

### Install

For permanent installation:

```bash
# Install Python module system-wide
cd build
make install

# Or install in development mode (links to source)
cd python
pip install -e .
```

After installation, you can import perflow from anywhere without setting PYTHONPATH.

## Quick Start

**Note**: Before running examples, ensure either:
1. You have set `PYTHONPATH` to include the build directory: `export PYTHONPATH=/path/to/PerFlow/build/python:$PYTHONPATH`, or
2. You have installed the module with `make install` or `pip install -e .`

### Example 1: Basic Analysis Workflow

```python
import perflow
from perflow.workflows import create_basic_analysis_workflow

# Define sample files
sample_files = [
    ("/path/to/rank_0.pflw", 0),
    ("/path/to/rank_1.pflw", 1),
]

# Create and execute workflow
workflow = create_basic_analysis_workflow(
    sample_files=sample_files,
    output_dir="/path/to/output",
    top_n=10,
    parallel=True
)

results = workflow.execute()
```

### Example 2: Custom Dataflow Graph

```python
from perflow.dataflow import Graph
from perflow.analysis import LoadTreeTask, HotspotAnalysisTask

# Create a custom workflow
graph = Graph(name="MyAnalysis")

# Add tasks
load_node = graph.add_node(LoadTreeTask(sample_files=files))
hotspot_node = graph.add_node(HotspotAnalysisTask(top_n=10))

# Connect tasks
graph.add_edge(load_node, hotspot_node, "tree")

# Execute
results = graph.execute()
```

### Example 3: Direct C++ API Usage

```python
import perflow

# Create tree builder
builder = perflow.TreeBuilder(
    mode=perflow.TreeBuildMode.CONTEXT_FREE,
    count_mode=perflow.SampleCountMode.BOTH
)

# Load sample files
builder.build_from_files_parallel(sample_files)

# Analyze
tree = builder.tree()
balance = perflow.BalanceAnalyzer.analyze(tree)
hotspots = perflow.HotspotAnalyzer.find_hotspots(tree, 10)

print(f"Imbalance factor: {balance.imbalance_factor}")
for i, h in enumerate(hotspots, 1):
    print(f"{i}. {h.function_name}: {h.self_percentage:.2f}%")
```

## Architecture

### Dataflow Graph System

The dataflow graph system allows you to compose analysis workflows as DAGs:

- **Nodes**: Represent analysis subtasks (e.g., load tree, find hotspots)
- **Edges**: Represent data dependencies between tasks
- **Graph**: Manages execution scheduling and optimization

#### Optimization Features

1. **Parallel Execution**: Independent tasks are executed in parallel automatically
2. **Lazy Evaluation**: Only compute what's needed for the final output
3. **Result Caching**: Cache intermediate results to avoid recomputation
4. **Graph Fusion**: Combine compatible operations to reduce overhead

### Pre-built Tasks

The `perflow.analysis` module provides ready-to-use tasks:

- **LoadTreeTask**: Load performance tree from sample files
- **BalanceAnalysisTask**: Analyze workload distribution
- **HotspotAnalysisTask**: Find performance bottlenecks
- **FilterNodesTask**: Filter tree nodes based on criteria
- **TraverseTreeTask**: Traverse tree with custom visitor
- **ExportVisualizationTask**: Generate PDF visualizations
- **ReportTask**: Generate analysis reports (text, JSON, HTML)
- **AggregateResultsTask**: Aggregate multiple results

### Pre-built Workflows

The `perflow.workflows` module provides complete workflows:

- **create_basic_analysis_workflow**: Load, analyze, and visualize performance data
- **create_comparative_analysis_workflow**: Compare multiple datasets
- **create_hotspot_focused_workflow**: Detailed hotspot analysis

## API Reference

### Core Classes

#### TreeNode

Represents a node in the performance tree.

```python
node.frame()              # Get resolved frame information
node.total_samples()      # Get total sample count
node.self_samples()       # Get self sample count
node.children()           # Get child nodes
node.parent()             # Get parent node
```

#### PerformanceTree

Aggregates call stack samples into a tree structure.

```python
tree = perflow.PerformanceTree(
    mode=perflow.TreeBuildMode.CONTEXT_FREE,
    count_mode=perflow.SampleCountMode.BOTH
)

tree.root()               # Get root node
tree.total_samples()      # Get total samples
tree.process_count()      # Get number of processes
```

#### TreeBuilder

Constructs performance trees from sample data.

```python
builder = perflow.TreeBuilder()
builder.build_from_files(sample_files)
builder.build_from_files_parallel(sample_files, num_threads=4)
builder.load_library_maps(libmap_files)
tree = builder.tree()
```

#### TreeTraversal

Provides tree traversal algorithms.

```python
# Depth-first traversal
perflow.TreeTraversal.depth_first_preorder(root, visitor)

# Breadth-first traversal
perflow.TreeTraversal.breadth_first(root, visitor)

# Find nodes
nodes = perflow.TreeTraversal.find_nodes(root, predicate)
leaf_nodes = perflow.TreeTraversal.get_leaf_nodes(root)
```

### Analysis Classes

#### BalanceAnalyzer

```python
result = perflow.BalanceAnalyzer.analyze(tree)
print(f"Mean: {result.mean_samples}")
print(f"Imbalance: {result.imbalance_factor}")
```

#### HotspotAnalyzer

```python
# Find hotspots by self time (exclusive)
hotspots = perflow.HotspotAnalyzer.find_self_hotspots(tree, 10)

# Find hotspots by total time (inclusive)
hotspots = perflow.HotspotAnalyzer.find_total_hotspots(tree, 10)
```

### Dataflow Classes

#### Task

Base class for custom analysis tasks.

```python
class MyTask(perflow.Task):
    def execute(self, **inputs):
        # Process inputs
        result = do_analysis(inputs)
        return result
```

#### Graph

Dataflow graph for composing workflows.

```python
graph = perflow.Graph(name="MyWorkflow")
node1 = graph.add_node(task1)
node2 = graph.add_node(task2)
graph.add_edge(node1, node2, "data")
results = graph.execute()
```

## Examples

See the `examples/` directory for complete examples:

- `basic_workflow_example.py`: Basic analysis workflow
- `dataflow_example.py`: Custom dataflow graph construction

### Running Examples

**Option 1: Set PYTHONPATH** (if not installed)

```bash
# From PerFlow root directory
export PYTHONPATH=$PWD/build/python:$PYTHONPATH

cd python/examples
python3 dataflow_example.py
python3 basic_workflow_example.py /path/to/samples /path/to/output
```

**Option 2: After Installation**

```bash
cd python/examples
python3 dataflow_example.py
python3 basic_workflow_example.py /path/to/samples /path/to/output
```

## Advanced Usage

### Custom Analysis Tasks

Create custom tasks by extending the `Task` class:

```python
from perflow.dataflow import Task

class CustomAnalysisTask(Task):
    def __init__(self, threshold, name=None):
        super().__init__(name or "CustomAnalysis")
        self.threshold = threshold
    
    def execute(self, tree=None, **inputs):
        # Implement custom analysis logic
        nodes = perflow.TreeTraversal.find_nodes(
            tree.root(),
            lambda n: n.self_samples() > self.threshold
        )
        return nodes
```

### Custom Tree Visitors

Implement custom tree traversal logic:

```python
class MyVisitor(perflow.TreeVisitor):
    def __init__(self):
        super().__init__()
        self.hotspot_count = 0
    
    def visit(self, node, depth):
        if node.self_samples() > 1000:
            self.hotspot_count += 1
            print(f"Hotspot at depth {depth}: {node.frame().function_name}")
        return True  # Continue traversal

visitor = MyVisitor()
perflow.TreeTraversal.depth_first_preorder(tree.root(), visitor)
print(f"Found {visitor.hotspot_count} hotspots")
```

### Parallel File Loading

Load sample files in parallel for better performance:

```python
builder = perflow.TreeBuilder()

# Parallel loading (uses all available cores)
builder.build_from_files_parallel(
    sample_files=sample_files,
    time_per_sample=1000.0,
    num_threads=8  # Or 0 for auto-detection
)
```

## Performance Tips

1. **Use parallel loading** for large datasets with `build_from_files_parallel()`
2. **Enable graph optimization** with `graph.parallel_enabled = True`
3. **Use predicates** for efficient node filtering instead of full traversal
4. **Cache results** when executing the same workflow multiple times
5. **Load library maps** for better symbol resolution

## Troubleshooting

### Quick Diagnostic

Run the diagnostic script to check your setup:

```bash
cd /path/to/PerFlow
python/check_setup.sh
```

This will check:
- Build directory exists
- Python bindings compiled successfully
- Module location is correct
- PYTHONPATH is set properly
- Python can import the module

### Common Import Errors

#### Error: "No module named 'perflow'"

**Cause**: Python can't find the perflow package.

**Solution**:
1. Check if you've built PerFlow:
   ```bash
   ls build/python/perflow/_perflow_bindings*.so
   ```
   If not found, build it:
   ```bash
   mkdir build && cd build
   cmake .. -DPERFLOW_BUILD_TESTS=ON
   make -j$(nproc)
   ```

2. Set PYTHONPATH (from PerFlow root directory):
   ```bash
   export PYTHONPATH=$PWD/build/python:$PYTHONPATH
   ```

3. Verify the path is correct:
   ```bash
   echo $PYTHONPATH
   python3 -c "import sys; print(sys.path)"
   ```

#### Error: "cannot import name '_perflow_bindings'"

**Cause**: The C++ bindings module is not in the expected location.

**Solution**:
1. Verify the bindings were built:
   ```bash
   find build -name "_perflow_bindings*.so"
   ```

2. Check the expected location:
   ```bash
   ls -la build/python/perflow/
   ```

3. The file should be either:
   - `_perflow_bindings.so`, or
   - `_perflow_bindings.cpython-39-x86_64-linux-gnu.so` (with Python version)

4. If not in `build/python/perflow/`, check CMake output:
   ```bash
   cd build
   cmake .. 2>&1 | grep -i python
   ```

#### Error: "Python development libraries not found"

**Cause**: Python dev headers are not installed.

**Solution**: Install Python development package:
```bash
# Ubuntu/Debian
sudo apt-get install python3-dev

# RHEL/CentOS
sudo dnf install python3-devel

# Then rebuild
cd build && cmake .. && make
```

### Import Error

If you get `ImportError` when importing perflow:

**Problem**: The C++ bindings module `_perflow_bindings.so` is not in Python's search path.

**Solution 1**: Set PYTHONPATH to include the build directory:
```bash
export PYTHONPATH=/path/to/PerFlow/build/python:$PYTHONPATH
```

**Solution 2**: Install the module:
```bash
cd /path/to/PerFlow/build
make install
```

**Verify the module location**:
```bash
# Check if bindings are built
ls build/python/perflow/_perflow_bindings*.so

# The .so file should be present in build/python/perflow/
```

### Debugging Steps

If the above doesn't work, debug step-by-step:

1. **Verify your current directory**:
   ```bash
   pwd  # Should show /path/to/PerFlow
   ```

2. **Check if bindings exist**:
   ```bash
   ls -la build/python/perflow/
   ```

3. **Manually test Python path**:
   ```bash
   python3 -c "import sys; sys.path.insert(0, 'build/python'); import perflow; print('Success!')"
   ```

4. **Check for permission issues**:
   ```bash
   ls -l build/python/perflow/_perflow_bindings*.so
   # Should be readable (r-- or rw-)
   ```

5. **Try with absolute path**:
   ```bash
   export PYTHONPATH=/absolute/path/to/PerFlow/build/python:$PYTHONPATH
   python3 -c "import perflow"
   ```

### Still Having Issues?

See the detailed troubleshooting guide: [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

Or run the diagnostic script:
```bash
cd /path/to/PerFlow
bash python/check_setup.sh
```

If Python bindings don't build:

```bash
# Install Python development libraries
sudo apt-get install python3-dev  # Ubuntu/Debian
sudo dnf install python3-devel    # RHEL/CentOS

# Reconfigure and rebuild
cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)
```

## Contributing

We welcome contributions! To add new features:

1. **New C++ APIs**: Add to header files and update `bindings.cpp`
2. **New Python tasks**: Add to `perflow/analysis.py`
3. **New workflows**: Add to `perflow/workflows.py`
4. **Documentation**: Update this README and add docstrings

## License

Apache License 2.0 - See LICENSE file for details

## Citation

If you use PerFlow Python frontend in your research, please cite:

```bibtex
@software{perflow2024,
  title = {PerFlow: A Programmable Performance Analysis Tool for Parallel Programs},
  author = {Jin, Yuyang},
  year = {2024},
  url = {https://github.com/yuyangJin/PerFlow}
}
```
