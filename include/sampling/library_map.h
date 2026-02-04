// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_LIBRARY_MAP_H_
#define PERFLOW_SAMPLING_LIBRARY_MAP_H_

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace perflow {
namespace sampling {

/// LibraryMap parses and stores memory mappings from /proc/self/maps
/// This is used to convert absolute addresses to library-relative offsets
class LibraryMap {
 public:
  /// Information about a loaded library/region
  struct LibraryInfo {
    std::string name;     // Library name or path (e.g., "/lib/libc.so.6")
    uintptr_t base;       // Start address of the mapping
    uintptr_t end;        // End address of the mapping
    bool executable;      // Whether the region is executable
    
    LibraryInfo() noexcept : name(), base(0), end(0), executable(false) {}
    
    LibraryInfo(const std::string& n, uintptr_t b, uintptr_t e, bool exec) noexcept
        : name(n), base(b), end(e), executable(exec) {}
  };

  /// Default constructor
  LibraryMap() noexcept : libraries_() {}

  /// Parse /proc/self/maps for the current process
  /// @return true if successful, false on error
  bool parse_current_process() noexcept {
    return parse_maps_file("/proc/self/maps");
  }

  /// Parse a specific maps file (useful for testing or analyzing other processes)
  /// @param filepath Path to the maps file
  /// @return true if successful, false on error
  bool parse_maps_file(const char* filepath) noexcept {
    libraries_.clear();

    FILE* maps_file = std::fopen(filepath, "r");
    if (maps_file == nullptr) {
      return false;
    }

    char line[4096];
    while (std::fgets(line, sizeof(line), maps_file) != nullptr) {
      if (!parse_maps_line(line)) {
        // Continue parsing even if one line fails
        continue;
      }
    }

    std::fclose(maps_file);
    return true;
  }

  /// Resolve an address to a library and offset
  /// @param addr Address to resolve
  /// @return Optional pair of (library_name, offset) if found
  std::optional<std::pair<std::string, uintptr_t>> resolve(
      uintptr_t addr) const noexcept {
    // Search for the library containing this address
    for (const auto& lib : libraries_) {
      if (addr >= lib.base && addr < lib.end) {
        // Only return executable regions for meaningful stack traces
        if (lib.executable) {
          uintptr_t offset;
          // For static base addresses (typical for non-PIE executables),
          // the offset should equal the raw address.
          // For dynamic base addresses (typical for shared libraries with ASLR),
          // the offset should be calculated as raw_address - base_address.
          // Threshold of 0x10000000 (256 MB) distinguishes static from dynamic.
          if (lib.base < kStaticBaseAddressThreshold) {
            // Static base address - offset equals raw address
            offset = addr;
          } else {
            // Dynamic base address - calculate offset
            offset = addr - lib.base;
          }
          return std::make_pair(lib.name, offset);
        }
      }
    }
    return std::nullopt;
  }

  /// Get all loaded libraries
  const std::vector<LibraryInfo>& libraries() const noexcept {
    return libraries_;
  }

  /// Get number of loaded libraries
  size_t size() const noexcept { return libraries_.size(); }

  /// Check if map is empty
  bool empty() const noexcept { return libraries_.empty(); }

  /// Clear all library information
  void clear() noexcept { libraries_.clear(); }

  /// Add a library entry manually (used for importing from files)
  /// @param lib Library information to add
  void add_library(const LibraryInfo& lib) {
    libraries_.push_back(lib);
  }

 private:
  std::vector<LibraryInfo> libraries_;

  /// Threshold to distinguish static vs dynamic base addresses.
  /// Base addresses below this threshold are considered static (non-PIE executables),
  /// where the offset should equal the raw address.
  /// Base addresses at or above this threshold are considered dynamic (shared libraries with ASLR),
  /// where the offset should be calculated as raw_address - base_address.
  /// 0x10000000 (256 MB) is a reasonable threshold that separates typical non-PIE
  /// executable addresses (0x400000-0x600000) from ASLR addresses (0x7f...).
  static constexpr uintptr_t kStaticBaseAddressThreshold = 0x10000000UL;

  /// Maximum pathname length for safety
  static constexpr size_t kMaxPathnameLength = 2047;  // 2048 - 1 for null terminator

  /// Parse a single line from /proc/self/maps
  /// Format: address perms offset dev inode pathname
  /// Example: 7f8a1c000000-7f8a1c021000 r-xp 00000000 08:01 12345 /lib/libc.so.6
  /// @param line Line to parse
  /// @return true if line was successfully parsed and handled, false if malformed
  bool parse_maps_line(const char* line) noexcept {
    uintptr_t start_addr = 0;
    uintptr_t end_addr = 0;
    char perms[8] = {0};
    char pathname[kMaxPathnameLength + 1] = {0};

    // Parse the address range and permissions
    // Format: start-end perms offset dev inode pathname
    int fields = std::sscanf(line, "%lx-%lx %7s %*x %*x:%*x %*d %2047[^\n]",
                             &start_addr, &end_addr, perms, pathname);

    if (fields < 3) {
      return false;  // Invalid line format
    }

    // Check if executable permission is set
    bool executable = (perms[2] == 'x');

    // Only store executable regions (relevant for stack traces)
    if (!executable) {
      return true;  // Line was valid but skipped (non-executable)
    }

    // Extract library name from pathname
    std::string lib_name;
    if (fields >= 4 && pathname[0] != '\0') {
      // Trim leading/trailing whitespace
      const char* start = pathname;
      while (*start == ' ' || *start == '\t') ++start;
      
      const char* end = start + std::strlen(start) - 1;
      while (end > start && (*end == ' ' || *end == '\t' || *end == '\n')) --end;
      
      lib_name = std::string(start, end - start + 1);
    } else {
      // Anonymous mapping or special region
      lib_name = "[anonymous]";
    }

    // Add the library to our list
    libraries_.emplace_back(lib_name, start_addr, end_addr, executable);
    return true;
  }
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_LIBRARY_MAP_H_
