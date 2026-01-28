// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_OFFSET_CONVERTER_H_
#define PERFLOW_ANALYSIS_OFFSET_CONVERTER_H_

#include <cstdint>
#include <map>
#include <vector>

#include "sampling/call_stack.h"
#include "sampling/library_map.h"

namespace perflow {
namespace analysis {

/// OffsetConverter performs post-processing conversion of raw runtime addresses
/// to static library offsets using captured library map snapshots.
///
/// This approach minimizes runtime overhead by deferring address resolution
/// to the analysis phase when performance is not critical.
///
/// Thread Safety: Not thread-safe. External synchronization required if used
/// from multiple threads.
class OffsetConverter {
 public:
  /// Default constructor
  OffsetConverter() noexcept = default;

  /// Add a library map snapshot for a specific map ID
  /// @param id Unique identifier for this map snapshot
  /// @param map Library map to associate with this ID
  void add_map_snapshot(uint32_t id, const sampling::LibraryMap& map) {
    map_snapshots_[id] = map;
  }

  /// Add a library map snapshot by moving
  /// @param id Unique identifier for this map snapshot
  /// @param map Library map to move into the snapshot storage
  void add_map_snapshot(uint32_t id, sampling::LibraryMap&& map) {
    map_snapshots_[id] = std::move(map);
  }

  /// Get the number of stored map snapshots
  size_t snapshot_count() const noexcept {
    return map_snapshots_.size();
  }

  /// Check if a snapshot exists for a given ID
  /// @param id Map snapshot ID to check
  /// @return true if snapshot exists, false otherwise
  bool has_snapshot(uint32_t id) const {
    return map_snapshots_.find(id) != map_snapshots_.end();
  }

  /// Convert a raw call stack to a resolved call stack
  /// @param raw Raw call stack with runtime addresses
  /// @return Resolved call stack with (library, offset) pairs
  /// @throws std::runtime_error if map_id is not found in snapshots
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth>
  sampling::ResolvedCallStack convert(const sampling::RawCallStack<MaxDepth>& raw) const {
    sampling::ResolvedCallStack resolved(raw.timestamp, raw.map_id);

    // Find the appropriate library map snapshot
    auto it = map_snapshots_.find(raw.map_id);
    if (it == map_snapshots_.end()) {
      // If map not found, create unresolved frames with raw addresses
      for (size_t i = 0; i < raw.addresses.depth(); ++i) {
        uintptr_t addr = raw.addresses.frame(i);
        resolved.frames.emplace_back("[unknown]", addr, addr);
      }
      return resolved;
    }

    const sampling::LibraryMap& map = it->second;

    // Resolve each address in the call stack
    for (size_t i = 0; i < raw.addresses.depth(); ++i) {
      uintptr_t addr = raw.addresses.frame(i);
      auto result = map.resolve(addr);

      if (result.has_value()) {
        // Successfully resolved to (library, offset)
        resolved.frames.emplace_back(result->first, result->second, addr);
      } else {
        // Could not resolve - store as unknown with raw address
        resolved.frames.emplace_back("[unknown]", addr, addr);
      }
    }

    return resolved;
  }

  /// Convert multiple raw call stacks to resolved call stacks
  /// @param raw_stacks Vector of raw call stacks
  /// @return Vector of resolved call stacks
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth>
  std::vector<sampling::ResolvedCallStack> convert_batch(
      const std::vector<sampling::RawCallStack<MaxDepth>>& raw_stacks) const {
    std::vector<sampling::ResolvedCallStack> resolved_stacks;
    resolved_stacks.reserve(raw_stacks.size());

    for (const auto& raw : raw_stacks) {
      resolved_stacks.push_back(convert(raw));
    }

    return resolved_stacks;
  }

  /// Convert a simple CallStack using a specific map ID
  /// @param stack Call stack with raw addresses
  /// @param map_id Library map snapshot ID to use
  /// @param timestamp Optional timestamp (defaults to 0)
  /// @return Resolved call stack
  template <size_t MaxDepth = sampling::kDefaultMaxStackDepth>
  sampling::ResolvedCallStack convert_with_map_id(
      const sampling::CallStack<MaxDepth>& stack,
      uint32_t map_id,
      int64_t timestamp = 0) const {
    sampling::RawCallStack<MaxDepth> raw(stack, timestamp, map_id);
    return convert(raw);
  }

  /// Clear all map snapshots
  void clear() noexcept {
    map_snapshots_.clear();
  }

  /// Get a reference to a specific map snapshot
  /// @param id Map snapshot ID
  /// @return Pointer to the map if found, nullptr otherwise
  const sampling::LibraryMap* get_snapshot(uint32_t id) const {
    auto it = map_snapshots_.find(id);
    if (it != map_snapshots_.end()) {
      return &it->second;
    }
    return nullptr;
  }

 private:
  /// Map of snapshot ID to library map
  std::map<uint32_t, sampling::LibraryMap> map_snapshots_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_OFFSET_CONVERTER_H_
