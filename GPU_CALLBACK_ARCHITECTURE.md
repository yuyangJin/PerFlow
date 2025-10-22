# GPU Callback Architecture - Python-Side Event Processing

## Overview

This document describes the Python-side callback architecture for GPU-accelerated trace analysis in PerFlow. The architecture allows end users to write analysis callbacks in Python that process individual events during trace replay.

## Core Concept

**Callbacks are Python functions written by end users** that:
1. Receive the **current event** being processed
2. Contain **analysis logic** for that event
3. **Record results** (append to lists, update dictionaries, etc.)
4. Are **invoked for each event** during forward/backward replay

## Architecture

### Callback Function Signature

```python
def my_callback(event: Event) -> None:
    """
    Process a single event during trace replay.
    
    Args:
        event: The current event being processed
    
    Returns:
        None (results are recorded within the callback)
    """
    # Your analysis logic here
    if isinstance(event, MpiSendEvent):
        # Analyze send event
        # Record results
        pass
```

### Registration

```python
from perflow.task.trace_analysis.gpu import GPUTraceReplayer, DataDependence

replayer = GPUTraceReplayer(trace)

# Register callback with data dependence information
replayer.registerGPUCallback(
    name="my_analysis",
    callback=my_callback,
    data_dependence=DataDependence.NO_DEPS
)
```

### Execution Flow

```
User calls forwardReplay()
    ↓
GPUTraceReplayer iterates through events
    ↓
For each event:
    ↓
    Invoke all registered callbacks
        ↓
        Callback receives event
        ↓
        Callback executes analysis logic
        ↓
        Callback records results
    ↓
Continue to next event
```

## Complete Example: Late Sender Detection

### Step 1: Define Analyzer Class

```python
from perflow.task.trace_analysis.gpu import GPUTraceReplayer, DataDependence
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent

class GPULateSender(GPUTraceReplayer):
    """Detect late senders in MPI traces."""
    
    def __init__(self, trace=None, use_gpu=True):
        super().__init__(trace, use_gpu)
        
        # Storage for results
        self.m_late_sends = []
        self.m_wait_times = {}
        
        # Register callback
        self.registerGPUCallback(
            "late_sender_detector",
            self._late_sender_callback,
            DataDependence.NO_DEPS  # Events are independent
        )
```

### Step 2: Implement Callback

```python
    def _late_sender_callback(self, event):
        """
        Callback to detect late senders.
        
        Invoked for each event during replay. Checks if the event
        is a receive with a late sender.
        
        Args:
            event: Current event being processed
        """
        # Check if this is a receive event
        if isinstance(event, MpiRecvEvent):
            send_event = event.getSendEvent()
            
            # Check if send is matched and late
            if send_event and isinstance(send_event, MpiSendEvent):
                recv_time = event.getTimestamp()
                send_time = send_event.getTimestamp()
                
                if send_time > recv_time:
                    # Late sender detected!
                    wait_time = send_time - recv_time
                    
                    # Record results
                    self.m_late_sends.append(send_event)
                    self.m_wait_times[event.getIdx()] = wait_time
```

### Step 3: Use the Analyzer

```python
from perflow.perf_data_struct.dynamic.trace.trace import Trace

# Create trace
trace = Trace()
# ... add events to trace ...

# Create analyzer
analyzer = GPULateSender(trace, use_gpu=True)

# Run analysis (invokes callbacks)
analyzer.forwardReplay()

# Get results
late_sends = analyzer.m_late_sends
wait_times = analyzer.m_wait_times

print(f"Found {len(late_sends)} late senders")
for send_event in late_sends:
    print(f"  Late send from PID {send_event.getPid()} at time {send_event.getTimestamp()}")
```

## Forward vs. Backward Replay

### Forward Replay

Processes events in chronological order (oldest to newest):

```python
replayer.forwardReplay()

# Execution order:
# event[0] → callback(event[0])
# event[1] → callback(event[1])
# event[2] → callback(event[2])
# ...
```

### Backward Replay

Processes events in reverse chronological order (newest to oldest):

```python
replayer.backwardReplay()

# Execution order:
# event[N-1] → callback(event[N-1])
# event[N-2] → callback(event[N-2])
# event[N-3] → callback(event[N-3])
# ...
```

**Same callback function works for both directions!**

## Data Dependence

Data dependence information guides how events can be parallelized (future GPU optimization):

### NO_DEPS (No Dependencies)

All events can be processed in parallel. No event depends on another.

```python
registerGPUCallback("my_analysis", callback, DataDependence.NO_DEPS)
```

**Use case**: Late sender detection - each receive can be checked independently.

### INTRA_PROCS_DEPS (Intra-Process Dependencies)

Events from different processes can be processed in parallel, but events within the same process have dependencies.

```python
registerGPUCallback("my_analysis", callback, DataDependence.INTRA_PROCS_DEPS)
```

**Use case**: Per-process state tracking where event order within a process matters.

### INTER_PROCS_DEPS (Inter-Process Dependencies)

Events within a process can be processed in parallel, but events across processes have dependencies.

```python
registerGPUCallback("my_analysis", callback, DataDependence.INTER_PROCS_DEPS)
```

**Use case**: Global synchronization analysis where process order matters.

### FULL_DEPS (Full Dependencies)

All events must be processed sequentially. Every event depends on previous events.

```python
registerGPUCallback("my_analysis", callback, DataDependence.FULL_DEPS)
```

**Use case**: Building a global state that every event modifies sequentially.

## Multiple Callbacks

You can register multiple callbacks for different analysis tasks:

```python
class MyAnalyzer(GPUTraceReplayer):
    def __init__(self, trace=None, use_gpu=True):
        super().__init__(trace, use_gpu)
        
        # Storage for multiple analyses
        self.late_sends = []
        self.communication_patterns = {}
        self.execution_times = []
        
        # Register multiple callbacks
        self.registerGPUCallback(
            "late_sender",
            self._check_late_sender,
            DataDependence.NO_DEPS
        )
        
        self.registerGPUCallback(
            "comm_pattern",
            self._analyze_communication,
            DataDependence.NO_DEPS
        )
        
        self.registerGPUCallback(
            "execution_time",
            self._track_execution_time,
            DataDependence.INTRA_PROCS_DEPS
        )
    
    def _check_late_sender(self, event):
        # Late sender logic
        pass
    
    def _analyze_communication(self, event):
        # Communication pattern logic
        pass
    
    def _track_execution_time(self, event):
        # Execution time tracking logic
        pass
```

All callbacks are invoked for each event:

```
For each event:
    _check_late_sender(event)
    _analyze_communication(event)
    _track_execution_time(event)
```

## Accessing Event Information

Within the callback, you have full access to the event object:

```python
def my_callback(event):
    # Basic event information
    event_type = event.getType()        # EventType enum
    timestamp = event.getTimestamp()    # float
    pid = event.getPid()                # int
    tid = event.getTid()                # int
    idx = event.getIdx()                # int
    
    # MPI-specific information (if MPI event)
    if isinstance(event, MpiSendEvent):
        dest_pid = event.getDestPid()
        tag = event.getTag()
        comm = event.getCommunicator()
        recv_event = event.getRecvEvent()
    
    elif isinstance(event, MpiRecvEvent):
        src_pid = event.getSrcPid()
        tag = event.getTag()
        comm = event.getCommunicator()
        send_event = event.getSendEvent()
    
    # Use this information for analysis
    # ...
```

## Recording Results

### Pattern 1: Append to Lists

```python
def my_callback(self, event):
    if some_condition(event):
        self.results_list.append(event)
```

### Pattern 2: Update Dictionaries

```python
def my_callback(self, event):
    key = event.getPid()
    if key not in self.results_dict:
        self.results_dict[key] = []
    self.results_dict[key].append(event.getTimestamp())
```

### Pattern 3: Accumulate Values

```python
def my_callback(self, event):
    if isinstance(event, MpiSendEvent):
        self.total_sends += 1
        self.total_bytes += event.getMessageSize()
```

### Pattern 4: Track State

```python
def my_callback(self, event):
    if event.getType() == EventType.ENTER:
        self.call_stack.append(event)
    elif event.getType() == EventType.LEAVE:
        if self.call_stack:
            enter_event = self.call_stack.pop()
            duration = event.getTimestamp() - enter_event.getTimestamp()
            self.function_durations[enter_event.getName()] = duration
```

## CPU Fallback

When GPU is not available, the same callbacks work on CPU:

```python
class MyAnalyzer(GPUTraceReplayer):
    def __init__(self, trace=None, use_gpu=True):
        super().__init__(trace, use_gpu)
        
        if self.use_gpu:
            # GPU path: register GPU callback
            self.registerGPUCallback("my_analysis", self._my_callback, DataDependence.NO_DEPS)
        else:
            # CPU path: register CPU callback
            self.registerCallback("my_analysis", self._my_callback, ReplayDirection.FWD)
    
    def _my_callback(self, event):
        # Same callback works for both GPU and CPU!
        pass
```

**Note**: In the current implementation, callbacks execute on CPU even when GPU is enabled. Future optimization will move the iteration to GPU while keeping callbacks in Python.

## Best Practices

### 1. Keep Callbacks Simple

```python
# Good: Simple, focused logic
def my_callback(self, event):
    if isinstance(event, MpiSendEvent):
        self.send_count += 1

# Avoid: Complex nested logic
def my_callback(self, event):
    # Too much logic in one callback
    # Consider splitting into multiple callbacks
```

### 2. Use Type Checks

```python
def my_callback(self, event):
    # Check event type before accessing type-specific methods
    if isinstance(event, MpiSendEvent):
        dest = event.getDestPid()  # Safe
```

### 3. Handle None Values

```python
def my_callback(self, event):
    timestamp = event.getTimestamp()
    if timestamp is not None:
        self.timestamps.append(timestamp)
```

### 4. Initialize Storage in __init__

```python
class MyAnalyzer(GPUTraceReplayer):
    def __init__(self, trace=None, use_gpu=True):
        super().__init__(trace, use_gpu)
        
        # Initialize storage before registering callbacks
        self.results = []
        self.counters = {}
        
        self.registerGPUCallback("my_analysis", self._my_callback, DataDependence.NO_DEPS)
```

### 5. Clear Results Between Runs

```python
class MyAnalyzer(GPUTraceReplayer):
    def clear(self):
        """Clear results before new analysis."""
        self.results = []
        self.counters = {}
    
    def forwardReplay(self):
        self.clear()  # Clear before replay
        super().forwardReplay()
```

## Advanced: Custom Analysis Tasks

### Example: Critical Path Finding

```python
class GPUCriticalPath(GPUTraceReplayer):
    def __init__(self, trace=None, use_gpu=True):
        super().__init__(trace, use_gpu)
        
        # State tracking
        self.earliest_times = {}  # event_id -> earliest start time
        self.latest_times = {}    # event_id -> latest end time
        
        # Forward replay: compute earliest times
        self.registerGPUCallback(
            "compute_earliest",
            self._compute_earliest,
            DataDependence.FULL_DEPS  # Sequential required
        )
    
    def _compute_earliest(self, event):
        """Compute earliest time for this event."""
        event_id = event.getIdx()
        
        # Get dependencies
        dependencies = event.getDependencies()  # Hypothetical
        
        if not dependencies:
            # No dependencies: can start at own timestamp
            self.earliest_times[event_id] = event.getTimestamp()
        else:
            # Has dependencies: must wait for all to complete
            max_dep_time = max(
                self.earliest_times.get(dep_id, 0) 
                for dep_id in dependencies
            )
            self.earliest_times[event_id] = max(
                max_dep_time, 
                event.getTimestamp()
            )
    
    def backwardReplay(self):
        """Compute latest times in backward pass."""
        self.registerGPUCallback(
            "compute_latest",
            self._compute_latest,
            DataDependence.FULL_DEPS
        )
        super().backwardReplay()
    
    def _compute_latest(self, event):
        """Compute latest time for this event."""
        # Similar logic in reverse
        pass
    
    def get_critical_path(self):
        """Return events on critical path."""
        critical = []
        for event_id, earliest in self.earliest_times.items():
            latest = self.latest_times.get(event_id, float('inf'))
            if earliest == latest:  # No slack
                critical.append(event_id)
        return critical
```

## Future GPU Optimization

The current implementation executes callbacks on CPU by iterating through events sequentially. Future optimization will:

1. **Launch GPU kernel** that iterates through events in parallel
2. **Invoke callbacks** from GPU threads (one thread per event for NO_DEPS)
3. **Handle dependencies** based on DataDependence enum
4. **Keep callbacks in Python** but execute iteration on GPU

This architecture already supports this optimization through the DataDependence information.

## Conclusion

The Python-side callback architecture provides:

- **Ease of use**: Write callbacks in Python
- **Flexibility**: Full access to event objects
- **Extensibility**: Easy to add new analysis tasks
- **Debuggability**: Can use print, logging, debugging
- **Performance**: Ready for GPU parallelization

End users can focus on implementing analysis logic without worrying about low-level GPU programming or trace iteration mechanics.
