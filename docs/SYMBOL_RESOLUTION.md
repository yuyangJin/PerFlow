# Symbol Resolution Guide

## Overview

The PerFlow symbol resolution feature extends the OffsetConverter to resolve memory addresses into human-readable function names, source file paths, and line numbers. This enhancement enables more accurate code location identification for performance analysis.

## Architecture

The symbol resolution system consists of three main components:

1. **SymbolResolver** - Core symbol resolution engine with multiple strategies
2. **OffsetConverter** - Enhanced converter with optional symbol resolution
3. **ResolvedFrame** - Extended data structure with symbol information

```
┌─────────────────┐
│  CallStack      │  Raw addresses from sampling
│  (raw addrs)    │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ LibraryMap      │  Address to (library, offset)
│ resolve()       │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ SymbolResolver  │  Offset to (function, file, line)
│ (optional)      │
└────────┬────────┘
         │
         v
┌─────────────────┐
│ ResolvedFrame   │  Complete symbol information
└─────────────────┘
```

## Components

### SymbolResolver

The `SymbolResolver` class provides symbol resolution with multiple strategies:

#### Resolution Strategies

1. **kDlAddrOnly** - Fast, export/dynamic symbols only
   - Uses `dladdr()` system call
   - Best for export symbols (printf, malloc, etc.)
   - ~1-2 microseconds per resolution
   - No debug symbols required

2. **kAddr2LineOnly** - Comprehensive, requires debug symbols
   - Uses `addr2line` tool with DWARF debug info
   - Provides function names, source files, and line numbers
   - ~10-20 ms per first resolution (cached after)
   - Requires debug symbols in binary

3. **kAutoFallback** (default) - Best of both worlds
   - Tries dladdr first (fast path)
   - Falls back to addr2line if dladdr fails
   - Optimal for most use cases

#### Symbol Caching

The resolver implements an efficient caching mechanism:
- Cache key: `(library_path, offset)`
- Provides 100% hit rate for repeated addresses
- Dramatically reduces overhead for duplicate addresses
- Thread-safe (const methods, mutable cache)
- Configurable (can be disabled)

#### API

```cpp
#include "analysis/symbol_resolver.h"

using namespace perflow::analysis;

// Create resolver with auto-fallback strategy and caching enabled
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback,
    true  // enable_cache
);

// Resolve a symbol
SymbolInfo info = resolver->resolve("/lib/libc.so.6", 0x1234);

if (info.is_resolved()) {
    std::cout << "Function: " << info.function_name << std::endl;
    if (!info.filename.empty()) {
        std::cout << "Source: " << info.filename 
                  << ":" << info.line_number << std::endl;
    }
}

// Get cache statistics
auto stats = resolver->get_cache_stats();
std::cout << "Cache hits: " << stats.hits << std::endl;
std::cout << "Cache size: " << stats.size << std::endl;
```

### Enhanced OffsetConverter

The `OffsetConverter` has been enhanced to support optional symbol resolution:

```cpp
#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

// Method 1: Constructor with resolver
auto resolver = std::make_shared<SymbolResolver>();
OffsetConverter converter(resolver);

// Method 2: Set resolver after construction
OffsetConverter converter;
auto resolver = std::make_shared<SymbolResolver>();
converter.set_symbol_resolver(resolver);

// Method 3: No resolver (backward compatible)
OffsetConverter converter;  // Works as before

// Convert with symbol resolution
LibraryMap lib_map;
lib_map.parse_current_process();
converter.add_map_snapshot(0, lib_map);

CallStack<> stack = /* ... */;

// Without symbols (fast, backward compatible)
auto frames1 = converter.convert(stack, 0);

// With symbols (more informative)
auto frames2 = converter.convert(stack, 0, true);

// Print resolved information
for (const auto& frame : frames2) {
    std::cout << frame.library_name << " + 0x" 
              << std::hex << frame.offset << std::dec;
    
    if (!frame.function_name.empty()) {
        std::cout << " [" << frame.function_name << "]";
    }
    
    if (!frame.filename.empty()) {
        std::cout << " at " << frame.filename 
                  << ":" << frame.line_number;
    }
    
    std::cout << std::endl;
}
```

### ResolvedFrame Structure

The `ResolvedFrame` structure includes all symbol information:

```cpp
struct ResolvedFrame {
    uintptr_t raw_address;        // Original raw address
    uintptr_t offset;             // Offset within library
    std::string library_name;     // Library path
    std::string function_name;    // Function name (if resolved)
    std::string filename;         // Source file (if available)
    uint32_t line_number;         // Line number (if available)
};
```

## Usage Examples

### Basic Symbol Resolution

```cpp
#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"
#include "sampling/library_map.h"
#include "sampling/call_stack.h"

using namespace perflow::analysis;
using namespace perflow::sampling;

// 1. Create symbol resolver
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback,
    true  // enable caching
);

// 2. Create converter with resolver
OffsetConverter converter(resolver);

// 3. Add library map
LibraryMap lib_map;
lib_map.parse_current_process();
converter.add_map_snapshot(0, lib_map);

// 4. Convert call stack with symbols
CallStack<> stack = /* captured stack */;
auto resolved = converter.convert(stack, 0, true);

// 5. Process results
for (const auto& frame : resolved) {
    if (!frame.function_name.empty()) {
        std::cout << "Function: " << frame.function_name << std::endl;
    }
    if (!frame.filename.empty()) {
        std::cout << "Source: " << frame.filename 
                  << ":" << frame.line_number << std::endl;
    }
}
```

### Different Resolution Strategies

```cpp
// Fast resolution (export symbols only)
auto fast_resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kDlAddrOnly,
    true
);

// Comprehensive resolution (requires debug symbols)
auto debug_resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAddr2LineOnly,
    true
);

// Balanced approach (try fast, fallback to comprehensive)
auto balanced_resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback,
    true
);
```

### Batch Processing with Caching

```cpp
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback,
    true  // enable cache
);
OffsetConverter converter(resolver);

// Process many call stacks
for (const auto& stack : all_captured_stacks) {
    auto resolved = converter.convert(stack, process_id, true);
    // Process resolved frames...
}

// Check cache effectiveness
auto stats = resolver->get_cache_stats();
double hit_rate = 100.0 * stats.hits / (stats.hits + stats.misses);
std::cout << "Cache hit rate: " << hit_rate << "%" << std::endl;
```

### Backward Compatibility

```cpp
// Old code continues to work without modification
OffsetConverter converter;
LibraryMap lib_map;
lib_map.parse_current_process();
converter.add_map_snapshot(0, lib_map);

CallStack<> stack = /* ... */;
auto resolved = converter.convert(stack, 0);

// Symbol fields will be empty but library and offset are resolved
for (const auto& frame : resolved) {
    std::cout << frame.library_name << " + 0x" 
              << std::hex << frame.offset << std::dec << std::endl;
}
```

## Performance Characteristics

### Resolution Performance

| Strategy | First Call | Cached Call | Memory |
|----------|------------|-------------|--------|
| kDlAddrOnly | 1-2 μs | ~10 ns | Negligible |
| kAddr2LineOnly | 10-20 ms | ~10 ns | ~100 bytes/symbol |
| kAutoFallback | 1-20 ms | ~10 ns | ~100 bytes/symbol |

### Cache Behavior

- **Cache Hit**: ~10 nanoseconds (hash map lookup)
- **Cache Miss**: Full resolution time + cache insertion
- **Memory**: ~100 bytes per cached symbol
- **Typical Usage**: 10,000 unique symbols = ~1 MB

### Recommendations

1. **For Development/Debugging**: Use `kAutoFallback` with caching
2. **For Production**: Use `kDlAddrOnly` for minimal overhead
3. **For Post-Analysis**: Use `kAddr2LineOnly` with debug symbols
4. **For Batch Processing**: Always enable caching

## Requirements

### System Requirements

- **dladdr support**: Available on all Linux systems (glibc)
- **addr2line tool**: Part of binutils package
  ```bash
  sudo apt-get install binutils
  ```

### Debug Symbols

For full source location resolution:

```bash
# Install debug symbols for system libraries
sudo apt-get install libc6-dbg

# Build your application with debug symbols
g++ -g -O2 myapp.cpp -o myapp

# Or keep debug symbols separate
objcopy --only-keep-debug myapp myapp.debug
objcopy --strip-debug myapp
objcopy --add-gnu-debuglink=myapp.debug myapp
```

## Integration with Post-Analysis

The symbol resolution integrates seamlessly with post-analysis workflows:

```cpp
#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"
#include "sampling/data_export.h"

// 1. Import captured data
DataImporter data_importer("perflow_rank_0.pflw");
LibraryMapImporter map_importer("perflow_rank_0.libmap");

StaticHashMap<CallStack<>, uint64_t, 10000> call_stacks;
data_importer.importData(call_stacks);

LibraryMap lib_map;
uint32_t rank;
map_importer.importMap(lib_map, &rank);

// 2. Create converter with symbol resolver
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback, true);
OffsetConverter converter(resolver);
converter.add_map_snapshot(rank, lib_map);

// 3. Process all call stacks with symbols
call_stacks.for_each([&](const CallStack<>& stack, uint64_t count) {
    auto resolved = converter.convert(stack, rank, true);
    
    std::cout << "Samples: " << count << std::endl;
    for (const auto& frame : resolved) {
        std::cout << "  ";
        if (!frame.function_name.empty()) {
            std::cout << frame.function_name;
        } else {
            std::cout << frame.library_name << "+0x" 
                      << std::hex << frame.offset << std::dec;
        }
        std::cout << std::endl;
    }
});
```

## Troubleshooting

### Symbols Not Resolving

**Problem**: All symbols show as `[not resolved]`

**Solutions**:
1. Ensure binaries have symbols:
   ```bash
   nm /path/to/binary | head
   file /path/to/binary
   ```

2. For stripped binaries, use debug symbol packages:
   ```bash
   sudo apt-get install libc6-dbg
   ```

3. For addr2line, check tool availability:
   ```bash
   which addr2line
   addr2line --version
   ```

### Performance Issues

**Problem**: Symbol resolution is slow

**Solutions**:
1. Enable caching (should be on by default):
   ```cpp
   auto resolver = std::make_shared<SymbolResolver>(
       SymbolResolver::Strategy::kAutoFallback,
       true  // enable cache
   );
   ```

2. Use `kDlAddrOnly` for faster resolution:
   ```cpp
   auto resolver = std::make_shared<SymbolResolver>(
       SymbolResolver::Strategy::kDlAddrOnly, true);
   ```

3. Monitor cache effectiveness:
   ```cpp
   auto stats = resolver->get_cache_stats();
   std::cout << "Hit rate: " 
             << (100.0 * stats.hits / (stats.hits + stats.misses)) 
             << "%" << std::endl;
   ```

### Memory Usage

**Problem**: High memory usage from symbol cache

**Solutions**:
1. Periodically clear cache:
   ```cpp
   resolver->clear_cache();
   ```

2. Disable caching for one-time analysis:
   ```cpp
   auto resolver = std::make_shared<SymbolResolver>(
       SymbolResolver::Strategy::kAutoFallback,
       false  // disable cache
   );
   ```

## Debugging Symbol Resolution

If symbol resolution is not working on your platform, you can enable debug mode to see detailed information about what's happening.

### Enabling Debug Mode

Set the `PERFLOW_SYMBOL_DEBUG` environment variable before running your program:

```bash
export PERFLOW_SYMBOL_DEBUG=1
./examples/symbol_resolution_example
```

Or for a single run:

```bash
PERFLOW_SYMBOL_DEBUG=1 ./examples/symbol_resolution_example
```

### Debug Output

With debug mode enabled, you'll see detailed output showing:
- Which resolution strategy is being used
- Cache hits and misses
- Each addr2line command being executed
- The raw output from addr2line (function name and location)
- Which text segment offset succeeded (for PIE executables)
- Why resolution failed (if it did)

Example debug output:
```
[SymbolResolver] Initialized with strategy=kAutoFallback, cache=enabled
[SymbolResolver] resolve("/path/to/binary", 0x1450)
[SymbolResolver] Cache MISS
[SymbolResolver] Using kAutoFallback strategy
[SymbolResolver] Trying dladdr first...
[SymbolResolver] dladdr failed, trying addr2line...
[SymbolResolver] resolve_with_addr2line(/path/to/binary, 0x1450)
[SymbolResolver]   Trying offset as-is: 0x1450
[SymbolResolver]     Executing: addr2line -e /path/to/binary -f -C 0x1450 2>/dev/null
[SymbolResolver]     Function name: '??'
[SymbolResolver]     Location: '??:0'
[SymbolResolver]     Both function and location unresolved
[SymbolResolver]   Trying with text_base 0x1000 -> adjusted offset 0x2450
[SymbolResolver]     Executing: addr2line -e /path/to/binary -f -C 0x2450 2>/dev/null
[SymbolResolver]     Function name: 'my_function()'
[SymbolResolver]     Location: '/path/to/source.cpp:42'
[SymbolResolver]     Resolved: func='my_function()', file='/path/to/source.cpp', line=42
[SymbolResolver]   SUCCESS with adjusted offset 0x2450 -> my_function()
```

### Programmatic Debug Mode

You can also enable debug mode programmatically:

```cpp
auto resolver = std::make_shared<SymbolResolver>(
    SymbolResolver::Strategy::kAutoFallback,
    true,   // enable cache
    true    // enable debug mode
);

// Or enable it later:
resolver->set_debug_mode(true);
```

### Troubleshooting with Debug Output

1. **Check if addr2line is working**: Look for "Executing: addr2line" lines to see the exact commands being run. You can copy and run them manually.

2. **Check text segment offset**: For PIE executables, the debug output shows which offset adjustment succeeded (e.g., "SUCCESS with adjusted offset 0x3a4e").

3. **Verify debug symbols**: If you see "Function name: '??'" for all offsets, your binary likely doesn't have debug symbols. Rebuild with `-g`.

4. **Check file paths**: Ensure the binary path passed to the resolver is correct and accessible.

## Best Practices

1. **Use Caching**: Always enable caching for batch processing
2. **Choose Right Strategy**: Pick strategy based on your use case
3. **Share Resolvers**: Reuse resolver instances across multiple converters
4. **Monitor Performance**: Check cache statistics periodically
5. **Graceful Degradation**: Code should work with or without symbols
6. **Optional Feature**: Keep symbol resolution opt-in
7. **Debug Symbols**: Ensure debug symbols available for best results

## Testing

Run symbol resolution tests:

```bash
cd build
./tests/perflow_tests --gtest_filter="SymbolResolver*:OffsetConverterSymbolResolution*"
```

Run example program:

```bash
cd build
./examples/symbol_resolution_example
```

## See Also

- [Call Stack Offset Conversion](CALL_STACK_OFFSET_CONVERSION.md)
- [Online Analysis API](ONLINE_ANALYSIS_API.md)
- [Implementation Summary](IMPLEMENTATION_SUMMARY.md)
- [Examples](../examples/symbol_resolution_example.cpp)

## References

- DWARF Debugging Standard: http://dwarfstd.org/
- ELF Format: `man 5 elf`
- dladdr: `man 3 dladdr`
- addr2line: `man 1 addr2line`
