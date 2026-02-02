// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_SYMBOL_RESOLVER_H_
#define PERFLOW_ANALYSIS_SYMBOL_RESOLVER_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <link.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <array>

namespace perflow {
namespace analysis {

/// Symbol information resolved from an address
struct SymbolInfo {
  std::string function_name;  // Function name (empty if not resolved)
  std::string filename;       // Source file path (empty if not available)
  uint32_t line_number;       // Line number (0 if not available)
  
  SymbolInfo() noexcept : function_name(), filename(), line_number(0) {}
  
  SymbolInfo(const std::string& func, const std::string& file, uint32_t line) noexcept
      : function_name(func), filename(file), line_number(line) {}
  
  /// Check if symbol was resolved successfully
  bool is_resolved() const noexcept {
    return !function_name.empty();
  }
};

/// SymbolResolver resolves memory addresses to function names and source locations
/// using multiple resolution strategies with graceful fallback:
/// 1. dladdr() - Fast, uses export/dynamic symbols
/// 2. addr2line - Slower, uses DWARF debug information
/// 3. Fallback - Returns hex offset if symbols unavailable
class SymbolResolver {
 public:
  /// Resolution strategy
  enum class Strategy {
    kDlAddrOnly,     // Only use dladdr (fast, export symbols only)
    kAddr2LineOnly,  // Only use addr2line (slower, needs debug symbols)
    kAutoFallback    // Try dladdr first, fallback to addr2line
  };
  
  /// Constructor
  /// @param strategy Resolution strategy to use
  /// @param enable_cache Whether to cache resolved symbols
  /// @param debug_mode Whether to print debug information
  explicit SymbolResolver(Strategy strategy = Strategy::kAutoFallback,
                          bool enable_cache = true,
                          bool debug_mode = false) noexcept;
  
  /// Destructor
  ~SymbolResolver() noexcept;
  
  // Disable copy (resources might be heavy)
  SymbolResolver(const SymbolResolver&) = delete;
  SymbolResolver& operator=(const SymbolResolver&) = delete;
  
  // Enable move
  SymbolResolver(SymbolResolver&&) noexcept;
  SymbolResolver& operator=(SymbolResolver&&) noexcept;
  
  /// Resolve an address to symbol information
  /// @param library_path Path to the library/executable
  /// @param offset Offset within the library
  /// @return SymbolInfo with function name and location (may be empty if not resolved)
  SymbolInfo resolve(const std::string& library_path, uintptr_t offset) const;
  
  /// Clear the symbol cache
  void clear_cache() noexcept;
  
  /// Get cache statistics
  struct CacheStats {
    size_t hits;
    size_t misses;
    size_t size;
  };
  
  CacheStats get_cache_stats() const noexcept;
  
  /// Enable or disable debug mode
  void set_debug_mode(bool enabled) noexcept;
  
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Implementation
// ============================================================================

// Cache key: library path + offset
using CacheKey = std::pair<std::string, uintptr_t>;

struct CacheKeyHash {
  size_t operator()(const CacheKey& key) const noexcept {
    // Simple hash combining
    size_t h1 = std::hash<std::string>{}(key.first);
    size_t h2 = std::hash<uintptr_t>{}(key.second);
    return h1 ^ (h2 << 1);
  }
};

// Implementation details
struct SymbolResolver::Impl {
  Strategy strategy;
  bool enable_cache;
  bool debug_mode;
  
  // Symbol cache
  mutable std::unordered_map<CacheKey, SymbolInfo, CacheKeyHash> cache;
  mutable size_t cache_hits;
  mutable size_t cache_misses;
  
  Impl(Strategy strat, bool cache_enabled, bool debug) noexcept
      : strategy(strat), enable_cache(cache_enabled), debug_mode(debug),
        cache_hits(0), cache_misses(0) {
    if (debug_mode) {
      std::cerr << "[SymbolResolver] Initialized with strategy=";
      switch (strategy) {
        case Strategy::kDlAddrOnly: std::cerr << "kDlAddrOnly"; break;
        case Strategy::kAddr2LineOnly: std::cerr << "kAddr2LineOnly"; break;
        case Strategy::kAutoFallback: std::cerr << "kAutoFallback"; break;
      }
      std::cerr << ", cache=" << (enable_cache ? "enabled" : "disabled") << "\n";
    }
  }
  
  /// Try to resolve using dladdr (fast, export symbols)
  SymbolInfo resolve_with_dladdr(const std::string& library_path, 
                                  uintptr_t offset) const {
    // For dladdr, we need an actual runtime address, not just an offset.
    // We need to open the library and calculate the address.
    
    void* handle = dlopen(library_path.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    bool should_close = false;
    
    if (!handle) {
      // Library not loaded yet, try to load it
      handle = dlopen(library_path.c_str(), RTLD_LAZY);
      if (!handle) {
        return SymbolInfo();
      }
      should_close = true;
    }
    
    // Get any symbol from the library to find its base address
    void* sym = dlsym(handle, "__not_a_real_symbol_123456__");
    // Even if symbol lookup fails, we can still use dladdr on the handle
    (void)sym;  // Suppress unused warning
    
    // We need to get the base address via link_map
    struct link_map* map = nullptr;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) == 0 && map != nullptr) {
      // Calculate the actual address
      void* addr = reinterpret_cast<void*>(map->l_addr + offset);
      
      Dl_info info;
      if (dladdr(addr, &info) != 0 && info.dli_sname != nullptr) {
        std::string func_name = info.dli_sname;
        if (should_close) {
          dlclose(handle);
        }
        return SymbolInfo(func_name, "", 0);
      }
    }
    
    if (should_close) {
      dlclose(handle);
    }
    return SymbolInfo();
  }
  
  /// Try to resolve using addr2line (slower, needs debug symbols)
  SymbolInfo resolve_with_addr2line(const std::string& library_path,
                                     uintptr_t offset) const {
    if (debug_mode) {
      std::cerr << "[SymbolResolver] resolve_with_addr2line(" << library_path 
                << ", 0x" << std::hex << offset << std::dec << ")\n";
    }
    
    // For PIE executables, we may need to try different offsets
    // The offset from LibraryMap might be relative to the first LOAD segment,
    // but symbols are relative to their actual segment (e.g., .text at 0x3000)
    
    // Try the offset as-is first
    if (debug_mode) {
      std::cerr << "[SymbolResolver]   Trying offset as-is: 0x" << std::hex << offset << std::dec << "\n";
    }
    SymbolInfo result = try_addr2line(library_path, offset);
    if (result.is_resolved()) {
      if (debug_mode) {
        std::cerr << "[SymbolResolver]   SUCCESS with offset 0x" << std::hex << offset << std::dec 
                  << " -> " << result.function_name << "\n";
      }
      return result;
    }
    
    // For PIE executables, try common text segment offsets
    // Try both positive offsets (for typical PIE) and zero/negative adjustments
    // Common text segment starts: 0x1000, 0x2000, 0x3000, 0x4000, etc.
    for (uintptr_t text_base : {0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 
                                 0x6000, 0x8000, 0xa000, 0x10000}) {
      uintptr_t adjusted_offset = offset + text_base;
      if (debug_mode) {
        std::cerr << "[SymbolResolver]   Trying with text_base 0x" << std::hex << text_base 
                  << " -> adjusted offset 0x" << adjusted_offset << std::dec << "\n";
      }
      result = try_addr2line(library_path, adjusted_offset);
      if (result.is_resolved()) {
        if (debug_mode) {
          std::cerr << "[SymbolResolver]   SUCCESS with adjusted offset 0x" << std::hex << adjusted_offset 
                    << std::dec << " -> " << result.function_name << "\n";
        }
        return result;
      }
    }
    
    if (debug_mode) {
      std::cerr << "[SymbolResolver]   FAILED to resolve with any offset\n";
    }
    return SymbolInfo();
  }
  
  /// Helper to try addr2line with a specific offset
  SymbolInfo try_addr2line(const std::string& library_path,
                           uintptr_t offset) const {
    // Build addr2line command
    std::ostringstream cmd;
    cmd << "addr2line -e " << library_path 
        << " -f -C 0x" << std::hex << offset 
        << " 2>/dev/null";
    
    if (debug_mode) {
      std::cerr << "[SymbolResolver]     Executing: " << cmd.str() << "\n";
    }
    
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
      if (debug_mode) {
        std::cerr << "[SymbolResolver]     ERROR: popen failed\n";
      }
      return SymbolInfo();
    }
    
    // Read function name (first line)
    std::array<char, 512> func_buffer{};
    if (!fgets(func_buffer.data(), func_buffer.size(), pipe)) {
      pclose(pipe);
      if (debug_mode) {
        std::cerr << "[SymbolResolver]     ERROR: Failed to read function name line\n";
      }
      return SymbolInfo();
    }
    
    std::string func_name(func_buffer.data());
    // Remove trailing newline and whitespace
    while (!func_name.empty() && (func_name.back() == '\n' || func_name.back() == '\r' || func_name.back() == ' ')) {
      func_name.pop_back();
    }
    
    if (debug_mode) {
      std::cerr << "[SymbolResolver]     Function name: '" << func_name << "'\n";
    }
    
    // Read source location (second line)
    std::array<char, 512> loc_buffer{};
    if (!fgets(loc_buffer.data(), loc_buffer.size(), pipe)) {
      pclose(pipe);
      if (debug_mode) {
        std::cerr << "[SymbolResolver]     No location line, checking if function is valid\n";
      }
      // We have function name but no location
      // Accept function name if it looks valid (not ?? or empty)
      if (!func_name.empty() && func_name != "??" && func_name != "??:0" && func_name != "??:?") {
        if (debug_mode) {
          std::cerr << "[SymbolResolver]     Returning function only: " << func_name << "\n";
        }
        return SymbolInfo(func_name, "", 0);
      }
      if (debug_mode) {
        std::cerr << "[SymbolResolver]     Function name not valid, returning unresolved\n";
      }
      return SymbolInfo();
    }
    
    std::string location(loc_buffer.data());
    // Remove trailing newline and whitespace
    while (!location.empty() && (location.back() == '\n' || location.back() == '\r' || location.back() == ' ')) {
      location.pop_back();
    }
    
    if (debug_mode) {
      std::cerr << "[SymbolResolver]     Location: '" << location << "'\n";
    }
    
    pclose(pipe);
    
    // Check if resolution failed - addr2line returns "??" for unresolved
    // Be more lenient - only reject if both function and location are clearly unresolved
    if (func_name == "??" && (location == "??:0" || location == "??:?" || location == "??")) {
      if (debug_mode) {
        std::cerr << "[SymbolResolver]     Both function and location unresolved (" "?" "?" ")\n";
      }
      return SymbolInfo();  // Not resolved
    }
    
    // If function name is valid but location is ??, still return the function
    if (func_name == "??") {
      // Only function is unresolved, check if we have location
      if (location != "??:0" && location != "??:?" && location != "??") {
        func_name = "";  // Clear unresolved function but keep location
        if (debug_mode) {
          std::cerr << "[SymbolResolver]     Function unresolved but have location\n";
        }
      } else {
        if (debug_mode) {
          std::cerr << "[SymbolResolver]     Both unresolved\n";
        }
        return SymbolInfo();  // Both unresolved
      }
    }
    
    std::string filename;
    uint32_t line_number = 0;
    
    // Parse location if it doesn't look like an unresolved marker
    if (!location.empty() && location != "??:0" && location != "??:?" && location != "??") {
      size_t colon_pos = location.rfind(':');
      if (colon_pos != std::string::npos) {
        filename = location.substr(0, colon_pos);
        std::string line_str = location.substr(colon_pos + 1);
        
        // Try to parse line number
        try {
          if (line_str != "?" && !line_str.empty()) {
            line_number = static_cast<uint32_t>(std::stoul(line_str));
          }
        } catch (...) {
          // Failed to parse line number, keep it as 0
          if (debug_mode) {
            std::cerr << "[SymbolResolver]     Failed to parse line number: " << line_str << "\n";
          }
        }
      }
    }
    
    // Return if we have at least function name or filename
    if (!func_name.empty() || !filename.empty()) {
      if (debug_mode) {
        std::cerr << "[SymbolResolver]     Resolved: func='" << func_name 
                  << "', file='" << filename << "', line=" << line_number << "\n";
      }
      return SymbolInfo(func_name, filename, line_number);
    }
    
    if (debug_mode) {
      std::cerr << "[SymbolResolver]     No valid info, returning unresolved\n";
    }
    return SymbolInfo();
  }
};

inline SymbolResolver::SymbolResolver(Strategy strategy, bool enable_cache, bool debug_mode) noexcept
    : impl_(std::make_unique<Impl>(strategy, enable_cache, debug_mode)) {
}

inline SymbolResolver::~SymbolResolver() noexcept = default;

inline SymbolResolver::SymbolResolver(SymbolResolver&&) noexcept = default;

inline SymbolResolver& SymbolResolver::operator=(SymbolResolver&&) noexcept = default;

inline void SymbolResolver::set_debug_mode(bool enabled) noexcept {
  impl_->debug_mode = enabled;
}

inline SymbolInfo SymbolResolver::resolve(const std::string& library_path, 
                                    uintptr_t offset) const {
  if (impl_->debug_mode) {
    std::cerr << "\n[SymbolResolver] resolve(\"" << library_path 
              << "\", 0x" << std::hex << offset << std::dec << ")\n";
  }
  
  // Check cache first
  if (impl_->enable_cache) {
    CacheKey key(library_path, offset);
    auto it = impl_->cache.find(key);
    if (it != impl_->cache.end()) {
      impl_->cache_hits++;
      if (impl_->debug_mode) {
        std::cerr << "[SymbolResolver] Cache HIT -> " << it->second.function_name << "\n";
      }
      return it->second;
    }
    impl_->cache_misses++;
    if (impl_->debug_mode) {
      std::cerr << "[SymbolResolver] Cache MISS\n";
    }
  }
  
  SymbolInfo result;
  
  // Try resolution based on strategy
  switch (impl_->strategy) {
    case Strategy::kDlAddrOnly:
      if (impl_->debug_mode) {
        std::cerr << "[SymbolResolver] Using kDlAddrOnly strategy\n";
      }
      result = impl_->resolve_with_dladdr(library_path, offset);
      break;
      
    case Strategy::kAddr2LineOnly:
      if (impl_->debug_mode) {
        std::cerr << "[SymbolResolver] Using kAddr2LineOnly strategy\n";
      }
      result = impl_->resolve_with_addr2line(library_path, offset);
      break;
      
    case Strategy::kAutoFallback:
      if (impl_->debug_mode) {
        std::cerr << "[SymbolResolver] Using kAutoFallback strategy\n";
        std::cerr << "[SymbolResolver] Trying dladdr first...\n";
      }
      // Try dladdr first (faster)
      result = impl_->resolve_with_dladdr(library_path, offset);
      
      // If dladdr didn't find the symbol, try addr2line
      if (!result.is_resolved()) {
        if (impl_->debug_mode) {
          std::cerr << "[SymbolResolver] dladdr failed, trying addr2line...\n";
        }
        result = impl_->resolve_with_addr2line(library_path, offset);
      } else if (impl_->debug_mode) {
        std::cerr << "[SymbolResolver] dladdr succeeded: " << result.function_name << "\n";
      }
      break;
  }
  
  // Cache the result
  if (impl_->enable_cache) {
    CacheKey key(library_path, offset);
    impl_->cache[key] = result;
  }
  
  return result;
}

inline void SymbolResolver::clear_cache() noexcept {
  impl_->cache.clear();
  impl_->cache_hits = 0;
  impl_->cache_misses = 0;
}

inline SymbolResolver::CacheStats SymbolResolver::get_cache_stats() const noexcept {
  return CacheStats{
    impl_->cache_hits,
    impl_->cache_misses,
    impl_->cache.size()
  };
}

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_SYMBOL_RESOLVER_H_
