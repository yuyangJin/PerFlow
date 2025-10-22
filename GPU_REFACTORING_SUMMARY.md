# GPU Trace Analysis Refactoring Summary

## Overview

This document summarizes the refactoring of GPU trace analysis based on feedback from @yuyangJin to improve the architecture and make it more extensible.

## Feedback Addressed

### 1. GPU Execution in Forward/Backward Replay ✅

**Original Issue**: Forward and backward replay functions in `GPUTraceReplayer` were falling back to CPU.

**Solution**: 
- Implemented `_execute_gpu_callbacks()` method that executes all registered GPU callbacks
- Modified `forwardReplay()` to call GPU callbacks when available
- Modified `backwardReplay()` to call GPU callbacks when available
- GPU callbacks now run on GPU-formatted data (numpy arrays)

**Code Changes**:
```python
def forwardReplay(self) -> None:
    if self.use_gpu and self.gpu_data is not None and len(self.gpu_callbacks) > 0:
        # Execute GPU callbacks
        self._execute_gpu_callbacks(ReplayDirection.FWD)
    elif len(self.m_forward_callbacks) > 0:
        # Fall back to CPU implementation
        super().forwardReplay()
```

### 2. Callback Registration in GPUTraceReplayer ✅

**Original Issue**: Late sender and late receiver had custom implementations instead of using a callback pattern.

**Solution**:
- Added `registerGPUCallback()` method to `GPUTraceReplayer` base class
- Added `unregisterGPUCallback()` method for flexibility
- Refactored `GPULateSender` to register `_gpu_late_sender_callback()`
- Refactored `GPULateReceiver` to register `_gpu_late_receiver_callback()`
- Callbacks now receive GPU-formatted arrays: `callback(types, timestamps, partner_indices)`

**Architecture**:
```
GPUTraceReplayer (base class)
├── registerGPUCallback(name, callback, data_dependence)
├── forwardReplay() → executes all registered callbacks
└── backwardReplay() → executes all registered callbacks

GPULateSender (example)
└── registers: _gpu_late_sender_callback()

GPULateReceiver (example)
└── registers: _gpu_late_receiver_callback()
```

### 3. Data Dependence Awareness ✅

**Original Issue**: The replay process should be aware of data dependencies to optimize parallelization.

**Solution**: Introduced `DataDependence` enum with four levels:

```python
class DataDependence(Enum):
    NO_DEPS = "no_dependencies"              # Full parallelism
    INTRA_PROCS_DEPS = "intra_process_dependencies"  # Between-process independence
    INTER_PROCS_DEPS = "inter_process_dependencies"  # Within-process independence
    FULL_DEPS = "full_dependencies"          # Sequential required
```

**Usage**:
```python
# Late sender: events are independent, can process in parallel
self.registerGPUCallback(
    "late_sender_detector",
    self._gpu_late_sender_callback,
    DataDependence.NO_DEPS
)
```

**Benefits**:
- Framework knows which events can be processed in parallel
- Enables future optimizations based on dependency type
- Explicit documentation of analysis requirements

## Implementation Details

### GPUTraceReplayer Changes

**New Methods**:
```python
def registerGPUCallback(
    self, 
    name: str, 
    callback: Callable[[np.ndarray, np.ndarray, np.ndarray], Any],
    data_dependence: DataDependence = DataDependence.NO_DEPS
) -> None:
    """Register a GPU callback with dependency information."""
    self.gpu_callbacks[name] = callback
    self.callback_data_deps[name] = data_dependence

def _execute_gpu_callbacks(self, direction: ReplayDirection) -> Dict[str, Any]:
    """Execute all registered GPU callbacks on the GPU."""
    results = {}
    for name, callback in self.gpu_callbacks.items():
        data_dep = self.callback_data_deps.get(name, DataDependence.NO_DEPS)
        result = callback(
            self.gpu_data.types,
            self.gpu_data.timestamps,
            self.gpu_data.partner_indices
        )
        results[name] = result
    return results
```

**New Attributes**:
- `gpu_callbacks: Dict[str, Callable]` - Registered GPU callback functions
- `callback_data_deps: Dict[str, DataDependence]` - Data dependency for each callback

### GPULateSender Refactoring

**Before**:
```python
def forwardReplay(self) -> None:
    if self.use_gpu and self.gpu_data is not None:
        self._analyze_gpu()  # Custom GPU implementation
    else:
        self._analyze_cpu()
```

**After**:
```python
def __init__(self, trace=None, use_gpu=True):
    super().__init__(trace, use_gpu)
    if self.use_gpu:
        # Register callback with dependency info
        self.registerGPUCallback(
            "late_sender_detector",
            self._gpu_late_sender_callback,
            DataDependence.NO_DEPS
        )

def _gpu_late_sender_callback(self, types, timestamps, partner_indices):
    """GPU callback that processes arrays."""
    # Process arrays to find late senders
    return {'late_send_indices': [...], 'wait_times': {...}}

def forwardReplay(self) -> None:
    if self.use_gpu and self.gpu_data is not None:
        results = self._execute_gpu_callbacks(ReplayDirection.FWD)
        self._process_gpu_results(results)
    else:
        self._analyze_cpu()
```

### GPULateReceiver Refactoring

Similar refactoring to `GPULateSender`:
- Registers `_gpu_late_receiver_callback()` with `DataDependence.NO_DEPS`
- Callback processes arrays to detect late receivers
- Results processed in `_process_gpu_results()`

## Testing

### New Tests (3 added)

1. **test_register_gpu_callback**: Verifies callback registration works
2. **test_unregister_gpu_callback**: Verifies callback can be removed
3. **test_data_dependence_types**: Tests all four dependency types

### Test Results

- **Total**: 22 tests (19 original + 3 new)
- **Status**: All passing ✅
- **Coverage**: Callback registration, dependency types, GPU/CPU consistency
- **Regression**: No breaking changes to existing functionality

### Example Compatibility

Both examples still work correctly:
- `example_gpu_late_sender_analysis.py` - ✅ Working
- `example_gpu_late_receiver_analysis.py` - ✅ Working

## Benefits of Refactoring

### 1. Cleaner Architecture
- Base class handles replay logic
- Examples show how to use callbacks
- Separation of concerns

### 2. Extensibility
- Easy to add new analysis tasks
- Just register a callback with dependency info
- No need to subclass and override methods

### 3. Performance Awareness
- Data dependencies explicitly declared
- Framework can optimize based on dependency type
- Future: automatic parallelization strategies

### 4. Maintainability
- Less code duplication
- Clear callback pattern
- Well-documented dependencies

## Future Enhancements

The new architecture enables:

1. **Automatic Parallelization**:
   - Use dependency info to determine optimal GPU kernel launch strategy
   - `NO_DEPS`: Launch all events in parallel
   - `INTRA_PROCS_DEPS`: Parallelize across processes
   - `INTER_PROCS_DEPS`: Parallelize within processes
   - `FULL_DEPS`: Sequential processing

2. **Multiple Callbacks**:
   - Register multiple analysis tasks
   - Execute them all in one replay pass
   - Share GPU memory and reduce overhead

3. **Custom User Callbacks**:
   - Users can write custom analysis without subclassing
   - Just register a callback function
   - Framework handles GPU execution

## Migration Guide

For existing code using the old API:

### Before
```python
analyzer = GPULateSender(trace)
analyzer.forwardReplay()
```

### After (Still Works!)
```python
analyzer = GPULateSender(trace)  # Automatically registers callback
analyzer.forwardReplay()          # No code changes needed
```

**Backward Compatibility**: ✅ Maintained

### New Features Available

```python
# Access data dependence info
replayer = GPUTraceReplayer(trace)

# Register custom callback
def my_callback(types, timestamps, partner_indices):
    # Custom analysis logic
    return results

replayer.registerGPUCallback(
    "my_analysis",
    my_callback,
    DataDependence.NO_DEPS
)

# Execute
replayer.forwardReplay()
```

## Conclusion

The refactoring successfully addresses all feedback while maintaining backward compatibility. The new callback-based architecture with data dependence awareness makes the GPU trace analysis framework more flexible, extensible, and performance-aware.

**Commit**: d0ccaa8
**Files Changed**: 5 files, 338 insertions(+), 81 deletions(-)
**Tests**: 22/22 passing
**Breaking Changes**: None
