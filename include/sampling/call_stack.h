// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_CALL_STACK_H_
#define PERFLOW_SAMPLING_CALL_STACK_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace perflow {
namespace sampling {

/// Default maximum call stack depth
constexpr size_t kDefaultMaxStackDepth = 128;

/// CallStack represents a function call stack as a sequence of return addresses.
/// The structure is designed for cache locality and efficient hashing.
///
/// Memory Layout:
/// - depth_: Current stack depth (number of valid frames)
/// - frames_: Fixed-size array of return addresses
///
/// The hash is computed lazily and cached for efficiency.
template <size_t MaxDepth = kDefaultMaxStackDepth>
class CallStack {
 public:
  using AddressType = uintptr_t;

  /// Default constructor creates an empty call stack
  CallStack() noexcept : depth_(0), hash_cached_(false), cached_hash_(0) {
    // Zero-initialize frames for deterministic behavior
    std::memset(frames_, 0, sizeof(frames_));
  }

  /// Constructor from an array of addresses
  /// @param addresses Array of return addresses (bottom to top)
  /// @param count Number of addresses in the array
  CallStack(const AddressType* addresses, size_t count) noexcept
      : depth_(count > MaxDepth ? MaxDepth : count),
        hash_cached_(false),
        cached_hash_(0) {
    std::memset(frames_, 0, sizeof(frames_));
    if (addresses != nullptr && depth_ > 0) {
      std::memcpy(frames_, addresses, depth_ * sizeof(AddressType));
    }
  }

  /// Copy constructor
  CallStack(const CallStack& other) noexcept
      : depth_(other.depth_),
        hash_cached_(other.hash_cached_),
        cached_hash_(other.cached_hash_) {
    std::memcpy(frames_, other.frames_, sizeof(frames_));
  }

  /// Copy assignment operator
  CallStack& operator=(const CallStack& other) noexcept {
    if (this != &other) {
      depth_ = other.depth_;
      hash_cached_ = other.hash_cached_;
      cached_hash_ = other.cached_hash_;
      std::memcpy(frames_, other.frames_, sizeof(frames_));
    }
    return *this;
  }

  /// Get current stack depth
  size_t depth() const noexcept { return depth_; }

  /// Get maximum stack depth
  static constexpr size_t max_depth() noexcept { return MaxDepth; }

  /// Check if the stack is empty
  bool empty() const noexcept { return depth_ == 0; }

  /// Check if the stack is full
  bool full() const noexcept { return depth_ >= MaxDepth; }

  /// Get the frame at a specific index
  /// @param index Frame index (0 = bottom of stack, depth-1 = top)
  /// @return Address at the specified index, or 0 if out of bounds
  AddressType frame(size_t index) const noexcept {
    if (index >= depth_) {
      return 0;
    }
    return frames_[index];
  }

  /// Get pointer to frame array for direct access
  const AddressType* frames() const noexcept { return frames_; }

  /// Push a new frame onto the stack
  /// @param address Return address to push
  /// @return true if successful, false if stack is full
  bool push(AddressType address) noexcept {
    if (depth_ >= MaxDepth) {
      return false;
    }
    frames_[depth_++] = address;
    hash_cached_ = false;
    return true;
  }

  /// Pop the top frame from the stack
  /// @return The popped address, or 0 if stack is empty
  AddressType pop() noexcept {
    if (depth_ == 0) {
      return 0;
    }
    hash_cached_ = false;
    return frames_[--depth_];
  }

  /// Clear the stack
  void clear() noexcept {
    depth_ = 0;
    hash_cached_ = false;
    std::memset(frames_, 0, sizeof(frames_));
  }

  /// Set the stack from an array of addresses
  /// @param addresses Array of return addresses
  /// @param count Number of addresses
  void set(const AddressType* addresses, size_t count) noexcept {
    depth_ = count > MaxDepth ? MaxDepth : count;
    hash_cached_ = false;
    std::memset(frames_, 0, sizeof(frames_));
    if (addresses != nullptr && depth_ > 0) {
      std::memcpy(frames_, addresses, depth_ * sizeof(AddressType));
    }
  }

  /// Compute hash value for the call stack
  /// Uses FNV-1a hash algorithm for good distribution
  size_t hash() const noexcept {
    if (hash_cached_) {
      return cached_hash_;
    }

    // FNV-1a hash parameters
    constexpr size_t kFnvOffsetBasis =
        sizeof(size_t) == 8 ? 14695981039346656037ULL : 2166136261U;
    constexpr size_t kFnvPrime =
        sizeof(size_t) == 8 ? 1099511628211ULL : 16777619U;

    size_t hash_value = kFnvOffsetBasis;

    // Hash the depth first
    hash_value ^= depth_;
    hash_value *= kFnvPrime;

    // Hash each frame address
    const uint8_t* data = reinterpret_cast<const uint8_t*>(frames_);
    size_t bytes = depth_ * sizeof(AddressType);
    for (size_t i = 0; i < bytes; ++i) {
      hash_value ^= data[i];
      hash_value *= kFnvPrime;
    }

    cached_hash_ = hash_value;
    hash_cached_ = true;
    return hash_value;
  }

  /// Equality comparison
  /// Uses hash comparison for efficiency - hash collisions are extremely rare
  /// with FNV-1a on call stack data
  bool operator==(const CallStack& other) const noexcept {
    if (depth_ != other.depth_) {
      return false;
    }
    return hash() == other.hash();
  }

  /// Inequality comparison
  bool operator!=(const CallStack& other) const noexcept {
    return !(*this == other);
  }

 private:
  // Current stack depth
  size_t depth_;

  // Cached hash value for efficiency
  mutable bool hash_cached_;
  mutable size_t cached_hash_;

  // Fixed-size array of frame addresses
  // Aligned for optimal cache line access
  alignas(64) AddressType frames_[MaxDepth];
};

/// Hash functor for CallStack (compatible with hash map implementations)
template <size_t MaxDepth = kDefaultMaxStackDepth>
struct CallStackHash {
  size_t operator()(const CallStack<MaxDepth>& stack) const noexcept {
    return stack.hash();
  }
};

/// Equality functor for CallStack
template <size_t MaxDepth = kDefaultMaxStackDepth>
struct CallStackEqual {
  bool operator()(const CallStack<MaxDepth>& lhs,
                  const CallStack<MaxDepth>& rhs) const noexcept {
    return lhs == rhs;
  }
};

/// RawCallStack stores raw runtime addresses along with metadata for post-processing.
/// This structure enables address resolution during the analysis phase instead of
/// at runtime, minimizing performance overhead during sampling.
///
/// Memory Layout:
/// - addresses: Raw addresses captured at runtime (library base + offset)
/// - timestamp: Nanosecond timestamp when the stack was captured
/// - map_id: Identifies which library map snapshot to use for resolution
template <size_t MaxDepth = kDefaultMaxStackDepth>
struct RawCallStack {
  CallStack<MaxDepth> addresses;  ///< Raw runtime addresses
  int64_t timestamp;              ///< Capture time in nanoseconds since epoch
  uint32_t map_id;                ///< Library map snapshot identifier

  /// Default constructor
  RawCallStack() noexcept : addresses(), timestamp(0), map_id(0) {}

  /// Constructor with parameters
  /// @param stack Call stack with raw addresses
  /// @param ts Timestamp in nanoseconds
  /// @param mid Map snapshot ID
  RawCallStack(const CallStack<MaxDepth>& stack, int64_t ts, uint32_t mid) noexcept
      : addresses(stack), timestamp(ts), map_id(mid) {}

  /// Copy constructor
  RawCallStack(const RawCallStack& other) noexcept
      : addresses(other.addresses), timestamp(other.timestamp), map_id(other.map_id) {}

  /// Copy assignment operator
  RawCallStack& operator=(const RawCallStack& other) noexcept {
    if (this != &other) {
      addresses = other.addresses;
      timestamp = other.timestamp;
      map_id = other.map_id;
    }
    return *this;
  }
};

/// ResolvedFrame represents a single stack frame resolved to (library, offset) pair
struct ResolvedFrame {
  std::string library_name;  ///< Name of the library/binary (e.g., "/lib/libc.so.6")
  uintptr_t offset;          ///< Offset within the library
  uintptr_t raw_address;     ///< Original raw address

  ResolvedFrame() noexcept : library_name(), offset(0), raw_address(0) {}

  ResolvedFrame(std::string lib, uintptr_t off, uintptr_t raw) noexcept
      : library_name(std::move(lib)), offset(off), raw_address(raw) {}
};

/// ResolvedCallStack stores a call stack with addresses resolved to (library, offset) pairs.
/// This is the output of the post-processing conversion step.
struct ResolvedCallStack {
  std::vector<ResolvedFrame> frames;  ///< Resolved stack frames
  int64_t timestamp;                  ///< Original capture timestamp
  uint32_t map_id;                    ///< Library map snapshot used for resolution

  ResolvedCallStack() noexcept : frames(), timestamp(0), map_id(0) {}

  ResolvedCallStack(int64_t ts, uint32_t mid) noexcept
      : frames(), timestamp(ts), map_id(mid) {}
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_CALL_STACK_H_
