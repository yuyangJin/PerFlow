// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_LIBRARY_MAP_H_
#define PERFLOW_SAMPLING_LIBRARY_MAP_H_

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace perflow {
namespace sampling {

/// LibraryMap parses and stores information about loaded libraries from /proc/self/maps.
/// This enables conversion of runtime addresses to static library offsets.
///
/// Thread Safety: Not thread-safe. External synchronization required if used from multiple threads.
class LibraryMap {
 public:
  /// Information about a single loaded library or memory region
  struct LibraryInfo {
    std::string name;      ///< Library name (e.g., "libc.so.6", "[stack]", "[heap]")
    uintptr_t base;        ///< Start address of the memory region
    uintptr_t end;         ///< End address of the memory region
    bool is_executable;    ///< Whether this region has execute permissions

    LibraryInfo() noexcept
        : name(), base(0), end(0), is_executable(false) {}

    LibraryInfo(std::string lib_name, uintptr_t start, uintptr_t finish, bool executable) noexcept
        : name(std::move(lib_name)), base(start), end(finish), is_executable(executable) {}
  };

  /// Default constructor
  LibraryMap() noexcept = default;

  /// Parse library mappings from the current process
  /// Reads and parses /proc/self/maps to build the library map
  /// @return true if parsing succeeded, false on error
  bool parse_current_process() {
    libraries_.clear();
    return parse_maps_file("/proc/self/maps");
  }

  /// Parse library mappings from a specific maps file
  /// Useful for testing or analyzing other processes
  /// @param maps_file_path Path to the maps file to parse
  /// @return true if parsing succeeded, false on error
  bool parse_maps_file(const std::string& maps_file_path) {
    std::ifstream maps_file(maps_file_path);
    if (!maps_file.is_open()) {
      return false;
    }

    std::string line;
    while (std::getline(maps_file, line)) {
      if (!parse_maps_line(line)) {
        // Continue parsing even if one line fails
        continue;
      }
    }

    return !libraries_.empty();
  }

  /// Resolve an address to its library name and offset
  /// @param addr Runtime address to resolve
  /// @return Optional pair of (library_name, offset) if address is found
  std::optional<std::pair<std::string, uintptr_t>> resolve(uintptr_t addr) const {
    for (const auto& lib : libraries_) {
      if (addr >= lib.base && addr < lib.end) {
        uintptr_t offset = addr - lib.base;
        return std::make_pair(lib.name, offset);
      }
    }
    return std::nullopt;
  }

  /// Get the number of loaded libraries/regions
  size_t size() const noexcept { return libraries_.size(); }

  /// Check if the map is empty
  bool empty() const noexcept { return libraries_.empty(); }

  /// Get all libraries
  const std::vector<LibraryInfo>& libraries() const noexcept { return libraries_; }

  /// Clear all library information
  void clear() noexcept { libraries_.clear(); }

 private:
  /// Parse a single line from /proc/self/maps
  /// Format: address permissions offset device inode pathname
  /// Example: 7f8a4c000000-7f8a4c021000 r-xp 00000000 08:01 1234567 /lib/x86_64-linux-gnu/libc.so.6
  /// @param line Line to parse
  /// @return true if parsing succeeded, false otherwise
  bool parse_maps_line(const std::string& line) {
    if (line.empty()) {
      return false;
    }

    // Parse address range
    size_t dash_pos = line.find('-');
    if (dash_pos == std::string::npos) {
      return false;
    }

    size_t space_pos = line.find(' ', dash_pos);
    if (space_pos == std::string::npos) {
      return false;
    }

    // Extract addresses
    std::string start_str = line.substr(0, dash_pos);
    std::string end_str = line.substr(dash_pos + 1, space_pos - dash_pos - 1);

    uintptr_t start_addr = 0;
    uintptr_t end_addr = 0;

    try {
      start_addr = std::stoull(start_str, nullptr, 16);
      end_addr = std::stoull(end_str, nullptr, 16);
    } catch (...) {
      return false;
    }

    // Parse permissions (second field)
    size_t perm_start = space_pos + 1;
    size_t perm_end = line.find(' ', perm_start);
    if (perm_end == std::string::npos || perm_end - perm_start < 4) {
      return false;
    }

    std::string permissions = line.substr(perm_start, 4);
    bool is_executable = (permissions[2] == 'x');

    // Only store executable regions to reduce memory usage
    if (!is_executable) {
      return false;
    }

    // Parse pathname (last field, may be empty for anonymous mappings)
    size_t pathname_pos = line.find('/');
    std::string pathname;

    if (pathname_pos != std::string::npos) {
      pathname = line.substr(pathname_pos);
      // Remove trailing whitespace
      size_t end = pathname.find_last_not_of(" \t\r\n");
      if (end != std::string::npos) {
        pathname = pathname.substr(0, end + 1);
      }
    } else {
      // Check for special regions like [stack], [heap], [vdso]
      size_t bracket_pos = line.find('[');
      if (bracket_pos != std::string::npos) {
        size_t bracket_end = line.find(']', bracket_pos);
        if (bracket_end != std::string::npos) {
          pathname = line.substr(bracket_pos, bracket_end - bracket_pos + 1);
        }
      } else {
        // Anonymous mapping without a name
        pathname = "[anon]";
      }
    }

    // Add to libraries list
    libraries_.emplace_back(pathname, start_addr, end_addr, is_executable);
    return true;
  }

  std::vector<LibraryInfo> libraries_;  ///< List of loaded libraries
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_LIBRARY_MAP_H_
