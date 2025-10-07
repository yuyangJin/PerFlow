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
  - Calling Context Tree (CCT) for hierarchical call path analysis
  - Built-in visualization: tree view and ring/sunburst charts
  - Hotspot detection and performance analysis
  - Load imbalance analyzer for parallel efficiency
  - Cache behavior analyzer for memory performance
  - Support for multiple performance metrics (cycles, instructions, cache misses, etc.)
  - Call stack analysis

- **Static Program Structure Analysis**: (S4 2025 Milestone 2)
  - Program Structure Graph (PSG) for representing complete code hierarchy
  - Communication Structure Tree (CST) - pruned PSG focusing on MPI operations
  - CST preserves call/loop/branch structure with MPI nodes as leaves
  - Support for functions, loops, basic blocks, and call sites
  - Call graph extraction and loop nesting analysis

- **Analysis Passes**: (S4 2025 Milestone 3)
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

### Communication Structure Tree (CST) Example
```python
from perflow.perf_data_struct.static import ProgramStructureGraph
from perflow.perf_data_struct.static.comm_structure_tree import CommStructureTree, CommType

# Build Program Structure Graph
psg = ProgramStructureGraph()
func = psg.addFunctionNode(1, "main", "main.c", 10)
loop = psg.addLoopNode(2, "compute_loop", parent_id=1, loop_type="for")
mpi_send = psg.addCallSite(3, caller_id=2, callee_name="MPI_Send")

# Build Communication Structure Tree by pruning PSG
mpi_nodes = {3}  # MPI_Send is an MPI operation
comm_types = {3: CommType.POINT_TO_POINT}
cst = CommStructureTree()
cst.buildFromPSG(psg, mpi_nodes, comm_types)

# Analyze communication structure
print(f"MPI nodes: {len(cst.getMPINodes())}")
print(f"Max nesting depth: {cst.getMaxNestingDepth()}")
patterns = cst.getCommunicationPattern()
print(f"Communication patterns: {patterns}")
```

## Documentation

- [Profile Analysis Guide](docs/profile_analysis.md)
- [CST Implementation Guide](docs/cst_implementation_guide.md)
- [Contributing Guide](docs/contributing.md)
- [Test Documentation](tests/README.md)

## Testing

- **283 tests** (259 unit + 25 integration) - 3 pre-existing failures unrelated to S4 2025 work
- **7 comprehensive examples** (including CCT/PSG/CST visualization and analysis)
- See [tests/README.md](tests/README.md) for details