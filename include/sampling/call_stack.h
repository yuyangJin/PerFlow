// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

#ifndef PERFLOW_SAMPLING_CALL_STACK_H_
#define PERFLOW_SAMPLING_CALL_STACK_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace perflow {
namespace sampling {

/**
 * @brief Maximum depth of call stack to capture
 */
constexpr size_t kMaxStackDepth = 64;

/**
 * @brief Capture the current call stack
 *
 * This function captures the current call stack up to the specified depth.
 * It is designed to be safe to call from signal handlers.
 *
 * @param stack Output vector to store the call stack addresses
 * @param max_depth Maximum number of frames to capture
 * @return Number of frames captured
 */
int CaptureCallStack(std::vector<void*>& stack, size_t max_depth = kMaxStackDepth);

/**
 * @brief Capture call stack into a pre-allocated buffer (signal-safe)
 *
 * This version is safe to call from signal handlers as it doesn't allocate.
 *
 * @param buffer Pre-allocated buffer for stack addresses
 * @param buffer_size Size of the buffer
 * @return Number of frames captured
 */
int CaptureCallStackRaw(void** buffer, size_t buffer_size);

/**
 * @brief Resolve an address to a symbol name
 *
 * @param addr Address to resolve
 * @param symbol Output buffer for symbol name
 * @param symbol_size Size of the output buffer
 * @return 0 on success, negative error code on failure
 */
int ResolveSymbol(void* addr, char* symbol, size_t symbol_size);

/**
 * @brief Call stack frame information
 */
struct StackFrame {
  void* address;           // Instruction pointer
  const char* function;    // Function name (may be null)
  const char* file;        // Source file name (may be null)
  int line;                // Source line number (0 if unknown)
  uintptr_t offset;        // Offset from function start
};

/**
 * @brief Resolve a call stack to symbol information
 *
 * @param addresses Array of addresses
 * @param count Number of addresses
 * @param frames Output array of stack frames
 * @return Number of frames successfully resolved
 */
int ResolveCallStack(void* const* addresses, size_t count,
                     std::vector<StackFrame>& frames);

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_CALL_STACK_H_
