// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_ANALYSIS_DIRECTORY_MONITOR_H_
#define PERFLOW_ANALYSIS_DIRECTORY_MONITOR_H_

#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace perflow {
namespace analysis {

/// FileInfo stores metadata about monitored files
struct FileInfo {
  std::string path;
  time_t last_modified;
  size_t last_size;
  bool processed;

  FileInfo() : path(), last_modified(0), last_size(0), processed(false) {}
  FileInfo(const std::string& p, time_t mtime, size_t size)
      : path(p), last_modified(mtime), last_size(size), processed(false) {}
};

/// FileType identifies the type of monitored file
enum class FileType {
  kUnknown = 0,
  kSampleData = 1,   // .pflw files
  kLibraryMap = 2,   // .libmap files
  kPerformanceTree = 3,  // .ptree files
  kText = 4,         // .txt files
};

/// DirectoryMonitor watches a directory for new or updated files
class DirectoryMonitor {
 public:
  /// Callback function type for file notifications
  /// Args: (file_path, file_type, is_new_file)
  using FileCallback =
      std::function<void(const std::string&, FileType, bool)>;

  /// Constructor
  /// @param directory Directory to monitor
  /// @param poll_interval_ms Polling interval in milliseconds
  explicit DirectoryMonitor(const std::string& directory,
                           uint32_t poll_interval_ms = 1000)
      : directory_(directory),
        poll_interval_ms_(poll_interval_ms),
        running_(false),
        monitor_thread_(),
        callback_(),
        file_info_(),
        mutex_() {}

  /// Destructor - stops monitoring
  ~DirectoryMonitor() { stop(); }

  // Disable copy and move
  DirectoryMonitor(const DirectoryMonitor&) = delete;
  DirectoryMonitor& operator=(const DirectoryMonitor&) = delete;

  /// Set the callback function for file notifications
  void set_callback(FileCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
  }

  /// Start monitoring
  /// @return True if started successfully
  bool start() {
    if (running_.load()) {
      return false;
    }

    running_.store(true);
    monitor_thread_ = std::thread(&DirectoryMonitor::monitor_loop, this);
    return true;
  }

  /// Stop monitoring
  void stop() {
    if (running_.load()) {
      running_.store(false);
      if (monitor_thread_.joinable()) {
        monitor_thread_.join();
      }
    }
  }

  /// Check if monitoring is active
  bool is_running() const { return running_.load(); }

  /// Get the monitored directory
  const std::string& directory() const { return directory_; }

  /// Manually scan the directory once
  void scan() {
    std::lock_guard<std::mutex> lock(mutex_);
    scan_directory();
  }

  /// Get file type from extension
  static FileType get_file_type(const std::string& path) {
    if (ends_with(path, ".pflw")) {
      return FileType::kSampleData;
    } else if (ends_with(path, ".libmap")) {
      return FileType::kLibraryMap;
    } else if (ends_with(path, ".ptree")) {
      return FileType::kPerformanceTree;
    } else if (ends_with(path, ".txt")) {
      return FileType::kText;
    }
    return FileType::kUnknown;
  }

 private:
  void monitor_loop() {
    while (running_.load()) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        scan_directory();
      }

      // Sleep for poll interval
      std::this_thread::sleep_for(
          std::chrono::milliseconds(poll_interval_ms_));
    }
  }

  void scan_directory() {
    // Use popen to list directory contents (portable approach)
    char command[4096];
    std::snprintf(command, sizeof(command), "find \"%s\" -type f 2>/dev/null",
                 directory_.c_str());

    FILE* pipe = popen(command, "r");
    if (!pipe) {
      return;
    }

    char line[4096];
    while (fgets(line, sizeof(line), pipe)) {
      // Remove trailing newline
      size_t len = std::strlen(line);
      if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
      }

      std::string filepath(line);
      check_file(filepath);
    }

    pclose(pipe);
  }

  void check_file(const std::string& filepath) {
    // Get file stats
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
      return;  // File doesn't exist or can't be accessed
    }

    // Check if it's a regular file
    if (!S_ISREG(st.st_mode)) {
      return;
    }

    // Determine file type
    FileType type = get_file_type(filepath);
    if (type == FileType::kUnknown) {
      return;  // Not a file type we're interested in
    }

    // Check if file is new or updated
    auto it = file_info_.find(filepath);
    if (it == file_info_.end()) {
      // New file
      file_info_[filepath] =
          FileInfo(filepath, st.st_mtime, st.st_size);
      if (callback_) {
        callback_(filepath, type, true);
      }
    } else {
      // Existing file - check if modified
      FileInfo& info = it->second;
      if (st.st_mtime > info.last_modified || st.st_size != info.last_size) {
        info.last_modified = st.st_mtime;
        info.last_size = st.st_size;
        info.processed = false;
        if (callback_) {
          callback_(filepath, type, false);
        }
      }
    }
  }

  static bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) {
      return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(),
                      suffix) == 0;
  }

  std::string directory_;
  uint32_t poll_interval_ms_;
  std::atomic<bool> running_;
  std::thread monitor_thread_;
  FileCallback callback_;
  std::unordered_map<std::string, FileInfo> file_info_;
  std::mutex mutex_;
};

}  // namespace analysis
}  // namespace perflow

#endif  // PERFLOW_ANALYSIS_DIRECTORY_MONITOR_H_
