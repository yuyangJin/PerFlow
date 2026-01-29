// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_DATA_EXPORT_H_
#define PERFLOW_SAMPLING_DATA_EXPORT_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "sampling/call_stack.h"
#include "sampling/static_hash_map.h"

namespace perflow {
namespace sampling {

/// Magic number for identifying PerFlow data files
constexpr uint32_t kPerFlowMagic = 0x50464C57;  // "PFLW"

/// Current data format version
constexpr uint16_t kDataFormatVersion = 1;

/// Compression types supported
enum class CompressionType : uint8_t {
  kNone = 0,     // No compression
  kGzip = 1,     // gzip compression
  kZlib = 2,     // zlib compression
  kReserved = 3  // Reserved for future use
};

/// Data file header structure
/// All multi-byte fields are little-endian
struct __attribute__((packed)) DataFileHeader {
  uint32_t magic;            // Magic number (kPerFlowMagic)
  uint16_t version;          // Format version
  uint8_t compression;       // Compression type (CompressionType)
  uint8_t reserved1;         // Reserved for alignment
  uint64_t entry_count;      // Number of entries in the file
  uint32_t max_stack_depth;  // Maximum stack depth used
  uint32_t reserved2;        // Reserved for future use
  uint64_t timestamp;        // Unix timestamp when data was written
  uint8_t reserved3[32];     // Reserved space for future metadata

  DataFileHeader() noexcept
      : magic(kPerFlowMagic),
        version(kDataFormatVersion),
        compression(static_cast<uint8_t>(CompressionType::kNone)),
        reserved1(0),
        entry_count(0),
        max_stack_depth(kDefaultMaxStackDepth),
        reserved2(0),
        timestamp(0) {
    std::memset(reserved3, 0, sizeof(reserved3));
  }
};

static_assert(sizeof(DataFileHeader) == 64, "Header must be 64 bytes");

/// Entry header in the data file
struct __attribute__((packed)) DataEntryHeader {
  uint32_t stack_depth;  // Depth of the call stack
  uint32_t reserved;     // Reserved for alignment
  uint64_t count;        // Sample count for this call stack

  DataEntryHeader() noexcept : stack_depth(0), reserved(0), count(0) {}
};

static_assert(sizeof(DataEntryHeader) == 16, "Entry header must be 16 bytes");

/// Result codes for data operations
enum class DataResult {
  kSuccess = 0,
  kErrorFileOpen = 1,
  kErrorFileWrite = 2,
  kErrorFileRead = 3,
  kErrorInvalidFormat = 4,
  kErrorVersionMismatch = 5,
  kErrorCompression = 6,
  kErrorIntegrity = 7,
  kErrorMemory = 8
};

/// DataExporter handles writing performance data to files
class DataExporter {
 public:
  /// Constructor
  /// @param directory Output directory path
  /// @param filename Base filename (without extension)
  /// @param compressed Whether to use compression
  DataExporter(const char* directory, const char* filename,
               bool compressed = false) noexcept
      : compressed_(compressed), file_(nullptr) {
    // Build full path
    size_t dir_len = std::strlen(directory);
    size_t file_len = std::strlen(filename);

    if (dir_len + file_len + 16 < kMaxPathLength) {
      std::strcpy(filepath_, directory);
      if (dir_len > 0 && filepath_[dir_len - 1] != '/') {
        filepath_[dir_len++] = '/';
      }
      std::strcpy(filepath_ + dir_len, filename);
      std::strcat(filepath_, compressed ? ".pflw.gz" : ".pflw");
    } else {
      filepath_[0] = '\0';
    }
  }

  /// Destructor - closes file if open
  ~DataExporter() noexcept { close(); }

  /// Export call stack data from a hash map
  /// @tparam MaxDepth Maximum stack depth
  /// @tparam Capacity Hash map capacity
  /// @param data Hash map containing call stacks and counts
  /// @return Result code
  template <size_t MaxDepth, size_t Capacity>
  DataResult exportData(
      const StaticHashMap<CallStack<MaxDepth>, uint64_t, Capacity>& data)
      noexcept {
    if (filepath_[0] == '\0') {
      return DataResult::kErrorFileOpen;
    }

    file_ = std::fopen(filepath_, "wb");
    if (file_ == nullptr) {
      return DataResult::kErrorFileOpen;
    }

    // Prepare header
    DataFileHeader header;
    header.compression =
        static_cast<uint8_t>(compressed_ ? CompressionType::kGzip
                                         : CompressionType::kNone);
    header.entry_count = data.size();
    header.max_stack_depth = static_cast<uint32_t>(MaxDepth);
    header.timestamp = static_cast<uint64_t>(std::time(nullptr));

    // Write header
    if (std::fwrite(&header, sizeof(header), 1, file_) != 1) {
      close();
      return DataResult::kErrorFileWrite;
    }

    // Write entries
    DataResult result = DataResult::kSuccess;
    data.for_each([this, &result](const CallStack<MaxDepth>& stack,
                                  const uint64_t& count) {
      if (result != DataResult::kSuccess) {
        return;
      }

      DataEntryHeader entry_header;
      entry_header.stack_depth = static_cast<uint32_t>(stack.depth());
      entry_header.count = count;

      if (std::fwrite(&entry_header, sizeof(entry_header), 1, file_) != 1) {
        result = DataResult::kErrorFileWrite;
        return;
      }

      // Write stack frames
      size_t frame_bytes = stack.depth() * sizeof(uintptr_t);
      if (frame_bytes > 0) {
        if (std::fwrite(stack.frames(), frame_bytes, 1, file_) != 1) {
          result = DataResult::kErrorFileWrite;
          return;
        }
      }
    });

    close();
    return result;
  }

  /// Export call stack data in human-readable text format
  /// @tparam MaxDepth Maximum stack depth
  /// @tparam Capacity Hash map capacity
  /// @param data Hash map containing call stacks and counts
  /// @return Result code
  template <size_t MaxDepth, size_t Capacity>
  DataResult exportDataText(
      const StaticHashMap<CallStack<MaxDepth>, uint64_t, Capacity>& data)
      noexcept {
    if (filepath_[0] == '\0') {
      return DataResult::kErrorFileOpen;
    }

    // Build text file path by replacing extension
    char text_filepath[kMaxPathLength];
    std::strcpy(text_filepath, filepath_);
    
    // Replace .pflw or .pflw.gz with .txt
    char* ext = std::strrchr(text_filepath, '.');
    if (ext != nullptr) {
      if (std::strcmp(ext, ".gz") == 0) {
        // Handle .pflw.gz case
        *ext = '\0';
        ext = std::strrchr(text_filepath, '.');
      }
      if (ext != nullptr) {
        std::strcpy(ext, ".txt");
      }
    } else {
      std::strcat(text_filepath, ".txt");
    }

    file_ = std::fopen(text_filepath, "w");
    if (file_ == nullptr) {
      return DataResult::kErrorFileOpen;
    }

    // Write header information
    std::fprintf(file_, "# PerFlow Performance Data (Text Format)\n");
    std::fprintf(file_, "# Generated: %llu\n", 
                 static_cast<unsigned long long>(std::time(nullptr)));
    std::fprintf(file_, "# Entry count: %zu\n", data.size());
    std::fprintf(file_, "# Max stack depth: %zu\n", MaxDepth);
    std::fprintf(file_, "#\n");
    std::fprintf(file_, "# Format: [Sample Count] Call Stack (depth: N)\n");
    std::fprintf(file_, "#   Address1\n");
    std::fprintf(file_, "#   Address2\n");
    std::fprintf(file_, "#   ...\n");
    std::fprintf(file_, "#\n\n");

    // Write entries
    DataResult result = DataResult::kSuccess;
    size_t entry_num = 0;
    
    data.for_each([this, &result, &entry_num](const CallStack<MaxDepth>& stack,
                                               const uint64_t& count) {
      if (result != DataResult::kSuccess) {
        return;
      }

      entry_num++;
      
      // Write entry header with count and depth
      if (std::fprintf(file_, "[%llu] Call Stack (depth: %zu)\n",
                       static_cast<unsigned long long>(count),
                       stack.depth()) < 0) {
        result = DataResult::kErrorFileWrite;
        return;
      }

      // Write stack frames as hex addresses
      for (size_t i = 0; i < stack.depth(); ++i) {
        if (std::fprintf(file_, "  0x%016lx\n",
                         static_cast<unsigned long>(stack.frames()[i])) < 0) {
          result = DataResult::kErrorFileWrite;
          return;
        }
      }

      // Add blank line between entries for readability
      if (std::fprintf(file_, "\n") < 0) {
        result = DataResult::kErrorFileWrite;
        return;
      }
    });

    close();
    return result;
  }

  /// Get the output file path
  const char* filepath() const noexcept { return filepath_; }

 private:
  static constexpr size_t kMaxPathLength = 4096;

  bool compressed_;
  FILE* file_;
  char filepath_[kMaxPathLength];

  void close() noexcept {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }
};

/// DataImporter handles reading performance data from files
class DataImporter {
 public:
  /// Constructor
  /// @param filepath Full path to the data file
  explicit DataImporter(const char* filepath) noexcept : file_(nullptr) {
    size_t len = std::strlen(filepath);
    if (len < kMaxPathLength) {
      std::strcpy(filepath_, filepath);
    } else {
      filepath_[0] = '\0';
    }
  }

  /// Destructor - closes file if open
  ~DataImporter() noexcept { close(); }

  /// Import call stack data into a hash map
  /// @tparam MaxDepth Maximum stack depth
  /// @tparam Capacity Hash map capacity
  /// @param data Hash map to populate with call stacks and counts
  /// @return Result code
  template <size_t MaxDepth, size_t Capacity>
  DataResult importData(
      StaticHashMap<CallStack<MaxDepth>, uint64_t, Capacity>& data) noexcept {
    if (filepath_[0] == '\0') {
      return DataResult::kErrorFileOpen;
    }

    file_ = std::fopen(filepath_, "rb");
    if (file_ == nullptr) {
      return DataResult::kErrorFileOpen;
    }

    // Read and validate header
    DataFileHeader header;
    if (std::fread(&header, sizeof(header), 1, file_) != 1) {
      close();
      return DataResult::kErrorFileRead;
    }

    if (header.magic != kPerFlowMagic) {
      close();
      return DataResult::kErrorInvalidFormat;
    }

    if (header.version > kDataFormatVersion) {
      close();
      return DataResult::kErrorVersionMismatch;
    }

    // Currently only support uncompressed format
    if (header.compression != static_cast<uint8_t>(CompressionType::kNone)) {
      close();
      return DataResult::kErrorCompression;
    }

    // Read entries
    uintptr_t frame_buffer[MaxDepth];

    for (uint64_t i = 0; i < header.entry_count; ++i) {
      DataEntryHeader entry_header;
      if (std::fread(&entry_header, sizeof(entry_header), 1, file_) != 1) {
        close();
        return DataResult::kErrorFileRead;
      }

      // Validate stack depth
      if (entry_header.stack_depth > MaxDepth) {
        close();
        return DataResult::kErrorIntegrity;
      }

      // Read stack frames
      if (entry_header.stack_depth > 0) {
        size_t frame_bytes = entry_header.stack_depth * sizeof(uintptr_t);
        if (std::fread(frame_buffer, frame_bytes, 1, file_) != 1) {
          close();
          return DataResult::kErrorFileRead;
        }
      }

      // Create call stack and insert into map
      CallStack<MaxDepth> stack(frame_buffer, entry_header.stack_depth);
      if (!data.insert(stack, entry_header.count)) {
        close();
        return DataResult::kErrorMemory;
      }
    }

    close();
    return DataResult::kSuccess;
  }

  /// Get the input file path
  const char* filepath() const noexcept { return filepath_; }

  /// Read just the header from a file
  /// @param header Output header structure
  /// @return Result code
  DataResult readHeader(DataFileHeader& header) noexcept {
    if (filepath_[0] == '\0') {
      return DataResult::kErrorFileOpen;
    }

    FILE* f = std::fopen(filepath_, "rb");
    if (f == nullptr) {
      return DataResult::kErrorFileOpen;
    }

    if (std::fread(&header, sizeof(header), 1, f) != 1) {
      std::fclose(f);
      return DataResult::kErrorFileRead;
    }

    std::fclose(f);

    if (header.magic != kPerFlowMagic) {
      return DataResult::kErrorInvalidFormat;
    }

    return DataResult::kSuccess;
  }

 private:
  static constexpr size_t kMaxPathLength = 4096;

  FILE* file_;
  char filepath_[kMaxPathLength];

  void close() noexcept {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }
};

// ============================================================================
// Library Map Export/Import
// ============================================================================

/// Magic number for library map files
constexpr uint32_t kLibMapMagic = 0x504C4D50;  // "PLMP"

/// Library map file header
struct __attribute__((packed)) LibMapFileHeader {
  uint32_t magic;            // Magic number (kLibMapMagic)
  uint16_t version;          // Format version
  uint16_t reserved1;        // Reserved for alignment
  uint32_t process_id;       // Process ID or rank
  uint32_t library_count;    // Number of libraries in the map
  uint64_t timestamp;        // Unix timestamp when map was captured
  uint8_t reserved2[40];     // Reserved space for future metadata

  LibMapFileHeader() noexcept
      : magic(kLibMapMagic),
        version(kDataFormatVersion),
        reserved1(0),
        process_id(0),
        library_count(0),
        timestamp(0) {
    std::memset(reserved2, 0, sizeof(reserved2));
  }
};

static_assert(sizeof(LibMapFileHeader) == 64, "LibMap header must be 64 bytes");

/// Library entry header in map file
struct __attribute__((packed)) LibMapEntryHeader {
  uintptr_t base;         // Base address
  uintptr_t end;          // End address
  uint8_t executable;     // Whether region is executable
  uint8_t reserved1[7];   // Reserved for alignment
  uint32_t name_length;   // Length of library name
  uint32_t reserved2;     // Reserved for future use

  LibMapEntryHeader() noexcept
      : base(0), end(0), executable(0), reserved1{0}, name_length(0), reserved2(0) {}
};

static_assert(sizeof(LibMapEntryHeader) == 32, "LibMap entry header must be 32 bytes");

/// LibraryMapExporter handles writing library maps to files
class LibraryMapExporter {
 public:
  /// Constructor
  /// @param directory Output directory path
  /// @param filename Base filename (without extension)
  LibraryMapExporter(const char* directory, const char* filename) noexcept
      : file_(nullptr) {
    size_t dir_len = std::strlen(directory);
    size_t file_len = std::strlen(filename);

    if (dir_len + file_len + 8 < kMaxPathLength) {  // 8 for "/.libmap\0"
      std::strcpy(filepath_, directory);
      if (dir_len > 0 && filepath_[dir_len - 1] != '/') {
        filepath_[dir_len++] = '/';
      }
      std::strcpy(filepath_ + dir_len, filename);
      std::strcat(filepath_, ".libmap");
    } else {
      filepath_[0] = '\0';
    }
  }

  /// Destructor - closes file if open
  ~LibraryMapExporter() noexcept { close(); }

  /// Export library map to file
  /// @param lib_map Library map to export
  /// @param process_id Process ID or rank
  /// @return Result code
  DataResult exportMap(const class LibraryMap& lib_map,
                       uint32_t process_id) noexcept;

  /// Get the output file path
  const char* filepath() const noexcept { return filepath_; }

 private:
  static constexpr size_t kMaxPathLength = 4096;

  FILE* file_;
  char filepath_[kMaxPathLength];

  void close() noexcept {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }
};

/// LibraryMapImporter handles reading library maps from files
class LibraryMapImporter {
 public:
  /// Constructor
  /// @param filepath Full path to the library map file
  explicit LibraryMapImporter(const char* filepath) noexcept : file_(nullptr) {
    size_t len = std::strlen(filepath);
    if (len < kMaxPathLength) {
      std::strcpy(filepath_, filepath);
    } else {
      filepath_[0] = '\0';
    }
  }

  /// Destructor - closes file if open
  ~LibraryMapImporter() noexcept { close(); }

  /// Import library map from file
  /// @param lib_map Library map to populate
  /// @param process_id Output parameter for process ID
  /// @return Result code
  DataResult importMap(class LibraryMap& lib_map,
                       uint32_t* process_id) noexcept;

  /// Get the input file path
  const char* filepath() const noexcept { return filepath_; }

 private:
  static constexpr size_t kMaxPathLength = 4096;

  FILE* file_;
  char filepath_[kMaxPathLength];

  void close() noexcept {
    if (file_ != nullptr) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }
};

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_DATA_EXPORT_H_
