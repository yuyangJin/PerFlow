# PerFlow 2.0

A programmable and fast performance analysis for parallel programs

** Developing ... **

## Features

- **Trace Analysis**: Comprehensive support for analyzing execution traces from parallel applications
  - Multiple trace format readers (OTF2, VTune, Nsight, HPCToolkit, CTF, Scalatrace)
  - Trace replay with forward/backward analysis
  - Late sender detection and communication pattern analysis
  
- **Profile Analysis**: Profiling data analysis infrastructure (NEW in S4 2025 Milestone 1)
  - Profile data structures (PerfData, ProfileInfo, SampleData)
  - Hotspot detection and performance analysis
  - Support for multiple performance metrics (cycles, instructions, cache misses, etc.)
  - Call stack analysis

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

- **243 tests** (218 unit + 25 integration)
- **5 comprehensive examples**
- See [tests/README.md](tests/README.md) for details