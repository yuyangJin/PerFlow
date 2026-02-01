// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include "analysis/symbol_resolver.h"

#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <array>

namespace perflow {
namespace analysis {

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

// Implementation details hidden from header
struct SymbolResolver::Impl {
  Strategy strategy;
  bool enable_cache;
  
  // Symbol cache
  mutable std::unordered_map<CacheKey, SymbolInfo, CacheKeyHash> cache;
  mutable size_t cache_hits;
  mutable size_t cache_misses;
  
  Impl(Strategy strat, bool cache_enabled) noexcept
      : strategy(strat), enable_cache(cache_enabled),
        cache_hits(0), cache_misses(0) {}
  
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
    // Build addr2line command
    std::ostringstream cmd;
    cmd << "addr2line -e " << library_path 
        << " -f -C 0x" << std::hex << offset 
        << " 2>/dev/null";
    
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
      return SymbolInfo();
    }
    
    // Read function name (first line)
    std::array<char, 512> func_buffer{};
    if (!fgets(func_buffer.data(), func_buffer.size(), pipe)) {
      pclose(pipe);
      return SymbolInfo();
    }
    
    std::string func_name(func_buffer.data());
    // Remove trailing newline
    if (!func_name.empty() && func_name.back() == '\n') {
      func_name.pop_back();
    }
    
    // Read source location (second line)
    std::array<char, 512> loc_buffer{};
    if (!fgets(loc_buffer.data(), loc_buffer.size(), pipe)) {
      pclose(pipe);
      // We have function name but no location
      if (func_name != "??" && func_name != "??:0" && func_name != "??:?") {
        return SymbolInfo(func_name, "", 0);
      }
      return SymbolInfo();
    }
    
    std::string location(loc_buffer.data());
    if (!location.empty() && location.back() == '\n') {
      location.pop_back();
    }
    
    pclose(pipe);
    
    // Parse location (format: "filename:line" or "??:0" or "??:?")
    if (func_name == "??" || location == "??:0" || location == "??:?") {
      return SymbolInfo();  // Not resolved
    }
    
    std::string filename;
    uint32_t line_number = 0;
    
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
      }
    }
    
    return SymbolInfo(func_name, filename, line_number);
  }
};

SymbolResolver::SymbolResolver(Strategy strategy, bool enable_cache) noexcept
    : impl_(std::make_unique<Impl>(strategy, enable_cache)) {
}

SymbolResolver::~SymbolResolver() noexcept = default;

SymbolResolver::SymbolResolver(SymbolResolver&&) noexcept = default;

SymbolResolver& SymbolResolver::operator=(SymbolResolver&&) noexcept = default;

SymbolInfo SymbolResolver::resolve(const std::string& library_path, 
                                    uintptr_t offset) const {
  // Check cache first
  if (impl_->enable_cache) {
    CacheKey key(library_path, offset);
    auto it = impl_->cache.find(key);
    if (it != impl_->cache.end()) {
      impl_->cache_hits++;
      return it->second;
    }
    impl_->cache_misses++;
  }
  
  SymbolInfo result;
  
  // Try resolution based on strategy
  switch (impl_->strategy) {
    case Strategy::kDlAddrOnly:
      result = impl_->resolve_with_dladdr(library_path, offset);
      break;
      
    case Strategy::kAddr2LineOnly:
      result = impl_->resolve_with_addr2line(library_path, offset);
      break;
      
    case Strategy::kAutoFallback:
      // Try dladdr first (faster)
      result = impl_->resolve_with_dladdr(library_path, offset);
      
      // If dladdr didn't find the symbol, try addr2line
      if (!result.is_resolved()) {
        result = impl_->resolve_with_addr2line(library_path, offset);
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

void SymbolResolver::clear_cache() noexcept {
  impl_->cache.clear();
  impl_->cache_hits = 0;
  impl_->cache_misses = 0;
}

SymbolResolver::CacheStats SymbolResolver::get_cache_stats() const noexcept {
  return CacheStats{
    impl_->cache_hits,
    impl_->cache_misses,
    impl_->cache.size()
  };
}

}  // namespace analysis
}  // namespace perflow
