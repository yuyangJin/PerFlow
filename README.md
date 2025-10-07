# PerFlow 2.0

A programmable and fast performance analysis for parallel programs

** Developing ... **

## Features

- **Trace Analysis**: Comprehensive support for analyzing execution traces from parallel applications
  - Multiple trace format readers (OTF2, VTune, Nsight, HPCToolkit, CTF, Scalatrace)
  - Trace replay with forward/backward analysis
  - Late sender detection and communication pattern analysis
  - Communication pattern analyzer for identifying patterns (all-to-all, nearest neighbor, hub, etc.)
  
- **Profile Analysis**: Profiling data analysis infrastructure (S4 2025 Milestone 1)
  - Profile data structures (PerfData, ProfileInfo, SampleData)
  - Hotspot detection and performance analysis
  - Load imbalance analyzer for parallel efficiency
  - Cache behavior analyzer for memory performance
  - Support for multiple performance metrics (cycles, instructions, cache misses, etc.)
  - Call stack analysis

- **Static Program Structure Analysis**: (S4 2025 Milestone 2)
  - Program Structure Graph (PSG) for representing code hierarchy
  - Communication Structure Tree (CST) for communication patterns
  - Support for functions, loops, basic blocks, and call sites
  - Call graph extraction and loop nesting analysis

- **Data Embedding**: (S4 2025 Milestone 3)
  - Function embedding based on structural and performance characteristics
  - Trace embedding for event sequence analysis
  - Profile embedding for performance metric patterns
  - Configurable embedding dimensionality

- **Analysis Passes**: (S4 2025 Milestone 4)
  - Hotspot detection
  - Load imbalance analysis
  - Cache behavior analysis
  - Communication pattern detection

- **Flow Framework**: Dataflow-based workflow management
  - Topological sort execution
  - Node and edge management
  - Cycle detection

## Install
```bash
pip install .
```

## Test
```bash
pytest tests/
```

## Quick Start

### Trace Analysis Example
```python
from perflow.task.trace_analysis.late_sender import LateSender
from perflow.perf_data_struct.dynamic.trace.trace import Trace

trace = Trace()
# ... add events to trace ...

analyzer = LateSender(trace)
analyzer.forwardReplay()

late_sends = analyzer.getLateSends()
print(f"Found {len(late_sends)} late senders")
```

### Profile Analysis Example
```python
from perflow.task.profile_analysis.hotspot_analyzer import HotspotAnalyzer
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData

profile = PerfData()
# ... add samples to profile ...

analyzer = HotspotAnalyzer(profile)
analyzer.analyze()

top_hotspots = analyzer.getTopHotspots("total_cycles", top_n=5)
for func_name, metrics in top_hotspots:
    print(f"{func_name}: {metrics['total_cycles']} cycles")
```

## Documentation

- [Profile Analysis Guide](docs/profile_analysis.md)
- [Contributing Guide](docs/contributing.md)
- [Test Documentation](tests/README.md)

## Testing

- **258 tests** (237 unit + 25 integration) - 3 pre-existing failures unrelated to S4 2025 work
- **5 comprehensive examples**
- See [tests/README.md](tests/README.md) for details