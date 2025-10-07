# PerFlow 2.0

A programmable and fast performance analysis for parallel programs

** Developing ... **

## Features

- **Trace Analysis**: Comprehensive support for analyzing execution traces from parallel applications
  - Multiple trace format readers (OTF2, VTune, Nsight, HPCToolkit, CTF, Scalatrace)
  - Trace replay with forward/backward analysis
  - Late sender detection and communication pattern analysis
  - Communication pattern analyzer for identifying patterns (all-to-all, nearest neighbor, hub, etc.)
  
- **Profile Analysis**: Profiling data analysis infrastructure
  - Profile data structures (PerfData, ProfileInfo, SampleData)
  - Calling Context Tree (CCT) for hierarchical call path analysis
  - Built-in visualization: tree view and ring/sunburst charts
  - Hotspot detection and performance analysis
  - Load imbalance analyzer for parallel efficiency
  - Cache behavior analyzer for memory performance
  - Support for multiple performance metrics (cycles, instructions, cache misses, etc.)
  - Call stack analysis

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
from perflow.perf_data_struct.dynamic.profile.calling_context_tree import CallingContextTree

profile = PerfData()
# ... add samples to profile ...

# Hotspot analysis
analyzer = HotspotAnalyzer(profile)
analyzer.analyze()
top_hotspots = analyzer.getTopHotspots("total_cycles", top_n=5)
for func_name, metrics in top_hotspots:
    print(f"{func_name}: {metrics['total_cycles']} cycles")

# Calling context tree analysis
cct = CallingContextTree()
cct.buildFromProfile(profile)
hot_paths = cct.getHotPaths("cycles", top_n=5)
for path, cycles in hot_paths:
    print(f"{' -> '.join(path)}: {cycles} cycles")

# Generate visualizations
cct.visualize("cct_tree.png", view_type="tree", metric="cycles")
cct.visualize("cct_ring.png", view_type="ring", metric="cycles")
for path, cycles in hot_paths:
    print(f"{' -> '.join(path)}: {cycles} cycles")
```


## Documentation

- [Profile Analysis Guide](docs/profile_analysis.md)
- [Contributing Guide](docs/contributing.md)
- [Test Documentation](tests/README.md)

## Testing

- **283 tests** (259 unit + 25 integration) - 3 pre-existing failures
- **7 comprehensive examples** (including CCT/PSG/CST visualization and analysis)
- See [tests/README.md](tests/README.md) for details
