# Call Stack Address to Offset Conversion

This document describes the call stack address offset conversion feature implemented in PerFlow.

## Overview

When capturing call stacks at runtime, addresses are stored as absolute runtime addresses (library base + offset). These addresses vary between process runs because libraries are loaded at different base addresses. To enable accurate symbol mapping and comparison across runs, we need to convert these to static offsets.

## Design

The implementation follows a **post-processing approach** to minimize runtime overhead:

1. **Capture Phase (Runtime)**: Store raw addresses with minimal metadata
2. **Analysis Phase (Offline)**: Convert addresses to (library, offset) pairs

This approach ensures < 0.5% runtime overhead as specified in the requirements.

## Components

### 1. LibraryMap (`include/sampling/library_map.h`)

Parses `/proc/self/maps` to capture information about loaded libraries.

**Key Features:**
- Parses executable memory regions from `/proc/self/maps`
- Resolves runtime addresses to (library_name, offset) pairs
- Handles at least 1000 loaded libraries efficiently
- Filters non-executable regions to reduce memory usage

**Example Usage:**
```cpp
#include "sampling/library_map.h"

using namespace perflow::sampling;

LibraryMap map;
if (map.parse_current_process()) {
    // Resolve an address
    uintptr_t addr = 0x7f8a4c010000;
    auto result = map.resolve(addr);
    if (result.has_value()) {
        std::cout << "Library: " << result->first << std::endl;
        std::cout << "Offset: 0x" << std::hex << result->second << std::endl;
    }
}
```

### 2. Extended Call Stack Structures (`include/sampling/call_stack.h`)

**RawCallStack**: Stores raw addresses with metadata
```cpp
template <size_t MaxDepth = kDefaultMaxStackDepth>
struct RawCallStack {
    CallStack<MaxDepth> addresses;  // Raw runtime addresses
    int64_t timestamp;              // Capture time (nanoseconds)
    uint32_t map_id;                // Library map snapshot ID
};
```

**ResolvedCallStack**: Result of address resolution
```cpp
struct ResolvedCallStack {
    std::vector<ResolvedFrame> frames;  // Resolved frames
    int64_t timestamp;                  // Original timestamp
    uint32_t map_id;                    // Map snapshot used
};

struct ResolvedFrame {
    std::string library_name;  // Library/binary name
    uintptr_t offset;          // Offset within library
    uintptr_t raw_address;     // Original raw address
};
```

### 3. OffsetConverter (`include/analysis/offset_converter.h`)

Post-processing converter that resolves raw addresses to offsets.

**Key Features:**
- Stores multiple library map snapshots (for dynamic loading scenarios)
- Converts single or batch call stacks
- Handles unknown addresses gracefully (marks as "[unknown]")

**Example Usage:**
```cpp
#include "analysis/offset_converter.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

// Create converter and add map snapshots
OffsetConverter converter;
LibraryMap map;
map.parse_current_process();
converter.add_map_snapshot(0, map);

// Convert a raw call stack
RawCallStack<> raw(stack, timestamp, 0);
ResolvedCallStack resolved = converter.convert(raw);

// Access resolved frames
for (const auto& frame : resolved.frames) {
    std::cout << frame.library_name << " + 0x" 
              << std::hex << frame.offset << std::endl;
}
```

## Integration Points

### During Sampler Initialization
```cpp
// Capture initial library map
LibraryMap initial_map;
initial_map.parse_current_process();
converter.add_map_snapshot(0, initial_map);
```

### During Sampling (Runtime)
```cpp
// Capture call stack (fast, no resolution)
CallStack<> stack = capture_call_stack();
int64_t timestamp = get_timestamp_ns();
uint32_t map_id = 0;  // Use appropriate snapshot ID

RawCallStack<> raw(stack, timestamp, map_id);
// Store raw for later processing
raw_stacks.push_back(raw);
```

### During Analysis (Post-Processing)
```cpp
// Convert all captured stacks
std::vector<ResolvedCallStack> resolved = 
    converter.convert_batch(raw_stacks);

// Process resolved stacks for symbol mapping, profiling, etc.
for (const auto& resolved_stack : resolved) {
    process_resolved_stack(resolved_stack);
}
```

## Handling Dynamic Library Loading

If libraries are loaded/unloaded during execution, capture new map snapshots:

```cpp
// After dlopen() or when detecting library changes
uint32_t new_map_id = next_map_id++;
LibraryMap new_map;
new_map.parse_current_process();
converter.add_map_snapshot(new_map_id, new_map);

// Use new map_id for subsequent samples
RawCallStack<> raw(stack, timestamp, new_map_id);
```

## Performance Characteristics

- **Runtime Overhead**: < 0.5% (only stores raw addresses + small metadata)
- **Library Capacity**: Handles 1000+ loaded libraries efficiently
- **Memory Usage**: O(n) where n is number of executable regions
- **Resolution Time**: O(m × n) for batch conversion where m is number of stacks and n is number of libraries (uses linear search)

## Testing

The implementation includes comprehensive unit tests:

- `test_library_map.cpp`: 10 tests for LibraryMap functionality
  - Parsing, resolution, special regions, large library counts
- `test_offset_converter.cpp`: 12 tests for OffsetConverter
  - Conversion, batch processing, multiple snapshots, error handling

All tests pass successfully:
```bash
cd build
./tests/perflow_tests --gtest_filter="LibraryMapTest.*"
./tests/perflow_tests --gtest_filter="OffsetConverterTest.*"
```

## Example

See `examples/offset_converter_example.cpp` for a complete working example demonstrating:
1. Capturing library maps
2. Storing raw call stacks
3. Post-processing to resolve offsets
4. Displaying results

## Success Criteria

✅ Convert addresses to correct (library_name, offset) pairs  
✅ Runtime overhead increase < 0.5% (post-processing approach)  
✅ Handle at least 1000 loaded libraries  
✅ Support library loading/unloading during execution (via multiple snapshots)

## Future Enhancements

Possible improvements for future versions:
- **Binary search optimization**: Replace linear search in `LibraryMap::resolve()` with binary search for O(log n) lookup
- Compressed storage format for map snapshots
- Incremental map updates (delta encoding)
- Integration with existing symbol resolvers (addr2line, etc.)
