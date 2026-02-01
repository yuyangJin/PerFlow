// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_SYMBOL_RESOLVER_H_
#define PERFLOW_ANALYSIS_SYMBOL_RESOLVER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

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
  explicit SymbolResolver(Strategy strategy = Strategy::kAutoFallback,
                          bool enable_cache = true) noexcept;
  
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
  
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_SYMBOL_RESOLVER_H_
