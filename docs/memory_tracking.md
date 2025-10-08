# Memory Consumption Tracking in Critical Path Analysis

## Overview

PerFlow's CriticalPathFinding now supports memory consumption tracking during the analysis process. This feature helps users understand the memory overhead of critical path analysis and optimize for large-scale trace analysis.

## Features

The memory tracking functionality records:
- Memory consumed by input traces
- Memory consumption during forward replay process
- Memory consumption during backward replay process
- Peak memory usage during the entire analysis
- Memory deltas for each phase

## Usage

### Enabling Memory Tracking

Memory tracking is disabled by default and can be enabled in two ways:

#### Option 1: Enable via Constructor

```python
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding

# Create analyzer with memory tracking enabled
analyzer = CriticalPathFinding(enable_memory_tracking=True)
```

#### Option 2: Enable via Setter Method

```python
analyzer = CriticalPathFinding()
analyzer.setEnableMemoryTracking(True)
```

### Running Analysis with Memory Tracking

```python
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
from perflow.perf_data_struct.dynamic.trace.trace import Trace

# Create or load a trace
trace = Trace()
# ... add events to trace ...

# Create analyzer with memory tracking
analyzer = CriticalPathFinding(enable_memory_tracking=True)
analyzer.get_inputs().add_data(trace)

# Run analysis
analyzer.run()

# Get memory statistics
memory_stats = analyzer.getMemoryStatistics()
```

### Retrieving Memory Statistics

```python
# Get all memory statistics
memory_stats = analyzer.getMemoryStatistics()

# Access individual statistics
if 'trace_memory_bytes' in memory_stats:
    trace_mb = memory_stats['trace_memory_bytes'] / (1024 * 1024)
    print(f"Input trace memory: {trace_mb:.2f} MB")

if 'forward_replay_delta_bytes' in memory_stats:
    fwd_delta_mb = memory_stats['forward_replay_delta_bytes'] / (1024 * 1024)
    print(f"Forward replay memory delta: {fwd_delta_mb:.2f} MB")

if 'backward_replay_delta_bytes' in memory_stats:
    bwd_delta_mb = memory_stats['backward_replay_delta_bytes'] / (1024 * 1024)
    print(f"Backward replay memory delta: {bwd_delta_mb:.2f} MB")

if 'peak_memory_bytes' in memory_stats:
    peak_mb = memory_stats['peak_memory_bytes'] / (1024 * 1024)
    print(f"Peak memory usage: {peak_mb:.2f} MB")
```

## Available Memory Statistics

When memory tracking is enabled, the following statistics are available:

| Statistic | Description |
|-----------|-------------|
| `trace_memory_bytes` | Estimated memory size of the input trace |
| `forward_replay_start_memory_bytes` | Process memory at the start of forward replay |
| `forward_replay_end_memory_bytes` | Process memory at the end of forward replay |
| `forward_replay_delta_bytes` | Memory change during forward replay |
| `backward_replay_start_memory_bytes` | Process memory at the start of backward replay |
| `backward_replay_end_memory_bytes` | Process memory at the end of backward replay |
| `backward_replay_delta_bytes` | Memory change during backward replay |
| `peak_memory_bytes` | Peak memory usage during the entire analysis |

## Example

```python
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType

# Create a simple trace
trace = Trace()
trace_info = TraceInfo(pid=0, tid=0)
trace.setTraceInfo(trace_info)

# Add some events
for i in range(10):
    event = Event(
        event_type=EventType.COMPUTE,
        idx=i,
        name=f"Event_{i}",
        pid=0,
        tid=0,
        timestamp=i * 0.1
    )
    trace.addEvent(event)

# Analyze with memory tracking
analyzer = CriticalPathFinding(enable_memory_tracking=True)
analyzer.get_inputs().add_data(trace)
analyzer.run()

# Display memory statistics
memory_stats = analyzer.getMemoryStatistics()
print("Memory Consumption Statistics:")
for key, value in memory_stats.items():
    if 'bytes' in key:
        mb_value = value / (1024 * 1024)
        print(f"  {key}: {mb_value:.2f} MB")
```

## Performance Impact

Enabling memory tracking has minimal performance impact:
- Memory measurements use `psutil` for process memory (RSS)
- Trace size estimation uses `sys.getsizeof()` for object sizes
- Measurements are taken only at key points (not per-event)

## Use Cases

Memory tracking is particularly useful for:
- Large-scale trace analysis where memory is a concern
- Optimizing memory usage in production environments
- Understanding memory requirements for different trace sizes
- Debugging memory-related issues in analysis workflows
- Capacity planning for analysis infrastructure

## Disabling Memory Tracking

Memory tracking can be disabled at any time:

```python
analyzer.setEnableMemoryTracking(False)
```

When disabled, no memory measurements are taken, and `getMemoryStatistics()` returns an empty dictionary.

## See Also

- Example: `tests/examples/example_critical_path_finding.py` (Example 4)
- Tests: `tests/unit/test_memory_tracking.py`
- Implementation: `perflow/task/trace_analysis/critical_path_finding.py`
