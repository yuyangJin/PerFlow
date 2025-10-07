# Profile Analysis in PerFlow

PerFlow 2.0 provides comprehensive support for analyzing profiling data from parallel applications. This document describes the profile analysis infrastructure and how to use it.

## Overview

The profile analysis module provides:

- **Profile Data Structures**: Data structures for representing profiling samples and metadata
- **Calling Context Tree (CCT)**: Hierarchical representation of calling contexts for detailed analysis
- **Profile Analyzers**: Base infrastructure for implementing profile analysis passes
- **Hotspot Analysis**: Built-in analyzer for identifying performance hotspots
- **Integration with Flow Framework**: Seamless integration with PerFlow's dataflow framework

## Profile Data Structures

### SampleData

`SampleData` represents a single profiling sample from program execution. Each sample captures:

- **Location Information**: Function name, module name, source file, line number, instruction pointer
- **Execution Context**: Process ID, thread ID, timestamp
- **Performance Metrics**: Cycles, instructions, cache misses, etc.
- **Call Stack**: Call stack at the time of sampling

```python
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData

# Create a sample
sample = SampleData(
    timestamp=1.5,
    pid=1000,
    tid=2000,
    function_name="matrix_multiply",
    module_name="libcompute.so"
)

# Add performance metrics
sample.setMetric("cycles", 50000.0)
sample.setMetric("instructions", 25000.0)
sample.setMetric("cache_misses", 500.0)

# Build call stack
sample.addToCallStack("main")
sample.addToCallStack("compute_batch")
sample.addToCallStack("matrix_multiply")
```

### ProfileInfo

`ProfileInfo` contains metadata about a profile collection:

- **Process/Thread Information**: Local process/thread IDs
- **Global Metadata**: Total processes, threads, profiling configuration
- **Timing Information**: Start time, end time, duration
- **Metrics**: List of collected performance metrics

```python
from perflow.perf_data_struct.dynamic.profile.perf_data import ProfileInfo

# Create profile metadata
profile_info = ProfileInfo(
    pid=1000,
    tid=0,
    num_processes=4,
    num_threads=8,
    profile_format="VTune",
    sample_rate=1000.0,  # Hz
    application_name="scientific_app"
)

# Add metrics
profile_info.addMetric("cycles")
profile_info.addMetric("instructions")
profile_info.addMetric("cache_misses")
```

### PerfData

`PerfData` is a collection of profiling samples with associated metadata:

```python
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData

# Create profile data
profile = PerfData()
profile.setProfileInfo(profile_info)

# Add samples
profile.addSample(sample)

# Aggregate data by function
aggregated = profile.aggregateByFunction()

# Get top functions by metric
top_functions = profile.getTopFunctions("cycles", top_n=10)
```

## Profile Analysis

### ProfileAnalyzer

`ProfileAnalyzer` is the base class for implementing profile analysis passes. It provides:

- **Callback Support**: Register callbacks to process each sample
- **Flow Integration**: Inherits from FlowNode for workflow integration
- **Sample Iteration**: Automatic iteration through profiling samples

```python
from perflow.task.profile_analysis.profile_analyzer import ProfileAnalyzer

# Create analyzer
analyzer = ProfileAnalyzer(profile)

# Register callback
def process_sample(sample):
    func_name = sample.getFunctionName()
    cycles = sample.getMetric("cycles")
    print(f"{func_name}: {cycles} cycles")

analyzer.registerCallback("processor", process_sample)

# Run analysis
analyzer.analyze()
```

### HotspotAnalyzer

`HotspotAnalyzer` identifies performance hotspots in profiling data:

```python
from perflow.task.profile_analysis.hotspot_analyzer import HotspotAnalyzer

# Create analyzer
analyzer = HotspotAnalyzer(profile, threshold=0.0)

# Run analysis
analyzer.analyze()

# Get all hotspots
hotspots = analyzer.getHotspots()

# Get top hotspots by cycles
top_hotspots = analyzer.getTopHotspots("total_cycles", top_n=5)

for func_name, metrics in top_hotspots:
    cycles = metrics["total_cycles"]
    samples = metrics["sample_count"]
    percentage = analyzer.getHotspotPercentage(func_name)
    print(f"{func_name}: {cycles} cycles ({percentage:.2f}% of execution)")

# Filter by threshold
high_hotspots = analyzer.getHotspotsByThreshold("total_cycles", threshold=100000.0)
```

## Workflow Integration

Profile analyzers integrate seamlessly with PerFlow's dataflow framework:

```python
from perflow.flow.flow import FlowGraph

# Create workflow
workflow = FlowGraph()

# Create analyzer node
analyzer = HotspotAnalyzer()
analyzer.get_inputs().add_data(profile)

# Add to workflow
workflow.add_node(analyzer)

# Run workflow
workflow.run()

# Access results
print(f"Total samples: {analyzer.getTotalSamples()}")
print(f"Hotspots found: {len(analyzer.getHotspots())}")
```

## Implementing Custom Analyzers

You can implement custom profile analyzers by extending `ProfileAnalyzer`:

```python
from perflow.task.profile_analysis.profile_analyzer import ProfileAnalyzer
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData

class MyCustomAnalyzer(ProfileAnalyzer):
    def __init__(self, profile=None):
        super().__init__(profile)
        self.results = {}
        
        # Register analysis callback
        self.registerCallback("my_analysis", self._analyze_sample)
    
    def _analyze_sample(self, sample: SampleData):
        """Custom analysis logic for each sample."""
        func_name = sample.getFunctionName()
        if func_name:
            # Your custom analysis logic here
            if func_name not in self.results:
                self.results[func_name] = 0
            self.results[func_name] += 1
    
    def get_results(self):
        """Get analysis results."""
        return self.results

# Use custom analyzer
analyzer = MyCustomAnalyzer(profile)
analyzer.analyze()
results = analyzer.get_results()
```

## Examples

Complete examples are available in the `tests/examples/` directory:

- **example_hotspot_analysis.py**: Comprehensive hotspot analysis workflow

## API Reference

### SampleData

- `getTimestamp()`, `setTimestamp(timestamp)`
- `getPid()`, `setPid(pid)`
- `getTid()`, `setTid(tid)`
- `getFunctionName()`, `setFunctionName(name)`
- `getModuleName()`, `setModuleName(name)`
- `getSourceFile()`, `setSourceFile(file)`
- `getLineNumber()`, `setLineNumber(line)`
- `getInstructionPointer()`, `setInstructionPointer(ip)`
- `getMetrics()`, `setMetrics(metrics)`
- `getMetric(name)`, `setMetric(name, value)`
- `getCallStack()`, `setCallStack(stack)`
- `addToCallStack(frame)`
- `getCallStackDepth()`

### ProfileInfo

- `getPid()`, `setPid(pid)`
- `getTid()`, `setTid(tid)`
- `getNumProcesses()`, `setNumProcesses(num)`
- `getNumThreads()`, `setNumThreads(num)`
- `getProfileFormat()`, `setProfileFormat(format)`
- `getSampleRate()`, `setSampleRate(rate)`
- `getProfileStartTime()`, `setProfileStartTime(time)`
- `getProfileEndTime()`, `setProfileEndTime(time)`
- `getProfileDuration()`
- `getApplicationName()`, `setApplicationName(name)`
- `getMetrics()`, `setMetrics(metrics)`
- `addMetric(metric)`

### PerfData

- `addSample(sample)`
- `getSamples()`
- `getSample(index)`
- `getSampleCount()`
- `getProfileInfo()`, `setProfileInfo(info)`
- `clear()`
- `aggregateByFunction()`
- `getAggregatedData()`
- `getTopFunctions(metric, top_n=10)`

### ProfileAnalyzer

- `setProfile(profile)`, `getProfile()`
- `registerCallback(name, callback)`
- `unregisterCallback(name)`
- `clearCallbacks()`
- `getCallbacks()`
- `analyze()`
- `run()`

### HotspotAnalyzer

- Inherits all ProfileAnalyzer methods
- `getHotspots()`
- `getTopHotspots(metric="total_cycles", top_n=10)`
- `getHotspotsByThreshold(metric="total_cycles", threshold=None)`
- `getTotalSamples()`
- `getHotspotPercentage(function_name)`
- `clear()`

## Performance Tips

1. **Use Appropriate Thresholds**: When analyzing large profiles, use thresholds to filter out noise
2. **Aggregate Data**: Use `aggregateByFunction()` for function-level summaries
3. **Batch Processing**: Process multiple profiles in a single workflow
4. **Custom Callbacks**: Implement custom callbacks for specialized analysis

## Future Extensions

The profile analysis infrastructure is designed to be extensible. Future additions may include:

- Memory allocation profiling
- Thread contention analysis
- GPU profiling support
- Cross-profile comparison utilities
- Automated optimization recommendations

## Calling Context Tree (CCT)

### Overview

The `CallingContextTree` represents the hierarchical structure of calling contexts in profiling data. Each node in the CCT represents a unique calling context (a function and the call path leading to it). This enables detailed analysis of performance in different calling contexts.

### Building a CCT

```python
from perflow.perf_data_struct.dynamic.profile.calling_context_tree import CallingContextTree
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData

# Create CCT
cct = CallingContextTree()

# Build from profiling data
cct.buildFromProfile(profile)

print(f"CCT has {cct.getNodeCount()} nodes")
print(f"Maximum calling depth: {cct.getContextDepth()}")
```

### Exclusive vs Inclusive Metrics

The CCT supports both exclusive and inclusive metrics:

- **Exclusive metrics**: Metrics attributed only to the specific context (samples collected at that exact call path)
- **Inclusive metrics**: Metrics including all descendant contexts (sum of all samples in subtree)

```python
# Get a node ID from a calling context
context = ("main", "compute", "matrix_multiply")
node_id = cct.m_context_nodes[context]

# Get exclusive metrics (only this exact context)
exclusive = cct.getExclusiveMetrics(node_id)
print(f"Exclusive cycles: {exclusive['cycles']}")

# Get inclusive metrics (this context + all descendants)
inclusive = cct.getInclusiveMetrics(node_id)
print(f"Inclusive cycles: {inclusive['cycles']}")
```

### Hot Path Analysis

Identify the hottest calling paths in your application:

```python
# Get top 10 hottest paths by cycles
hot_paths = cct.getHotPaths("cycles", top_n=10)

for path, cycles in hot_paths:
    path_str = " -> ".join(path)
    print(f"{path_str}: {cycles:,.0f} cycles")
```

### Function Statistics

Get aggregated statistics for a function across all calling contexts:

```python
# Get stats for a function across all contexts
stats = cct.getFunctionStats("matrix_multiply")

print(f"Total samples: {stats['total_samples']}")
print(f"Appears in {stats['num_contexts']} different contexts")
print(f"Total cycles: {stats['total_metrics']['cycles']:,.0f}")

# List all contexts
for context, sample_count in stats['contexts']:
    print(f"  {' -> '.join(context)}: {sample_count} samples")
```

### Visualization

The CCT can be visualized using text or DOT format:

```python
# Text visualization (for debugging)
def print_tree(node_id, indent=0):
    node = cct.getNode(node_id)
    metrics = cct.getNodeMetrics(node_id)
    print("  " * indent + f"{node.getName()}: {metrics.get('cycles', 0)} cycles")
    for child_id in cct.getChildren(node_id):
        print_tree(child_id, indent + 1)

# DOT format for Graphviz
# See tests/examples/example_cct_psg_visualization.py for complete example
```

### API Reference - CallingContextTree

- `buildFromProfile(perf_data)`: Build CCT from profiling data
- `getNodeMetrics(node_id)`: Get exclusive metrics for a node
- `getNodeSampleCount(node_id)`: Get sample count for a node
- `getInclusiveMetrics(node_id)`: Get inclusive metrics (including descendants)
- `getExclusiveMetrics(node_id)`: Get exclusive metrics (excluding descendants)
- `getHotPaths(metric, top_n)`: Get hottest calling paths
- `getContextDepth()`: Get maximum depth of calling contexts
- `getFunctionStats(function_name)`: Get statistics for a function across all contexts
- `clear()`: Clear the CCT

## Examples

Complete examples are available in the `tests/examples/` directory:

- **example_hotspot_analysis.py**: Comprehensive hotspot analysis workflow
- **example_cct_psg_visualization.py**: CCT and PSG visualization with text and DOT formats

