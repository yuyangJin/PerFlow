// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

/**
 * @file call_stack.cpp
 * @brief Call stack capture implementation
 *
 * Provides signal-safe call stack capture functionality using
 * backtrace and related APIs.
 */

#include "sampling/call_stack.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <cstdlib>
#include <cstring>

namespace perflow {
namespace sampling {

int CaptureCallStack(std::vector<void*>& stack, size_t max_depth) {
  stack.resize(max_depth);
  int depth = backtrace(stack.data(), static_cast<int>(max_depth));
  stack.resize(depth);
  return depth;
}

int CaptureCallStackRaw(void** buffer, size_t buffer_size) {
  return backtrace(buffer, static_cast<int>(buffer_size));
}

int ResolveSymbol(void* addr, char* symbol, size_t symbol_size) {
  if (symbol == nullptr || symbol_size == 0) {
    return -1;
  }

  Dl_info info;
  if (dladdr(addr, &info) == 0) {
    snprintf(symbol, symbol_size, "0x%lx", reinterpret_cast<unsigned long>(addr));
    return -1;
  }

  if (info.dli_sname != nullptr) {
    // Try to demangle C++ symbols
    int status;
    char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
    if (status == 0 && demangled != nullptr) {
      snprintf(symbol, symbol_size, "%s", demangled);
      free(demangled);
    } else {
      snprintf(symbol, symbol_size, "%s", info.dli_sname);
    }
    return 0;
  }

  // No symbol name, use address
  snprintf(symbol, symbol_size, "0x%lx", reinterpret_cast<unsigned long>(addr));
  return -1;
}

int ResolveCallStack(void* const* addresses, size_t count,
                     std::vector<StackFrame>& frames) {
  frames.clear();
  frames.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    StackFrame frame;
    frame.address = addresses[i];
    frame.function = nullptr;
    frame.file = nullptr;
    frame.line = 0;
    frame.offset = 0;

    Dl_info info;
    if (dladdr(addresses[i], &info) != 0) {
      frame.function = info.dli_sname;
      frame.file = info.dli_fname;
      if (info.dli_saddr != nullptr) {
        frame.offset = reinterpret_cast<uintptr_t>(addresses[i]) -
                       reinterpret_cast<uintptr_t>(info.dli_saddr);
      }
    }

    frames.push_back(frame);
  }

  return static_cast<int>(frames.size());
}

}  // namespace sampling
}  // namespace perflow
