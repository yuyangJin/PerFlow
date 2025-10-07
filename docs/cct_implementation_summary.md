# CallingContextTree (CCT) Implementation Summary

## Overview

This document summarizes the implementation of the CallingContextTree (CCT) class, which was requested in issue comment #3376346243.

## What is a Calling Context Tree?

A Calling Context Tree (CCT) is a hierarchical data structure that represents the calling contexts in profiling data. Unlike simple function-level aggregation, a CCT distinguishes between the same function called from different call paths, enabling more precise performance analysis.

## Implementation Details

### Location
- **File**: `perflow/perf_data_struct/dynamic/profile/calling_context_tree.py`
- **Class**: `CallingContextTree`
- **Extends**: `Tree` (from base classes)

### Key Features

1. **Hierarchical Structure**: Built as a tree where each node represents a unique calling context
2. **Built from ProfileData**: Integrates with existing profiling infrastructure
3. **Exclusive Metrics**: Metrics for a specific calling context only
4. **Inclusive Metrics**: Metrics including all descendant contexts
5. **Hot Path Detection**: Identifies the hottest calling paths
6. **Function Statistics**: Aggregates statistics across all contexts for a function

### Core Methods

#### Building the Tree
```python
cct = CallingContextTree()
cct.buildFromProfile(profile_data)  # Build from PerfData
```

#### Querying Metrics
```python
# Get metrics for a specific context
exclusive = cct.getExclusiveMetrics(node_id)
inclusive = cct.getInclusiveMetrics(node_id)

# Get sample count
count = cct.getNodeSampleCount(node_id)
```

#### Analysis Methods
```python
# Find hottest calling paths
hot_paths = cct.getHotPaths("cycles", top_n=10)

# Get statistics for a function across all contexts
stats = cct.getFunctionStats("matrix_multiply")

# Get maximum calling depth
depth = cct.getContextDepth()
```

## Testing

### Test Coverage
- **16 comprehensive unit tests** in `tests/unit/test_calling_context_tree.py`
- All tests passing (100%)
- Tests cover:
  - CCT creation and building
  - Nested and shared calling contexts
  - Exclusive and inclusive metrics
  - Hot path detection
  - Function statistics
  - Edge cases (empty profiles, missing data)

### Test Categories

1. **Creation Tests** (2 tests)
   - Empty CCT creation
   - Building from empty profile

2. **Building Tests** (3 tests)
   - Simple call stacks
   - Nested call stacks
   - Shared call paths

3. **Metrics Tests** (3 tests)
   - Exclusive metrics
   - Inclusive metrics
   - Metric aggregation

4. **Query Tests** (3 tests)
   - Hot path detection
   - Context depth calculation
   - Function statistics

5. **Edge Cases** (4 tests)
   - Samples without call stacks
   - Samples without function names
   - Clearing the CCT
   - Rebuilding from different profiles

6. **Integration Tests** (1 test)
   - Complete workflow with realistic data

## Visualization

### Implementation
- **File**: `tests/examples/example_cct_psg_visualization.py`
- **Formats**: Text and DOT (Graphviz)

### Features

#### Text Visualization
- Hierarchical tree display with indentation
- Shows exclusive and inclusive metrics
- Lists sample counts per node
- Displays hot paths

Example output:
```
├─ <root>
│  Samples: 0
│  Inclusive: cycles: 109,500
│
  ├─ main
  │  Samples: 50
  │  Exclusive: cycles: 50,000
  │  Inclusive: cycles: 50,000
```

#### DOT Format Visualization
- Generates `.dot` files for Graphviz
- Color-coded nodes based on sample counts:
  - Red: > 40 samples (hot)
  - Orange: 20-40 samples
  - Yellow: 10-20 samples
  - Light blue: < 10 samples
- Shows metrics on nodes
- Directed edges show calling relationships

Generated files:
- `cct_example.dot`: CCT visualization
- `psg_example.dot`: PSG visualization

To generate images:
```bash
dot -Tpng cct_example.dot -o cct.png
dot -Tsvg cct_example.dot -o cct.svg
```

## Integration with Existing Code

The CCT integrates seamlessly with existing PerFlow infrastructure:

1. **ProfileData Integration**: Built directly from `PerfData` objects
2. **SampleData Compatibility**: Uses call stacks from `SampleData`
3. **Tree Base Class**: Extends the `Tree` base class for consistency
4. **Flow Framework**: Can be used in analysis workflows

## Use Cases

The CCT enables several important analysis scenarios:

1. **Context-Sensitive Profiling**: Distinguish performance of the same function in different contexts
2. **Call Path Analysis**: Identify specific execution paths that are performance bottlenecks
3. **Root Cause Analysis**: Trace back from hotspots to their calling contexts
4. **Comparative Analysis**: Compare the same function's performance across different call paths

## Example Usage

### Basic Usage
```python
from perflow.perf_data_struct.dynamic.profile.calling_context_tree import CallingContextTree
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData

# Build CCT from profile
cct = CallingContextTree()
cct.buildFromProfile(profile)

# Find hot paths
hot_paths = cct.getHotPaths("cycles", top_n=5)
for path, cycles in hot_paths:
    print(f"{' -> '.join(path)}: {cycles:,.0f} cycles")
```

### Advanced Analysis
```python
# Get statistics for a specific function
stats = cct.getFunctionStats("matrix_multiply")
print(f"Total samples: {stats['total_samples']}")
print(f"Appears in {stats['num_contexts']} contexts")

# Compare exclusive vs inclusive
for context, node_id in cct.m_context_nodes.items():
    exclusive = cct.getExclusiveMetrics(node_id)
    inclusive = cct.getInclusiveMetrics(node_id)
    
    if exclusive.get("cycles", 0) > 0:
        ratio = inclusive["cycles"] / exclusive["cycles"]
        print(f"{' -> '.join(context)}: ratio = {ratio:.2f}")
```

## Documentation

Complete documentation is available in:
- `docs/profile_analysis.md`: CCT API reference and usage guide
- `tests/examples/example_cct_psg_visualization.py`: Complete working example
- `README.md`: Quick start examples

## Performance Considerations

1. **Memory**: The CCT stores one node per unique calling context. For deep call stacks with many unique paths, memory usage can be significant.

2. **Build Time**: Building the CCT is O(n*d) where n is the number of samples and d is the average call stack depth.

3. **Query Time**: 
   - Node queries: O(1)
   - Hot path queries: O(n log n) for sorting
   - Inclusive metrics: O(subtree size)

## Future Enhancements

Possible future improvements:

1. **Compression**: Merge nodes with identical subtrees
2. **Filtering**: Build CCT with only contexts above a threshold
3. **Incremental Building**: Update CCT with new samples without rebuilding
4. **Serialization**: Save/load CCT to/from disk
5. **Interactive Visualization**: Web-based interactive CCT explorer

## Summary

The CallingContextTree implementation provides:
- ✅ Built upon ProfileData as requested
- ✅ Comprehensive testing (16 tests, 100% passing)
- ✅ Text and DOT visualization examples
- ✅ Complete documentation
- ✅ Integration with existing infrastructure
- ✅ Production-ready code

The CCT complements the existing profiling infrastructure and enables more sophisticated performance analysis by preserving calling context information.
