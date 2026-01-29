// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#include "sampling/data_export.h"
#include "sampling/library_map.h"

#include <ctime>
#include <vector>

namespace perflow {
namespace sampling {

DataResult LibraryMapExporter::exportMap(const LibraryMap& lib_map,
                                         uint32_t process_id) noexcept {
  if (filepath_[0] == '\0') {
    return DataResult::kErrorFileOpen;
  }

  file_ = std::fopen(filepath_, "wb");
  if (file_ == nullptr) {
    return DataResult::kErrorFileOpen;
  }

  // Prepare header
  LibMapFileHeader header;
  header.process_id = process_id;
  header.library_count = static_cast<uint32_t>(lib_map.size());
  header.timestamp = static_cast<uint64_t>(std::time(nullptr));

  // Write header
  if (std::fwrite(&header, sizeof(header), 1, file_) != 1) {
    close();
    return DataResult::kErrorFileWrite;
  }

  // Write each library entry
  for (const auto& lib : lib_map.libraries()) {
    LibMapEntryHeader entry_header;
    entry_header.base = lib.base;
    entry_header.end = lib.end;
    entry_header.executable = lib.executable ? 1 : 0;
    entry_header.name_length = static_cast<uint32_t>(lib.name.length());

    if (std::fwrite(&entry_header, sizeof(entry_header), 1, file_) != 1) {
      close();
      return DataResult::kErrorFileWrite;
    }

    // Write library name
    if (entry_header.name_length > 0) {
      if (std::fwrite(lib.name.c_str(), entry_header.name_length, 1, file_) != 1) {
        close();
        return DataResult::kErrorFileWrite;
      }
    }
  }

  close();
  return DataResult::kSuccess;
}

DataResult LibraryMapImporter::importMap(LibraryMap& lib_map,
                                         uint32_t* process_id) noexcept {
  if (filepath_[0] == '\0') {
    return DataResult::kErrorFileOpen;
  }

  file_ = std::fopen(filepath_, "rb");
  if (file_ == nullptr) {
    return DataResult::kErrorFileOpen;
  }

  // Read and validate header
  LibMapFileHeader header;
  if (std::fread(&header, sizeof(header), 1, file_) != 1) {
    close();
    return DataResult::kErrorFileRead;
  }

  if (header.magic != kLibMapMagic) {
    close();
    return DataResult::kErrorInvalidFormat;
  }

  if (header.version > kDataFormatVersion) {
    close();
    return DataResult::kErrorVersionMismatch;
  }

  if (process_id != nullptr) {
    *process_id = header.process_id;
  }

  // Clear existing library map
  lib_map.clear();

  // Read library entries
  for (uint32_t i = 0; i < header.library_count; ++i) {
    LibMapEntryHeader entry_header;
    if (std::fread(&entry_header, sizeof(entry_header), 1, file_) != 1) {
      close();
      return DataResult::kErrorFileRead;
    }

    // Read library name using vector for automatic memory management
    std::string lib_name;
    if (entry_header.name_length > 0) {
      // Limit name length to reasonable size to prevent memory issues
      constexpr uint32_t kMaxLibraryNameLength = 4096;
      if (entry_header.name_length > kMaxLibraryNameLength) {
        close();
        return DataResult::kErrorIntegrity;
      }

      std::vector<char> name_buffer(entry_header.name_length + 1);
      if (std::fread(name_buffer.data(), entry_header.name_length, 1, file_) != 1) {
        close();
        return DataResult::kErrorFileRead;
      }
      name_buffer[entry_header.name_length] = '\0';
      lib_name = name_buffer.data();
    }

    // Add library to map
    LibraryMap::LibraryInfo lib_info(
        lib_name,
        entry_header.base,
        entry_header.end,
        entry_header.executable != 0
    );
    lib_map.add_library(lib_info);
  }

  close();
  return DataResult::kSuccess;
}

}  // namespace sampling
}  // namespace perflow
