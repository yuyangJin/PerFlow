# PerFlow Sampling Framework

This document describes the code structure and components of the PerFlow sampling framework, designed for PMU-based performance data collection on ARMv9 supercomputing systems.

## Overview

The sampling framework provides a high-performance, signal-safe infrastructure for collecting and analyzing performance data using hardware Performance Monitoring Units (PMU). All data structures are designed with zero dynamic allocation during sampling to avoid deadlocks in signal handlers.

## Directory Structure

```
PerFlow/
├── include/
│   └── sampling/
│       ├── sampling.h          # Main header (includes all components)
│       ├── call_stack.h        # Function call stack representation
│       ├── static_hash_map.h   # Pre-allocated hash map
│       ├── data_export.h       # Data serialization (import/export)
│       └── pmu_sampler.h       # PMU sampling interface
├── tests/
│   ├── CMakeLists.txt          # Test build configuration
│   ├── test_call_stack.cpp     # CallStack unit tests
│   ├── test_static_hash_map.cpp # StaticHashMap unit tests
│   ├── test_data_export.cpp    # Data export/import tests
│   └── test_pmu_sampler.cpp    # PMU sampler tests
├── cmake/
│   └── PerFlowConfig.cmake.in  # CMake package configuration
├── CMakeLists.txt              # Main build configuration
└── docs/
    ├── contributing.md         # Contribution guidelines
    └── sampling_framework.md   # This document
```

## Core Components

### 1. CallStack (`call_stack.h`)

A fixed-size call stack structure optimized for PMU sampling.

**Key Features:**
- Template class `CallStack<MaxDepth>` with configurable maximum depth (default: 128 frames)
- FNV-1a hash function with lazy caching for efficient lookups
- `alignas(64)` alignment for cache locality optimization
- Zero-copy comparison operations

**API:**
```cpp
template <size_t MaxDepth = kDefaultMaxStackDepth>
class CallStack {
public:
    CallStack();                                    // Default constructor
    CallStack(const AddressType* addresses, size_t count);  // From array
    
    size_t depth() const noexcept;                  // Current stack depth
    static constexpr size_t max_depth() noexcept;   // Maximum depth
    bool push(AddressType address) noexcept;        // Push frame
    AddressType pop() noexcept;                     // Pop frame
    AddressType frame(size_t index) const noexcept; // Access frame
    size_t hash() const noexcept;                   // Get hash value
    bool operator==(const CallStack& other) const;  // Equality comparison
};
```

### 2. StaticHashMap (`static_hash_map.h`)

A pre-allocated hash map designed for signal-safe operation.

**Key Features:**
- Template class `StaticHashMap<Key, Value, Capacity>`
- Open addressing with linear probing for collision resolution
- Atomic operations for thread-safety
- No dynamic memory allocation
- Automatic hash method detection using SFINAE

**API:**
```cpp
template <typename Key, typename Value, size_t Capacity = kDefaultMapCapacity>
class StaticHashMap {
public:
    StaticHashMap();
    
    size_type size() const noexcept;
    static constexpr size_type capacity() noexcept;
    bool empty() const noexcept;
    bool full() const noexcept;
    
    Value* find(const Key& key) noexcept;
    bool insert(const Key& key, const Value& value) noexcept;
    Value* insert_or_get(const Key& key) noexcept;
    bool increment(const Key& key) noexcept;
    bool erase(const Key& key) noexcept;
    void clear() noexcept;
    
    template <typename Callback>
    void for_each(Callback callback) noexcept;
};
```

### 3. DataExporter / DataImporter (`data_export.h`)

Binary serialization for performance data with versioning support.

**Key Features:**
- 64-byte header with magic number, version, and metadata
- Support for compressed (zlib) and uncompressed formats
- Integrity validation during import
- Customizable output directory and filename

**File Format:**
```
+------------------+
| Header (64 bytes)|
|  - Magic: PFLW   |
|  - Version       |
|  - Entry count   |
|  - Timestamp     |
+------------------+
| Entry 1          |
|  - Stack depth   |
|  - Count         |
|  - Frame addrs   |
+------------------+
| Entry 2          |
| ...              |
+------------------+
```

**API:**
```cpp
class DataExporter {
public:
    DataExporter(const char* directory, const char* filename, bool compressed = false);
    
    template <size_t MaxDepth, size_t Capacity>
    DataResult exportData(const StaticHashMap<CallStack<MaxDepth>, uint64_t, Capacity>& data);
    
    const char* filepath() const noexcept;
};

class DataImporter {
public:
    explicit DataImporter(const char* filepath);
    
    template <size_t MaxDepth, size_t Capacity>
    DataResult importData(StaticHashMap<CallStack<MaxDepth>, uint64_t, Capacity>& data);
    
    DataResult readHeader(DataFileHeader& header);
};
```

### 4. PmuSampler (`pmu_sampler.h`)

PMU-based sampling interface with overflow handling.

**Key Features:**
- Template class `PmuSampler<MaxStackDepth, MapCapacity>`
- Configurable sampling frequency and event types
- Overflow callback for sample collection
- Automatic call stack capture
- Flush-to-disk mechanism

**Supported PMU Events:**
- CPU cycles (`kCpuCycles`)
- Instructions retired (`kInstructions`)
- Cache misses (`kCacheMisses`)
- Branch mispredictions (`kBranchMisses`)
- L1/L2 cache access and misses
- Memory accesses

**API:**
```cpp
template <size_t MaxStackDepth = kDefaultMaxStackDepth,
          size_t MapCapacity = kDefaultMapCapacity>
class PmuSampler {
public:
    PmuSampler();
    ~PmuSampler();
    
    SamplerResult initialize(const PmuSamplerConfig& config);
    SamplerResult start();
    SamplerResult stop();
    void cleanup();
    
    DataResult flush();
    void handleOverflow();  // Called on PMU overflow
    
    SamplerStatus status() const noexcept;
    uint64_t sampleCount() const noexcept;
    const MapType& samples() const noexcept;
};
```

## Usage Example

```cpp
#include "sampling/sampling.h"

using namespace perflow::sampling;

int main() {
    // Create sampler with small capacity for testing
    PmuSampler<16, 1024> sampler;
    
    // Configure sampling
    PmuSamplerConfig config;
    config.frequency = 100;  // 100 samples/second
    config.primary_event = PmuEventType::kCpuCycles;
    std::strcpy(config.output_directory, "/tmp/perflow");
    std::strcpy(config.output_filename, "samples");
    
    // Initialize and start
    sampler.initialize(config);
    sampler.start();
    
    // ... application runs, samples collected via overflow callbacks ...
    
    // Stop and export
    sampler.stop();
    sampler.flush();
    
    return 0;
}
```

## Building

```bash
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j4
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `PERFLOW_BUILD_TESTS` | ON | Build unit tests |
| `PERFLOW_BUILD_EXAMPLES` | OFF | Build example programs |
| `PERFLOW_ENABLE_SANITIZERS` | OFF | Enable address/UB sanitizers |

## Testing

```bash
cd build
./tests/perflow_tests
```

The test suite includes 47 tests covering:
- CallStack operations and hashing
- StaticHashMap insert/find/erase operations
- Data export/import with integrity validation
- PmuSampler lifecycle and configuration

## Thread Safety

All components are designed for thread-safe operation:
- `StaticHashMap` uses atomic operations for state management
- `PmuSampler` is designed for signal handler invocation
- No locks are held during critical sampling paths

## Memory Management

The framework follows strict memory management rules:
- **No dynamic allocation during sampling** - All memory is pre-allocated
- **Static sizing** - Capacity is determined at compile time via templates
- **Signal-safe operations** - All operations in overflow handlers are async-signal-safe

## Extending the Framework

### Adding New PMU Events

1. Add event code to `PmuEventType` enum in `pmu_sampler.h`
2. Update `setupPerfEvent()` to handle the new event
3. Add corresponding tests

### Custom Analysis

Use the `for_each` iterator on `StaticHashMap` to process samples:

```cpp
sampler.samples().for_each([](const CallStack<16>& stack, const uint64_t& count) {
    // Process each unique call stack and its sample count
});
```

## Dependencies

- C++17 compatible compiler
- CMake 3.14+
- Google Test (fetched automatically)
- zlib (optional, for compression)

## License

Apache License 2.0
