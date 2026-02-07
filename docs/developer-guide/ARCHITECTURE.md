# PerFlow Architecture Overview

This document provides a high-level overview of PerFlow's architecture, design principles, and key components.

## Design Philosophy

PerFlow is designed with three core principles:

1. **Ultra-Low Overhead** - Profiling should not significantly impact application performance
2. **Production-Ready** - Safe for use in production HPC environments
3. **Flexible Analysis** - Support both online and offline analysis workflows

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     User's MPI Application                       │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                      Application Code                      │  │
│  └────────────────────────────┬──────────────────────────────┘  │
│                               │ MPI_Init() intercepted          │
│  ┌────────────────────────────▼──────────────────────────────┐  │
│  │            PerFlow MPI Sampler (LD_PRELOAD)              │  │
│  │  ┌──────────────────┐      ┌──────────────────────────┐  │  │
│  │  │  PMU Sampler     │  OR  │  Timer Sampler           │  │  │
│  │  │  (Hardware PMU)  │      │  (POSIX timers)          │  │  │
│  │  └────────┬─────────┘      └────────┬─────────────────┘  │  │
│  │           │                          │                     │  │
│  │           └──────────┬───────────────┘                     │  │
│  │                      │                                     │  │
│  │         ┌────────────▼────────────┐                       │  │
│  │         │   CallStack Capture     │                       │  │
│  │         │   (libunwind)           │                       │  │
│  │         └────────────┬────────────┘                       │  │
│  │                      │                                     │  │
│  │         ┌────────────▼────────────┐                       │  │
│  │         │   Sample Aggregation    │                       │  │
│  │         │   (StaticHashMap)       │                       │  │
│  │         └────────────┬────────────┘                       │  │
│  │                      │                                     │  │
│  │         ┌────────────▼────────────┐                       │  │
│  │         │   Data Export (.pflw)   │                       │  │
│  │         └─────────────────────────┘                       │  │
│  └───────────────────────────────────────────────────────────┘  │
└──────────────────────────────┬──────────────────────────────────┘
                               │
                               │ Sample files written to disk
                               │
┌──────────────────────────────▼──────────────────────────────────┐
│                    PerFlow Analysis Module                       │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   Data Import & Processing                 │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐ │  │
│  │  │ DataImporter │  │ LibraryMap   │  │ SymbolResolver  │ │  │
│  │  └──────┬───────┘  └──────┬───────┘  └────────┬────────┘ │  │
│  │         │                  │                    │          │  │
│  │         └──────────────────┼────────────────────┘          │  │
│  │                            │                               │  │
│  │              ┌─────────────▼─────────────┐                │  │
│  │              │   OffsetConverter         │                │  │
│  │              │   (addr → lib + offset)   │                │  │
│  │              └─────────────┬─────────────┘                │  │
│  └────────────────────────────┼──────────────────────────────┘  │
│                                │                                 │
│  ┌────────────────────────────▼──────────────────────────────┐  │
│  │              Performance Tree Builder                      │  │
│  │  ┌──────────────────────────────────────────────────────┐ │  │
│  │  │  TreeNode (function, library, source location)       │ │  │
│  │  │  - Per-process sample counts                         │ │  │
│  │  │  - Edge weights (call relationships)                 │ │  │
│  │  │  - Hierarchical parent-child structure               │ │  │
│  │  └──────────────────────────────────────────────────────┘ │  │
│  └────────────────────────────┬──────────────────────────────┘  │
│                                │                                 │
│  ┌────────────────────────────▼──────────────────────────────┐  │
│  │                    Analysis & Visualization                 │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐ │  │
│  │  │  Balance     │  │   Hotspot    │  │   Tree          │ │  │
│  │  │  Analyzer    │  │   Analyzer   │  │   Visualizer    │ │  │
│  │  └──────────────┘  └──────────────┘  └─────────────────┘ │  │
│  └─────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Sampling Module

The sampling module is responsible for collecting performance data with minimal overhead.

#### Hardware PMU Sampler (`libperflow_mpi_sampler.so`)
- **Mechanism**: Uses PAPI library to configure hardware Performance Monitoring Units
- **Trigger**: PMU overflow generates signal (SIGIO), triggering sample collection
- **Overhead**: <0.5% @ 1kHz sampling
- **Requirements**: PAPI library, PMU access permissions

**Key Features**:
- Cycle-accurate sampling based on hardware events
- Minimal interrupt latency
- Supports multiple PMU events (cycles, instructions, cache misses, etc.)

#### Timer-Based Sampler (`libperflow_mpi_sampler_timer.so`)
- **Mechanism**: POSIX timers (timer_create) with SIGPROF signal
- **Trigger**: Timer expiration generates signal, triggering sample collection
- **Overhead**: ~1-2% @ 1kHz sampling
- **Requirements**: POSIX timers (available on all modern Unix systems)

**Key Features**:
- Platform-independent (works in containers, VMs)
- No special permissions required
- Configurable frequency (100Hz - 10kHz)

#### Call Stack Capture
- **Library**: libunwind
- **Process**: On each sample, capture up to 128 stack frames
- **Performance**: ~1-2 microseconds per capture
- **Safety**: Signal-safe implementation for use in signal handlers

### 2. Data Structures

#### CallStack
- **Type**: Fixed-size array template `CallStack<MaxDepth>`
- **Optimization**: Cache-aligned (64 bytes), lazy hash computation
- **Hash**: FNV-1a algorithm for efficient hashmap lookups

#### StaticHashMap
- **Type**: Pre-allocated hash map with atomic operations
- **Collision**: Open addressing with linear probing
- **Capacity**: Compile-time fixed (typically 10,000+ entries)
- **Thread-Safety**: Atomic operations for concurrent access

**Why Static?**
- No dynamic allocation in signal handlers (avoids deadlocks)
- Predictable memory usage
- Cache-friendly layout

### 3. Data Serialization

#### File Format (.pflw)
```
Header (64 bytes):
  - Magic: "PFLW" (4 bytes)
  - Version: uint32_t (4 bytes)
  - Entry count: uint64_t (8 bytes)
  - Timestamp: uint64_t (8 bytes)
  - Reserved: (40 bytes)

Entries (variable):
  For each unique call stack:
    - Stack depth: uint32_t (4 bytes)
    - Sample count: uint64_t (8 bytes)
    - Frame addresses: uint64_t × depth
```

**Features**:
- Optional zlib compression (~70% size reduction)
- Version compatibility checking
- Integrity validation on import

### 4. Analysis Module

#### Performance Tree

**Tree Building**:
```
Sample data → TreeBuilder → PerformanceTree
                              ├─ Root (virtual)
                              ├─ Node (function A)
                              │  ├─ sample counts per process
                              │  ├─ inclusive/exclusive time
                              │  └─ children (callees)
                              └─ Node (function B)
                                 └─ ...
```

**Two Build Modes**:

1. **Context-Free** (default):
   - Merges all instances of the same function
   - Compact representation
   - Better for high-level overview

2. **Context-Aware**:
   - Distinguishes by calling context (call site offset)
   - Detailed call graph
   - Better for precise call path analysis

#### Symbol Resolution

**Resolution Pipeline**:
```
Raw address → LibraryMap → (library, offset) → SymbolResolver → (function, file, line)
```

**Strategies**:
1. **dladdr**: Fast, only exported symbols (~1-2 μs)
2. **addr2line**: Comprehensive, all symbols (~10-50 μs)
3. **Auto-fallback**: Try dladdr first, fallback to addr2line

**Caching**: In-memory LRU cache with >90% hit rate

#### Analysis Algorithms

**Balance Analysis**:
```
For each tree node:
  max_samples = max(samples across all processes)
  avg_samples = mean(samples across all processes)
  imbalance = (max_samples - avg_samples) / avg_samples
```

**Hotspot Analysis**:
```
For each node:
  inclusive_time = own samples + all descendants' samples
  exclusive_time = own samples only
  percentage = (inclusive_time / total_samples) × 100%

Sort by inclusive_time, return top N
```

### 5. Visualization

**Tree Visualizer**:
- **Format**: DOT language (GraphViz)
- **Output**: PDF, PNG, SVG
- **Color Coding**: 
  - Hot colors (red) for high sample counts
  - Cool colors (blue) for low sample counts
- **Node Info**: Function name, sample counts, percentages

## Data Flow

### 1. Profiling Phase

```
Application starts
    ↓
MPI_Init() intercepted by PerFlow
    ↓
Sampler initialized (PMU or Timer)
    ↓
Sampling begins at configured frequency
    ↓
For each sample:
    ├─ Capture call stack (libunwind)
    ├─ Hash call stack (FNV-1a)
    ├─ Lookup/insert in StaticHashMap
    └─ Increment sample count
    ↓
Application ends
    ↓
MPI_Finalize() triggers data export
    ↓
Sample data written to rank_N.pflw files
```

### 2. Analysis Phase

```
Load sample files (.pflw)
    ↓
Load library maps (optional, for symbol resolution)
    ↓
Convert addresses to (library, offset) pairs
    ↓
Resolve symbols (optional)
    ├─ dladdr for exported symbols
    └─ addr2line for comprehensive resolution
    ↓
Build performance tree
    ├─ Aggregate call stacks
    ├─ Compute per-process statistics
    └─ Build hierarchical structure
    ↓
Perform analyses
    ├─ Balance analysis
    ├─ Hotspot detection
    └─ Custom queries
    ↓
Generate outputs
    ├─ Text reports
    ├─ Tree serialization
    └─ Visualizations (PDF)
```

## Memory Management

### Sampling Phase (Signal Handlers)
- **No dynamic allocation** - All memory pre-allocated
- **Fixed-size structures** - CallStack, StaticHashMap
- **Signal-safe operations** - All functions are async-signal-safe

### Analysis Phase
- **Dynamic allocation allowed** - Not in signal context
- **Symbol caching** - Trade memory for speed
- **Tree storage** - Dynamic growth as needed

## Thread Safety

### Sampling Module
- **Per-thread sampling** - Each thread has own sampler state
- **Atomic operations** - StaticHashMap uses atomics for insertions
- **Signal masking** - Prevents nested signal handlers

### Analysis Module
- **Thread-safe read** - Multiple threads can read same tree
- **Synchronized write** - Tree building is single-threaded or uses locks

## Performance Characteristics

### Sampling Overhead

| Component | Time (μs) | Notes |
|-----------|-----------|-------|
| Signal delivery | 0.5-1.0 | Hardware/OS overhead |
| Call stack capture | 1-2 | libunwind unwind |
| Hash computation | 0.1-0.2 | FNV-1a on 64 frames |
| HashMap lookup | 0.1-0.3 | Single probe expected |
| **Total per sample** | **2-4** | **<0.5% @ 1kHz** |

### Analysis Performance

| Operation | Time | Notes |
|-----------|------|-------|
| Load .pflw file | ~10 MB/s | I/O bound |
| Symbol resolution (dladdr) | 1-2 μs | Per symbol, first lookup |
| Symbol resolution (addr2line) | 10-50 μs | Per symbol, first lookup |
| Symbol resolution (cached) | 0.1 μs | Cache hit |
| Tree building | ~100k samples/s | Process and aggregate |
| Balance analysis | ~1 ms | Per 1000 nodes |
| Hotspot analysis | ~1 ms | Per 1000 nodes, with sorting |

### Memory Usage

| Component | Size | Notes |
|-----------|------|-------|
| CallStack<128> | ~1 KB | 128 × 8 bytes + metadata |
| StaticHashMap entry | ~1 KB | Key + value + control |
| Sampler per rank | ~10 MB | Typical for 10k unique stacks |
| Symbol cache | ~100 bytes | Per cached symbol |
| Tree node | ~200 bytes | Base + per-process data |

## Extension Points

### Adding New Samplers
1. Implement signal-based sample trigger
2. Call `CallStack::capture()` in signal handler
3. Store samples in `StaticHashMap`
4. Export via `DataExporter`

### Custom Analysis
```cpp
// Access tree structure
analysis.tree().root().for_each_child([](TreeNode& node) {
    // Custom logic for each node
});

// Query sample data
auto samples = analysis.tree().root().samples();
```

### New Visualization Formats
```cpp
// Inherit from base visualizer
class CustomVisualizer : public TreeVisualizer {
    void export_format(const std::string& path) override {
        // Custom export logic
    }
};
```

## Design Trade-offs

### Static vs. Dynamic Allocation
- **Choice**: Static allocation in sampling, dynamic in analysis
- **Benefit**: Signal-safe sampling, flexible analysis
- **Cost**: Fixed capacity during sampling

### Context-Free vs. Context-Aware Trees
- **Choice**: Support both modes
- **Benefit**: Compact view or detailed view as needed
- **Cost**: Implementation complexity

### Symbol Resolution Timing
- **Choice**: Post-process resolution (not during sampling)
- **Benefit**: Minimal runtime overhead
- **Cost**: Requires debug info at analysis time

### Sampling Frequency
- **Choice**: User-configurable (100Hz-10kHz)
- **Benefit**: Balance between detail and overhead
- **Cost**: Must choose appropriate frequency

## Security Considerations

- **No sensitive data in samples** - Only addresses and counts
- **No code injection** - LD_PRELOAD is explicit, controlled by user
- **Memory safety** - Bounds checking, no buffer overflows
- **Signal safety** - Proper signal masking, no locks in handlers

## Future Directions

Potential areas for enhancement:
- GPU profiling support
- Network communication analysis
- Memory allocation tracking
- Cache behavior analysis
- Real-time monitoring dashboard
- Cloud-native integration

## See Also

- [Sampling Framework Details](../sampling_framework.md)
- [Symbol Resolution Guide](../SYMBOL_RESOLUTION.md)
- [Online Analysis API](../ONLINE_ANALYSIS_API.md)
- [Timer Sampler Documentation](../TIMER_BASED_SAMPLER.md)
