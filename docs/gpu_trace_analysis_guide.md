# GPU-Accelerated Trace Analysis Guide

This guide explains the GPU-accelerated trace analysis feature in PerFlow, covering architecture, usage, and performance optimization.

## Overview

PerFlow's GPU trace analysis module provides CUDA-accelerated implementations of trace analysis algorithms, enabling high-performance parallel processing of large-scale execution traces. The implementation includes:

- GPU-friendly data structures (Structure of Arrays)
- CUDA kernels for parallel trace replay
- Python bindings with automatic CPU fallback
- Optimizations using shared memory and parallel reduction

## Architecture

### Design Principles

1. **GPU-Friendly Data Structures**: Convert object-oriented Python structures to flat arrays (SoA) for efficient GPU processing
2. **Parallel Computation**: Each thread processes one event independently
3. **Callback Registration**: Analysis tasks registered as callbacks, similar to CPU implementation
4. **Automatic Fallback**: Seamlessly use CPU when GPU unavailable
5. **Memory Efficiency**: Minimize data transfer between CPU and GPU

### Component Structure

```
perflow/task/trace_analysis/gpu/
├── __init__.py                  # Module exports
├── event_array.h                # C header for GPU data structures
├── trace_replay_kernel.cu       # CUDA kernels implementation
├── gpu_trace_replayer.py        # Base GPU replayer class
├── gpu_late_sender.py          # GPU late sender analyzer
├── gpu_late_receiver.py        # GPU late receiver analyzer
├── CMakeLists.txt              # Build configuration
├── build_gpu.sh                # Build script
└── README.md                   # Module documentation
```

## Data Structure Conversion

### From Python Objects to GPU Arrays

**Python Event Object:**
```python
class Event:
    m_type: EventType
    m_idx: int
    m_pid: int
    m_tid: int
    m_timestamp: float
    m_replay_pid: int
```

**GPU Structure of Arrays:**
```c
typedef struct {
    int32_t *types;        // Array of all event types
    int32_t *indices;      // Array of all indices
    int32_t *pids;         // Array of all PIDs
    int32_t *tids;         // Array of all TIDs
    double *timestamps;    // Array of all timestamps
    int32_t *replay_pids;  // Array of all replay PIDs
    int32_t num_events;    // Total count
} EventArray;
```

### Benefits of SoA Format

1. **Coalesced Memory Access**: Adjacent threads access adjacent memory locations
2. **Better Cache Utilization**: Each array stored contiguously in memory
3. **SIMD Efficiency**: Single instruction can process multiple data elements
4. **Reduced Padding**: No struct alignment overhead

## CUDA Kernel Design

### Late Sender Detection Kernel

```cuda
__global__ void detect_late_senders_kernel(
    const int32_t *base_types,
    const double *base_timestamps,
    const int32_t *mpi_partner_indices,
    int32_t num_events,
    int32_t *late_send_indices,
    double *wait_times,
    int32_t *num_late_sends)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx >= num_events) return;
    
    // Check if this is a receive event
    if (base_types[idx] == EVENT_TYPE_RECV) {
        int32_t send_idx = mpi_partner_indices[idx];
        
        // Check if send arrived late
        if (send_idx >= 0) {
            double recv_time = base_timestamps[idx];
            double send_time = base_timestamps[send_idx];
            
            if (send_time > recv_time) {
                double wait_time = send_time - recv_time;
                
                // Atomically record result
                int result_idx = atomicAdd(num_late_sends, 1);
                late_send_indices[result_idx] = send_idx;
                wait_times[result_idx] = wait_time;
            }
        }
    }
}
```

### Key Design Decisions

1. **One Thread Per Event**: Simple parallel pattern, good for GPU
2. **Atomic Operations**: Safe result accumulation across threads
3. **Bounds Checking**: Prevent out-of-bounds memory access
4. **No Divergence**: Minimize branch divergence within warps

## Optimizations

### 1. Shared Memory

Cache frequently accessed data in on-chip shared memory:

```cuda
__global__ void detect_late_senders_shared_kernel(...)
{
    extern __shared__ char shared_mem[];
    int32_t *shared_types = (int32_t *)shared_mem;
    double *shared_timestamps = (double *)(shared_mem + ...);
    
    int local_idx = threadIdx.x;
    
    // Load data into shared memory
    if (idx < num_events) {
        shared_types[local_idx] = base_types[idx];
        shared_timestamps[local_idx] = base_timestamps[idx];
    }
    
    __syncthreads();
    
    // Access from shared memory (much faster)
    if (shared_types[local_idx] == EVENT_TYPE_RECV) {
        // ...
    }
}
```

**Benefits:**
- ~100x faster than global memory
- Reduces memory bandwidth usage
- Better for large traces with many accesses

### 2. Parallel Reduction

Efficiently compute total wait time using tree-based reduction:

```cuda
__global__ void reduce_sum_kernel(
    const double *wait_times,
    int32_t num_items,
    double *total_wait)
{
    extern __shared__ double shared_data[];
    
    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Load data
    shared_data[tid] = (idx < num_items) ? wait_times[idx] : 0.0;
    __syncthreads();
    
    // Tree reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            shared_data[tid] += shared_data[tid + stride];
        }
        __syncthreads();
    }
    
    // First thread writes block result
    if (tid == 0) {
        atomicAdd(total_wait, shared_data[0]);
    }
}
```

**Complexity:**
- Sequential: O(n)
- Parallel: O(log n) with n/2 threads

### 3. Memory Coalescing

Ensure threads access consecutive memory locations:

```c
// Good: Coalesced access
timestamps[idx] = value;  // Thread idx accesses element idx

// Bad: Strided access
timestamps[idx * stride] = value;  // Threads access non-consecutive elements
```

### 4. Grid Configuration

Optimize block size and grid size:

```python
threads_per_block = 256  # Good for most GPUs
num_blocks = (num_events + threads_per_block - 1) // threads_per_block
kernel<<<num_blocks, threads_per_block>>>(...);
```

**Guidelines:**
- Block size: 128-256 threads (multiple of 32 for warp efficiency)
- Multiple blocks to saturate GPU
- Avoid very small or very large blocks

## Usage Examples

### Basic Usage

```python
from perflow.task.trace_analysis.gpu import GPULateSender

# Create analyzer
analyzer = GPULateSender(trace, use_gpu=True)

# Run analysis
analyzer.forwardReplay()

# Get results
late_sends = analyzer.getLateSends()
print(f"Found {len(late_sends)} late sends")
```

### With Optimizations

```python
# Enable shared memory optimization
analyzer.enableSharedMemory(True)

# Run analysis
analyzer.forwardReplay()
```

### Check GPU Status

```python
from perflow.task.trace_analysis.gpu import GPUAvailable

if GPUAvailable:
    print("Using GPU acceleration")
    analyzer = GPULateSender(trace, use_gpu=True)
else:
    print("GPU not available, using CPU")
    from perflow.task.trace_analysis.late_sender import LateSender
    analyzer = LateSender(trace)
```

## Performance Analysis

### Expected Speedup

| Trace Size | CPU Time | GPU Time | Speedup | Notes |
|------------|----------|----------|---------|-------|
| 1K         | 2 ms     | 1 ms     | 2x      | GPU overhead dominates |
| 10K        | 20 ms    | 3 ms     | 6.7x    | GPU starts to shine |
| 100K       | 200 ms   | 15 ms    | 13.3x   | Good parallelism |
| 1M         | 2 s      | 80 ms    | 25x     | Excellent speedup |
| 10M        | 20 s     | 500 ms   | 40x     | Near-optimal |

### Performance Factors

**GPU Benefits Most When:**
- Large number of events (>10K)
- Regular computation patterns
- High arithmetic intensity
- Multiple traces processed in batch

**CPU May Be Faster When:**
- Very small traces (<1K events)
- Irregular patterns with lots of branching
- Complex callback logic
- Memory-bound operations

### Profiling

Use NVIDIA tools to profile:

```bash
# Profile with nvprof
nvprof python example_gpu_late_sender_analysis.py

# Visual profiling with Nsight
nsys profile -o profile.qdrep python example_gpu_late_sender_analysis.py
```

## Building and Installation

### Prerequisites

```bash
# Check CUDA installation
nvcc --version

# Verify GPU
nvidia-smi
```

### Build GPU Library

```bash
cd perflow/task/trace_analysis/gpu

# Build with default settings
./build_gpu.sh

# Clean build
./build_gpu.sh --clean

# Specify architecture
./build_gpu.sh --arch=sm_75
```

### Verify Installation

```bash
# Check if library was built
ls libtrace_replay_gpu.so

# Test in Python
python -c "from perflow.task.trace_analysis.gpu import GPUAvailable; print(f'GPU: {GPUAvailable}')"
```

## Extending GPU Analysis

### Adding New Analysis Algorithms

1. **Define CUDA Kernel** in `trace_replay_kernel.cu`:

```cuda
__global__ void my_analysis_kernel(
    const int32_t *types,
    const double *timestamps,
    int32_t num_events,
    // Output parameters
    int32_t *results)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_events) return;
    
    // Your analysis logic here
}
```

2. **Create Python Class** in `gpu_my_analyzer.py`:

```python
from .gpu_trace_replayer import GPUTraceReplayer

class GPUMyAnalyzer(GPUTraceReplayer):
    def __init__(self, trace=None, use_gpu=True):
        super().__init__(trace, use_gpu)
        self.results = []
    
    def _analyze_gpu(self):
        # Call your kernel
        pass
    
    def _analyze_cpu(self):
        # CPU fallback
        pass
```

3. **Add Tests** in `test_gpu_trace_analysis.py`:

```python
def test_my_analyzer():
    trace = create_test_trace()
    analyzer = GPUMyAnalyzer(trace)
    analyzer.forwardReplay()
    assert len(analyzer.results) > 0
```

## Troubleshooting

### Common Issues

**1. Library Not Found**
```
Solution: Run ./build_gpu.sh to compile CUDA code
```

**2. CUDA Out of Memory**
```
Solution: 
- Process traces in smaller batches
- Use GPU with more memory
- Fall back to CPU for very large traces
```

**3. Slow Performance**
```
Checklist:
- Enable shared memory optimization
- Check grid configuration (threads_per_block)
- Profile with nvprof to identify bottlenecks
- Verify memory coalescing
```

**4. Incorrect Results**
```
Debug steps:
- Run CPU/GPU consistency tests
- Check atomic operation usage
- Verify bounds checking
- Add printf debugging in kernel
```

## Best Practices

### Do's ✓

- Use SoA format for data structures
- Enable shared memory for better performance
- Profile before optimizing
- Test CPU/GPU consistency
- Handle GPU unavailable gracefully

### Don'ts ✗

- Don't use AoS (Array of Structures) format
- Don't ignore atomic operation costs
- Don't assume GPU is always faster
- Don't forget bounds checking
- Don't skip CPU fallback implementation

## References

- [CUDA C++ Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)
- [CUDA Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)
- [Parallel Reduction Patterns](https://developer.download.nvidia.com/assets/cuda/files/reduction.pdf)
- [Memory Coalescing](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html#coalesced-access-to-global-memory)

## Next Steps

1. Try the examples: `python tests/examples/example_gpu_late_sender_analysis.py`
2. Run the tests: `pytest tests/unit/test_gpu_trace_analysis.py`
3. Profile your workload: Use nvprof or Nsight
4. Extend with new algorithms: Follow the guide above
5. Share feedback: Report issues or suggest improvements
