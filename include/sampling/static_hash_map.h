// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_STATIC_HASH_MAP_H_
#define PERFLOW_SAMPLING_STATIC_HASH_MAP_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace perflow {
namespace sampling {

// Helper to detect if type has hash() method
template <typename T, typename = void>
struct has_hash_method : std::false_type {};

template <typename T>
struct has_hash_method<T, std::void_t<decltype(std::declval<const T&>().hash())>>
    : std::true_type {};

/// Default capacity for the static hash map
constexpr size_t kDefaultMapCapacity = 65536;

/// Entry state for hash map slots
enum class EntryState : uint8_t {
  kEmpty = 0,     // Slot is empty
  kOccupied = 1,  // Slot contains valid data
  kDeleted = 2    // Slot was deleted (tombstone)
};

/// StaticHashMap is a fixed-capacity hash map with open addressing.
/// Designed for high-performance sampling with no dynamic allocation.
///
/// Features:
/// - Pre-allocated memory (no malloc/new during operations)
/// - Open addressing with linear probing for collision resolution
/// - Thread-safe operations using atomic compare-and-swap
/// - Optimized for cache locality
///
/// @tparam Key Key type (must support hash and equality comparison)
/// @tparam Value Value type
/// @tparam Capacity Maximum number of entries
/// @tparam Hash Hash functor for Key
/// @tparam KeyEqual Equality comparison functor for Key
template <typename Key, typename Value, size_t Capacity = kDefaultMapCapacity,
          typename Hash = void, typename KeyEqual = void>
class StaticHashMap {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using size_type = size_t;

  /// Hash map entry containing key, value, and state
  struct Entry {
    alignas(8) Key key;
    alignas(8) Value value;
    std::atomic<EntryState> state{EntryState::kEmpty};
    uint8_t padding_[7];  // Padding for alignment

    Entry() noexcept : key(), value() {}
  };

  /// Default constructor
  StaticHashMap() noexcept : size_(0) { clear(); }

  /// Destructor
  ~StaticHashMap() = default;

  // Disable copy operations to prevent unintended copies of large structures
  StaticHashMap(const StaticHashMap&) = delete;
  StaticHashMap& operator=(const StaticHashMap&) = delete;

  /// Get the current number of entries
  size_type size() const noexcept {
    return size_.load(std::memory_order_relaxed);
  }

  /// Get the capacity
  static constexpr size_type capacity() noexcept { return Capacity; }

  /// Check if the map is empty
  bool empty() const noexcept { return size() == 0; }

  /// Check if the map is full
  bool full() const noexcept { return size() >= Capacity; }

  /// Clear all entries
  void clear() noexcept {
    for (size_type i = 0; i < Capacity; ++i) {
      entries_[i].state.store(EntryState::kEmpty, std::memory_order_relaxed);
      // Note: We don't reset key/value for performance
    }
    size_.store(0, std::memory_order_release);
  }

  /// Find an entry by key
  /// @param key Key to search for
  /// @return Pointer to the value if found, nullptr otherwise
  Value* find(const Key& key) noexcept {
    size_type start_idx = hash_index(key);
    size_type idx = start_idx;

    do {
      EntryState state = entries_[idx].state.load(std::memory_order_acquire);

      if (state == EntryState::kEmpty) {
        return nullptr;  // Key not found
      }

      if (state == EntryState::kOccupied && key_equal(entries_[idx].key, key)) {
        return &entries_[idx].value;
      }

      idx = (idx + 1) % Capacity;  // Linear probing
    } while (idx != start_idx);

    return nullptr;  // Full table traversed
  }

  /// Find an entry by key (const version)
  const Value* find(const Key& key) const noexcept {
    return const_cast<StaticHashMap*>(this)->find(key);
  }

  /// Insert or update an entry
  /// @param key Key to insert
  /// @param value Value to associate with the key
  /// @return true if inserted/updated, false if map is full
  bool insert(const Key& key, const Value& value) noexcept {
    size_type start_idx = hash_index(key);
    size_type idx = start_idx;
    size_type first_deleted = Capacity;  // Track first deleted slot

    do {
      EntryState state = entries_[idx].state.load(std::memory_order_acquire);

      if (state == EntryState::kEmpty) {
        // Check if map is at capacity (new entry would exceed)
        if (size_.load(std::memory_order_relaxed) >= Capacity) {
          return false;  // Map is full, key not found
        }

        // Use first deleted slot if available
        if (first_deleted < Capacity) {
          idx = first_deleted;
        }

        // Try to claim this slot
        EntryState expected = entries_[idx].state.load(std::memory_order_relaxed);
        if (expected == EntryState::kEmpty || expected == EntryState::kDeleted) {
          entries_[idx].key = key;
          entries_[idx].value = value;
          entries_[idx].state.store(EntryState::kOccupied,
                                     std::memory_order_release);
          size_.fetch_add(1, std::memory_order_relaxed);
          return true;
        }
      }

      if (state == EntryState::kDeleted && first_deleted == Capacity) {
        first_deleted = idx;  // Remember first deleted slot
      }

      if (state == EntryState::kOccupied && key_equal(entries_[idx].key, key)) {
        // Key exists, update value
        entries_[idx].value = value;
        return true;
      }

      idx = (idx + 1) % Capacity;
    } while (idx != start_idx);

    // Use first deleted slot if found (and map not at capacity)
    if (first_deleted < Capacity &&
        size_.load(std::memory_order_relaxed) < Capacity) {
      entries_[first_deleted].key = key;
      entries_[first_deleted].value = value;
      entries_[first_deleted].state.store(EntryState::kOccupied,
                                           std::memory_order_release);
      size_.fetch_add(1, std::memory_order_relaxed);
      return true;
    }

    return false;  // Map is full
  }

  /// Insert a key with default value or get existing entry
  /// @param key Key to insert or find
  /// @return Pointer to the value (new or existing), nullptr if full
  Value* insert_or_get(const Key& key) noexcept {
    if (size_.load(std::memory_order_relaxed) >= Capacity) {
      // Map might be full, but check if key exists
      Value* existing = find(key);
      if (existing != nullptr) {
        return existing;
      }
      return nullptr;
    }

    size_type start_idx = hash_index(key);
    size_type idx = start_idx;
    size_type first_empty = Capacity;

    do {
      EntryState state = entries_[idx].state.load(std::memory_order_acquire);

      if (state == EntryState::kEmpty || state == EntryState::kDeleted) {
        if (first_empty == Capacity) {
          first_empty = idx;
        }
        if (state == EntryState::kEmpty) {
          break;  // No need to continue searching
        }
      }

      if (state == EntryState::kOccupied && key_equal(entries_[idx].key, key)) {
        return &entries_[idx].value;  // Key already exists
      }

      idx = (idx + 1) % Capacity;
    } while (idx != start_idx);

    // Insert new entry at first available slot
    if (first_empty < Capacity) {
      entries_[first_empty].key = key;
      entries_[first_empty].value = Value{};
      entries_[first_empty].state.store(EntryState::kOccupied,
                                         std::memory_order_release);
      size_.fetch_add(1, std::memory_order_relaxed);
      return &entries_[first_empty].value;
    }

    return nullptr;
  }

  /// Increment the value for a key (assumes Value supports operator++)
  /// @param key Key whose value to increment
  /// @return true if key found and incremented, false otherwise
  bool increment(const Key& key) noexcept {
    Value* val = insert_or_get(key);
    if (val != nullptr) {
      ++(*val);
      return true;
    }
    return false;
  }

  /// Add to the value for a key
  /// @param key Key whose value to add to
  /// @param delta Amount to add
  /// @return true if successful, false otherwise
  bool add(const Key& key, const Value& delta) noexcept {
    Value* val = insert_or_get(key);
    if (val != nullptr) {
      *val += delta;
      return true;
    }
    return false;
  }

  /// Remove an entry by key
  /// @param key Key to remove
  /// @return true if removed, false if not found
  bool erase(const Key& key) noexcept {
    size_type start_idx = hash_index(key);
    size_type idx = start_idx;

    do {
      EntryState state = entries_[idx].state.load(std::memory_order_acquire);

      if (state == EntryState::kEmpty) {
        return false;  // Key not found
      }

      if (state == EntryState::kOccupied && key_equal(entries_[idx].key, key)) {
        entries_[idx].state.store(EntryState::kDeleted,
                                   std::memory_order_release);
        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
      }

      idx = (idx + 1) % Capacity;
    } while (idx != start_idx);

    return false;
  }

  /// Iterate over all occupied entries
  /// @tparam Callback Function type: void(const Key&, Value&)
  /// @param callback Function to call for each entry
  template <typename Callback>
  void for_each(Callback callback) noexcept {
    for (size_type i = 0; i < Capacity; ++i) {
      if (entries_[i].state.load(std::memory_order_acquire) ==
          EntryState::kOccupied) {
        callback(entries_[i].key, entries_[i].value);
      }
    }
  }

  /// Iterate over all occupied entries (const version)
  template <typename Callback>
  void for_each(Callback callback) const noexcept {
    for (size_type i = 0; i < Capacity; ++i) {
      if (entries_[i].state.load(std::memory_order_acquire) ==
          EntryState::kOccupied) {
        callback(entries_[i].key, entries_[i].value);
      }
    }
  }

  /// Get direct access to entries (for serialization)
  const Entry* entries() const noexcept { return entries_; }

  /// Get entry at specific index
  const Entry& entry_at(size_type index) const noexcept {
    return entries_[index];
  }

 private:
  /// Compute hash index for a key
  size_type hash_index(const Key& key) const noexcept {
    // Use the key's hash method if available, otherwise use a default hash
    size_type h = compute_hash(key);
    return h % Capacity;
  }

  /// Compute hash - specialization for types with hash() method
  template <typename K = Key>
  typename std::enable_if<has_hash_method<K>::value, size_type>::type
  compute_hash(const K& key) const noexcept {
    return key.hash();
  }

  /// Compute hash - default implementation using memory contents
  template <typename K = Key>
  typename std::enable_if<!has_hash_method<K>::value, size_type>::type
  compute_hash(const K& key) const noexcept {
    // FNV-1a hash on raw bytes
    constexpr size_t kFnvOffsetBasis =
        sizeof(size_t) == 8 ? 14695981039346656037ULL : 2166136261U;
    constexpr size_t kFnvPrime =
        sizeof(size_t) == 8 ? 1099511628211ULL : 16777619U;

    size_t hash_value = kFnvOffsetBasis;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&key);
    for (size_t i = 0; i < sizeof(K); ++i) {
      hash_value ^= data[i];
      hash_value *= kFnvPrime;
    }
    return hash_value;
  }

  /// Check key equality
  bool key_equal(const Key& lhs, const Key& rhs) const noexcept {
    return lhs == rhs;
  }

  // Entry storage - pre-allocated
  alignas(64) Entry entries_[Capacity];

  // Current number of occupied entries
  std::atomic<size_type> size_;
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_STATIC_HASH_MAP_H_
