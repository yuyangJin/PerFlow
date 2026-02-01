// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_OFFSET_CONVERTER_H_
#define PERFLOW_ANALYSIS_OFFSET_CONVERTER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "analysis/symbol_resolver.h"
#include "sampling/call_stack.h"
#include "sampling/library_map.h"

namespace perflow {
namespace analysis {

/// OffsetConverter converts raw call stack addresses to resolved frames
/// with library names and offsets for post-processing analysis
class OffsetConverter {
 public:
  /// Default constructor
  OffsetConverter() noexcept : map_snapshots_(), symbol_resolver_() {}
  
  /// Constructor with symbol resolver
  /// @param resolver Symbol resolver for function name and location resolution
  explicit OffsetConverter(std::shared_ptr<SymbolResolver> resolver) noexcept
      : map_snapshots_(), symbol_resolver_(resolver) {}
  
  /// Set symbol resolver (can be called after construction)
  /// @param resolver Symbol resolver to use (nullptr to disable symbol resolution)
  void set_symbol_resolver(std::shared_ptr<SymbolResolver> resolver) noexcept {
    symbol_resolver_ = resolver;
  }
  
  /// Check if symbol resolution is enabled
  bool has_symbol_resolver() const noexcept {
    return symbol_resolver_ != nullptr;
  }

  /// Add a library map snapshot for a specific process/rank
  /// @param id Process or rank identifier
  /// @param map Library map snapshot
  void add_map_snapshot(uint32_t id, const sampling::LibraryMap& map) {
    map_snapshots_[id] = map;
  }

  /// Convert a raw call stack to resolved frames using a specific map snapshot
  /// @tparam MaxDepth Maximum stack depth
  /// @param stack Raw call stack
  /// @param snapshot_id Process/rank ID to use for resolution
  /// @param resolve_symbols Whether to resolve symbols (requires SymbolResolver, default false)
  /// @return Vector of resolved frames
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth>
  std::vector<sampling::ResolvedFrame> convert(
      const sampling::CallStack<MaxDepth>& stack,
      uint32_t snapshot_id,
      bool resolve_symbols = false) const;

  /// Get the number of map snapshots stored
  size_t snapshot_count() const noexcept {
    return map_snapshots_.size();
  }

  /// Check if a snapshot exists for a given ID
  bool has_snapshot(uint32_t id) const noexcept {
    return map_snapshots_.find(id) != map_snapshots_.end();
  }

  /// Clear all map snapshots
  void clear() noexcept {
    map_snapshots_.clear();
  }

 private:
  // Map from process/rank ID to library map snapshot
  std::map<uint32_t, sampling::LibraryMap> map_snapshots_;
  
  // Optional symbol resolver for function names and source locations
  std::shared_ptr<SymbolResolver> symbol_resolver_;
};

// Template implementation
template <size_t MaxDepth>
std::vector<sampling::ResolvedFrame> OffsetConverter::convert(
    const sampling::CallStack<MaxDepth>& stack,
    uint32_t snapshot_id,
    bool resolve_symbols) const {
  std::vector<sampling::ResolvedFrame> resolved_frames;

  // Find the map snapshot for this process
  auto it = map_snapshots_.find(snapshot_id);
  if (it == map_snapshots_.end()) {
    // No map snapshot available - return frames with raw addresses only
    for (size_t i = 0; i < stack.depth(); ++i) {
      sampling::ResolvedFrame frame;
      frame.raw_address = stack.frame(i);
      frame.offset = stack.frame(i);
      frame.library_name = "[unknown]";
      resolved_frames.push_back(frame);
    }
    return resolved_frames;
  }

  const sampling::LibraryMap& lib_map = it->second;

  // Resolve each address in the call stack
  for (size_t i = 0; i < stack.depth(); ++i) {
    uintptr_t addr = stack.frame(i);
    sampling::ResolvedFrame frame;
    frame.raw_address = addr;

    // Try to resolve the address
    auto resolved = lib_map.resolve(addr);
    if (resolved.has_value()) {
      frame.library_name = resolved->first;
      frame.offset = resolved->second;
      
      // Optionally resolve symbols if requested and resolver is available
      if (resolve_symbols && symbol_resolver_) {
        auto symbol_info = symbol_resolver_->resolve(frame.library_name, frame.offset);
        frame.function_name = symbol_info.function_name;
        frame.filename = symbol_info.filename;
        frame.line_number = symbol_info.line_number;
      }
    } else {
      // Could not resolve - address not in any known library
      frame.library_name = "[unresolved]";
      frame.offset = addr;
    }

    resolved_frames.push_back(frame);
  }

  return resolved_frames;
}

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_OFFSET_CONVERTER_H_
