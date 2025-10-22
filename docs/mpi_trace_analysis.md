# MPI-Based Parallel Trace Analysis

This document describes PerFlow's MPI-based parallel trace analysis capability, which enables efficient analysis of large-scale parallel application traces by distributing the workload across multiple replay processes.

## Overview

Large-scale parallel applications can generate extremely large trace files that are challenging to analyze on a single machine. PerFlow's MPI-based trace analysis addresses this by:

1. **Distributing traces** by execution process rank across multiple replay processes
2. **Parallel replay** where each replay process handles its assigned traces
3. **Inter-process event data access** for analysis tasks that need data from other processes
4. **Load balancing** through even distribution of execution processes

## Architecture

### Components

1. **MPIConfig**: Manages MPI configuration and provides singleton access
2. **TraceDistributor**: Distributes traces across replay processes using round-robin assignment
3. **EventDataFetcher**: Provides transparent access to event data (local or remote)
4. **MPITraceReplayer**: Extends TraceReplayer with MPI-based parallel replay

### Trace Distribution

Traces are distributed by execution process (EP) rank to replay processes (RP) using the formula:

```
rp_id = ep_id % num_replay_processes
```

This ensures even load balancing across replay processes.

## Usage

### Basic Example

```python
from perflow.task.trace_analysis.low_level.mpi_trace_replayer import MPITraceReplayer
from perflow.task.trace_analysis.low_level.mpi_config import MPIConfig

# Initialize MPI
mpi_config = MPIConfig.get_instance()
mpi_config.enable_mpi()

# Create replayer
replayer = MPITraceReplayer(enable_mpi=True)

# Distribute traces
distributed_traces = replayer.distribute_traces(
    all_traces,
    num_execution_processes=16,
    num_replay_processes=8
)

# Replay
replayer.forwardReplay()
```

### Running with MPI

```bash
# Sequential mode (no MPI)
python your_analysis_script.py

# Parallel mode with 8 processes
mpirun -np 8 python your_analysis_script.py
```

## Features

### 1. Enable/Disable MPI Mode

MPI mode can be enabled or disabled programmatically:

```python
mpi_config = MPIConfig.get_instance()

# Enable MPI (returns True if successful)
if mpi_config.enable_mpi():
    print("MPI enabled")
else:
    print("MPI not available or initialization failed")

# Disable MPI
mpi_config.disable_mpi()
```

### 2. Trace Distribution

Traces are distributed evenly across replay processes:

```python
distributor = TraceDistributor(
    num_execution_processes=16,
    num_replay_processes=8
)

# Compute distribution mapping
mapping = distributor.compute_distribution()

# Get load balance information
load_info = distributor.get_load_balance_info()
```

### 3. Parallel Replay

Each replay process replays only its assigned traces:

```python
replayer = MPITraceReplayer(enable_mpi=True)
replayer.distribute_traces(traces, num_execution_processes=16)

# Register callbacks for analysis
replayer.registerCallback("analyzer", analysis_callback, ReplayDirection.FWD)

# Replay (only processes assigned traces)
replayer.forwardReplay()
```

### 4. Inter-Process Event Data Fetch

Access event data regardless of where it resides:

```python
# Get event data (automatically handles local vs. remote)
event_data = replayer.get_event_data(ep_id=5, event_idx=100)

if event_data:
    print(f"Event: {event_data['name']}")
    print(f"Timestamp: {event_data['timestamp']}")
```

The EventDataFetcher:
- Uses **intra-process memory access** for local events (fast)
- Uses **MPI communication** for remote events (transparent)
- **Caches** remote data to minimize communication overhead

## Example: Late Sender Analysis

See `tests/examples/example_mpi_late_sender_analysis.py` for a complete example demonstrating MPI-based late sender detection with 8 replay processes.

### Key Features of the Example

- Simulates 16 execution processes
- Distributes across 8 replay processes
- Detects late sender communication patterns
- Gathers results from all processes
- Works in both sequential and parallel modes

### Running the Example

```bash
# Sequential mode
python tests/examples/example_mpi_late_sender_analysis.py

# Parallel mode with 8 processes
mpirun -np 8 python tests/examples/example_mpi_late_sender_analysis.py
```

## Implementation Details

### MPI Configuration

The `MPIConfig` class:
- Implements singleton pattern for global access
- Gracefully handles environments without mpi4py
- Provides rank, size, and communicator access
- Includes helper methods (is_root, barrier, etc.)

### Trace Distribution Algorithm

The round-robin distribution ensures:
- Even load balancing
- Deterministic assignment
- Simple mapping computation
- Works with any number of EPs and RPs

### Event Data Fetcher

The EventDataFetcher:
1. Checks local traces first (O(1) lookup)
2. Falls back to MPI communication for remote data
3. Caches fetched data to avoid repeated communication
4. Handles missing data gracefully

### Communication Protocol

Inter-process event data fetch uses a simple protocol:
- Request: `('fetch', ep_id, event_idx)`
- Response: event data dictionary or None

## Testing

### Unit Tests (50 tests)

- `test_mpi_config.py`: MPI configuration (7 tests)
- `test_trace_distributor.py`: Trace distribution (15 tests)
- `test_event_data_fetcher.py`: Event data fetch (13 tests)
- `test_mpi_trace_replayer.py`: MPI replayer (15 tests)

### Integration Tests (7 tests)

- `test_mpi_trace_analysis.py`: End-to-end workflows

Run tests:
```bash
# All MPI-related tests
pytest tests/unit/test_mpi_*.py tests/unit/test_trace_distributor.py tests/unit/test_event_data_fetcher.py -v

# Integration tests
pytest tests/integration/test_mpi_trace_analysis.py -v
```

## Performance Considerations

### Load Balancing

- Round-robin distribution ensures even load
- Imbalance possible if traces have very different sizes
- Can be addressed with custom distribution strategies

### Communication Overhead

- EventDataFetcher caches remote data
- Minimize inter-process data dependencies when possible
- Consider trace layout for locality

### Scalability

- Tested with up to 8 replay processes
- Should scale to larger numbers with proper trace organization
- Communication overhead increases with inter-process dependencies

## Limitations and Future Work

### Current Limitations

1. Event data fetch uses blocking communication
2. No automatic load rebalancing
3. Assumes traces fit in memory

### Future Enhancements

1. Non-blocking/asynchronous communication
2. Dynamic load balancing
3. Out-of-core trace processing
4. Support for collective operations in analysis

## API Reference

### MPIConfig

```python
class MPIConfig:
    @classmethod
    def get_instance() -> 'MPIConfig'
    
    def enable_mpi() -> bool
    def disable_mpi() -> None
    def is_enabled() -> bool
    def is_mpi_available() -> bool
    
    def get_rank() -> Optional[int]
    def get_size() -> Optional[int]
    def get_comm() -> Optional[any]
    def is_root() -> bool
    def barrier() -> None
```

### TraceDistributor

```python
class TraceDistributor:
    def __init__(num_execution_processes, num_replay_processes)
    
    def compute_distribution() -> Dict[int, int]
    def distribute_traces(traces: List[Trace]) -> List[Trace]
    def get_eps_for_rp(rp_id: int) -> List[int]
    def get_load_balance_info() -> Dict[int, int]
```

### EventDataFetcher

```python
class EventDataFetcher:
    def register_traces(traces: List[Trace], ep_to_rp_mapping: Dict[int, int])
    
    def fetch_event_data(ep_id: int, event_idx: int) -> Optional[Dict[str, Any]]
    def is_local_event(ep_id: int) -> bool
    def clear_cache() -> None
```

### MPITraceReplayer

```python
class MPITraceReplayer(TraceReplayer):
    def __init__(trace=None, enable_mpi=False)
    
    def enable_mpi() -> bool
    def disable_mpi() -> None
    def is_mpi_enabled() -> bool
    
    def distribute_traces(traces, num_execution_processes, num_replay_processes) -> List[Trace]
    def get_event_data(ep_id: int, event_idx: int)
    
    def forwardReplay() -> None
    def backwardReplay() -> None
    
    def barrier() -> None
    def get_rank() -> Optional[int]
    def get_size() -> Optional[int]
    def is_root() -> bool
```

## Contributing

When adding new MPI-based analysis tasks:

1. Extend `MPITraceReplayer` or use it as a component
2. Use `get_event_data()` for inter-process data access
3. Add appropriate synchronization (barriers) where needed
4. Test in both sequential and parallel modes
5. Document MPI-specific requirements

## References

- MPI Standard: https://www.mpi-forum.org/
- mpi4py Documentation: https://mpi4py.readthedocs.io/
- PerFlow Contributing Guide: [docs/contributing.md](contributing.md)
