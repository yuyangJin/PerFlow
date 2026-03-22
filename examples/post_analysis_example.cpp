// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file post_analysis_example.cpp
/// @brief Post-analysis of PerFlow sampling data: hotspot, imbalance, compute/comm
///
/// Usage:
///   post_analysis_example <data_directory> [--fast] [--json <output_dir>]
///
/// Outputs:
///   1. Top 20 hottest function stacks (function, file:line, library, time%)
///   2. Inter-process imbalance analysis
///   3. Computation vs communication breakdown
///   4. JSON data for web visualization (with --json)

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <chrono>

#include "sampling/call_stack.h"
#include "sampling/data_export.h"
#include "sampling/library_map.h"
#include "sampling/static_hash_map.h"
#include "analysis/offset_converter.h"
#include "analysis/symbol_resolver.h"
#include "analysis/performance_tree.h"
#include "analysis/tree_builder.h"

using namespace perflow::sampling;
using namespace perflow::analysis;

constexpr size_t kMaxDepth = 128;
constexpr size_t kCapacity = 65536;
using CallStackType = CallStack<kMaxDepth>;
using HashMapType = StaticHashMap<CallStackType, uint64_t, kCapacity>;

// ============================================================================
// File utilities
// ============================================================================

static bool file_exists(const char* path) {
  FILE* fp = std::fopen(path, "rb");
  if (!fp) return false;
  std::fclose(fp);
  return true;
}

// ============================================================================
// Text importers (.txt / .libmap.txt)
// ============================================================================

static bool import_libmap_text(const char* libmap_txt, LibraryMap& out_map) {
  std::ifstream in(libmap_txt);
  if (!in.is_open()) return false;
  out_map.clear();

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    // Example:
    // [0x0000000000400000 - 0x000000000055c000] (0x15c000 bytes) [x] /path/to/bin
    auto lbr = line.find('[');
    auto dash = line.find(" - ");
    auto rbr = line.find(']');
    if (lbr == std::string::npos || dash == std::string::npos ||
        rbr == std::string::npos || dash < lbr || rbr < dash) {
      continue;
    }
    std::string base_s = line.substr(lbr + 1, dash - (lbr + 1));
    std::string end_s = line.substr(dash + 3, rbr - (dash + 3));

    uintptr_t base = 0, end = 0;
    try {
      base = static_cast<uintptr_t>(std::stoull(base_s, nullptr, 16));
      end = static_cast<uintptr_t>(std::stoull(end_s, nullptr, 16));
    } catch (...) {
      continue;
    }

    bool exec = (line.find("[x]") != std::string::npos);
    std::string name;
    auto x_pos = line.find("] ");
    if (x_pos != std::string::npos) {
      // try find last "] " which precedes path
      size_t last = line.rfind("] ");
      if (last != std::string::npos) name = line.substr(last + 2);
    }
    if (name.empty()) name = "[unknown]";

    out_map.add_library(LibraryMap::LibraryInfo(name, base, end, exec));
  }
  return !out_map.empty();
}

static bool import_samples_text(const char* sample_txt, HashMapType& out_data) {
  std::ifstream in(sample_txt);
  if (!in.is_open()) return false;

  // NOTE: StaticHashMap has fixed capacity. If the file contains more unique
  // stacks than kCapacity, insert() will fail.
  std::string line;
  std::vector<uintptr_t> frames;
  uint64_t current_count = 0;
  size_t current_depth = 0;
  bool in_entry = false;

  auto flush_entry = [&]() -> bool {
    if (!in_entry) return true;
    if (current_depth == 0 || frames.size() < current_depth) return true;
    CallStackType stack(frames.data(), current_depth);
    return out_data.insert(stack, current_count);
  };

  while (std::getline(in, line)) {
    if (line.empty()) {
      if (!flush_entry()) return false;
      in_entry = false;
      frames.clear();
      current_count = 0;
      current_depth = 0;
      continue;
    }
    if (line[0] == '#') continue;

    if (!line.empty() && line[0] == '[') {
      // New entry header: [count] Call Stack (depth: N)
      if (!flush_entry()) return false;
      in_entry = true;
      frames.clear();
      current_count = 0;
      current_depth = 0;

      auto rb = line.find(']');
      if (rb == std::string::npos) continue;
      std::string count_s = line.substr(1, rb - 1);
      try {
        current_count = static_cast<uint64_t>(std::stoull(count_s));
      } catch (...) {
        current_count = 0;
      }
      auto dp = line.find("depth:");
      if (dp != std::string::npos) {
        auto rp = line.find(')', dp);
        std::string depth_s;
        if (rp != std::string::npos) {
          depth_s = line.substr(dp + 6, rp - (dp + 6));
        } else {
          depth_s = line.substr(dp + 6);
        }
        // trim spaces
        while (!depth_s.empty() && (depth_s.front() == ' ' || depth_s.front() == '\t'))
          depth_s.erase(depth_s.begin());
        while (!depth_s.empty() && (depth_s.back() == ' ' || depth_s.back() == '\t'))
          depth_s.pop_back();
        try {
          current_depth = static_cast<size_t>(std::stoull(depth_s));
        } catch (...) {
          current_depth = 0;
        }
        if (current_depth > kMaxDepth) current_depth = kMaxDepth;
      }
      continue;
    }

    if (!in_entry) continue;
    // Address line: "  0x0000...."
    auto ox = line.find("0x");
    if (ox == std::string::npos) continue;
    std::string addr_s = line.substr(ox + 2);
    // stop at whitespace
    size_t sp = addr_s.find_first_of(" \t\r\n");
    if (sp != std::string::npos) addr_s = addr_s.substr(0, sp);
    uintptr_t addr = 0;
    try {
      addr = static_cast<uintptr_t>(std::stoull(addr_s, nullptr, 16));
    } catch (...) {
      continue;
    }
    frames.push_back(addr);
  }

  if (!flush_entry()) return false;
  return true;
}

// ============================================================================
// Helper: classify libraries
// ============================================================================

static bool is_communication_lib(const std::string& lib_name) {
  const char* patterns[] = {
    "libmpi", "libopen-rte", "libopen-pal", "libucx", "ucp_", "libpmix"
  };
  for (const char* p : patterns) {
    if (lib_name.find(p) != std::string::npos) return true;
  }
  return false;
}

static bool is_exclude_lib(const std::string& lib_name) {
  const char* patterns[] = {
    "libmpi", "libopen-rte", "libopen-pal", "libucx", "libpmix",
    "libc.so", "libstdc++", "libpthread", "libc++.so", "libc++abi",
    "ld-linux", "libgcc_s", "libunwind", "libperflow_mpi_sampler", "[vdso]"
  };
  for (const char* p : patterns) {
    if (lib_name.find(p) != std::string::npos) return true;
  }
  return false;
}

// ============================================================================
// Per-rank data container
// ============================================================================

struct RankData {
  int rank;
  HashMapType call_stacks;
  LibraryMap lib_map;
  bool valid = false;
  bool used_text_format = false;
};

// ============================================================================
// Parallel data loading
// ============================================================================

static std::vector<RankData> load_all_ranks_parallel(const char* data_dir,
                                                      int num_ranks,
                                                      int num_threads) {
  std::vector<RankData> all_data(num_ranks);
  std::atomic<int> loaded_count(0);

  auto load_range = [&](int start, int end) {
    for (int r = start; r < end; ++r) {
      all_data[r].rank = r;
      char data_file[512], libmap_file[512];

      // Prefer binary format (.pflw/.libmap); fallback to text (.txt/.libmap.txt)
      std::snprintf(data_file, sizeof(data_file),
                    "%s/perflow_mpi_rank_%d.pflw", data_dir, r);
      std::snprintf(libmap_file, sizeof(libmap_file),
                    "%s/perflow_mpi_rank_%d.libmap", data_dir, r);

      bool ok = false;
      if (file_exists(data_file) && file_exists(libmap_file)) {
        LibraryMapImporter map_imp(libmap_file);
        if (map_imp.importMap(all_data[r].lib_map, nullptr) == DataResult::kSuccess) {
          DataImporter importer(data_file);
          if (importer.importData(all_data[r].call_stacks) == DataResult::kSuccess) {
            ok = true;
            all_data[r].used_text_format = false;
          }
        }
      }

      if (!ok) {
        std::snprintf(data_file, sizeof(data_file),
                      "%s/perflow_mpi_rank_%d.txt", data_dir, r);
        std::snprintf(libmap_file, sizeof(libmap_file),
                      "%s/perflow_mpi_rank_%d.libmap.txt", data_dir, r);
        if (file_exists(data_file) && file_exists(libmap_file)) {
          if (import_libmap_text(libmap_file, all_data[r].lib_map) &&
              import_samples_text(data_file, all_data[r].call_stacks)) {
            ok = true;
            all_data[r].used_text_format = true;
          }
        }
      }

      if (!ok) continue;
      all_data[r].valid = true;
      loaded_count.fetch_add(1, std::memory_order_relaxed);
    }
  };

  int actual_threads = std::min(num_threads, num_ranks);
  int ranks_per_thread = (num_ranks + actual_threads - 1) / actual_threads;
  std::vector<std::future<void>> futures;

  for (int t = 0; t < actual_threads; ++t) {
    int start = t * ranks_per_thread;
    int end = std::min(start + ranks_per_thread, num_ranks);
    if (start >= num_ranks) break;
    futures.push_back(std::async(std::launch::async, load_range, start, end));
  }
  for (auto& f : futures) f.wait();

  std::cout << "  Loaded " << loaded_count.load() << " / " << num_ranks
            << " ranks using " << actual_threads << " threads\n";
  return all_data;
}

// ============================================================================
// Context-aware stack key: (library, offset) per frame for cross-process match
// ============================================================================

static std::string build_stack_key(const std::vector<ResolvedFrame>& frames) {
  std::ostringstream oss;
  for (size_t i = 0; i < frames.size(); ++i) {
    if (i > 0) oss << "||";
    oss << frames[i].library_name << "|" << std::hex << frames[i].offset << std::dec;
  }
  return oss.str();
}

// Build a node identity key using call depth + (library, offset).
// Using ResolvedFrame::offset makes it stable across processes with different
// base address.
static std::string build_node_key(int depth, const ResolvedFrame& f) {
  std::ostringstream oss;
  oss << depth << "|"
      << f.library_name << "|"
      << std::hex << f.offset << std::dec;
  return oss.str();
}

// ============================================================================
// Analysis 1: Inter-process imbalance (overall + context-aware)
// ============================================================================

static void analysis_imbalance(const std::vector<RankData>& all_data,
                                int num_ranks) {
  std::cout << "\n=== 进程间不均衡性分析 (Inter-Process Imbalance) ===\n\n";
  std::vector<uint64_t> samples_per_rank(num_ranks, 0);
  for (const auto& rd : all_data) {
    if (!rd.valid || rd.rank >= num_ranks) continue;
    uint64_t total = 0;
    rd.call_stacks.for_each([&total](const CallStackType&, const uint64_t& c) {
      total += c;
    });
    samples_per_rank[rd.rank] = total;
  }

  uint64_t sum = 0, min_s = std::numeric_limits<uint64_t>::max(), max_s = 0;
  int active_ranks = 0;
  for (uint64_t s : samples_per_rank) {
    sum += s;
    if (s > 0) {
      active_ranks++;
      if (s < min_s) min_s = s;
    }
    if (s > max_s) max_s = s;
  }
  double mean = (active_ranks > 0) ? static_cast<double>(sum) / active_ranks : 0;
  double variance = 0;
  for (uint64_t s : samples_per_rank) {
    if (s > 0) {
      double d = static_cast<double>(s) - mean;
      variance += d * d;
    }
  }
  variance = (active_ranks > 1) ? variance / (active_ranks - 1) : 0;
  double stddev = std::sqrt(variance);
  double cv = (mean > 0) ? 100.0 * stddev / mean : 0;

  std::cout << "  总采样数: " << sum << ", 有效进程数: " << active_ranks << "\n";
  std::cout << "  平均每进程: " << std::fixed << std::setprecision(1) << mean << "\n";
  std::cout << "  标准差: " << stddev << ", 变异系数(CV): " << cv << "%\n";
  std::cout << "  最小: " << (min_s == std::numeric_limits<uint64_t>::max() ? 0ULL : min_s)
            << ", 最大: " << max_s << "\n";
  if (mean > 0 && max_s > 0) {
    double imbalance = 100.0 * (max_s - min_s) / mean;
    std::cout << "  不均衡度(相对平均): " << imbalance << "%\n";
  }

  std::cout << "\n  采样数最多的 5 个 rank:\n";
  std::vector<std::pair<int, uint64_t>> ranked;
  for (int r = 0; r < num_ranks; ++r)
    ranked.emplace_back(r, samples_per_rank[r]);
  std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  for (size_t i = 0; i < std::min(size_t(5), ranked.size()); ++i) {
    double pct = (sum > 0) ? 100.0 * ranked[i].second / sum : 0;
    std::cout << "    Rank " << ranked[i].first << ": " << ranked[i].second
              << " (" << std::setprecision(1) << pct << "%)\n";
  }
  std::cout << "\n  采样数最少的 5 个 rank:\n";
  size_t start = (ranked.size() >= 5) ? ranked.size() - 5 : 0;
  for (size_t i = start; i < ranked.size(); ++i) {
    double pct = (sum > 0) ? 100.0 * ranked[i].second / sum : 0;
    std::cout << "    Rank " << ranked[i].first << ": " << ranked[i].second
              << " (" << std::setprecision(1) << pct << "%)\n";
  }
}

// ============================================================================
// Analysis 1b: Context-aware imbalance (parallel, Top 20 most unbalanced stacks)
// ============================================================================

struct ImbalanceEntry {
  std::string stack_key;
  std::vector<ResolvedFrame> frames;  // representative frames (from rank 0)
  uint64_t total_samples = 0;
  std::vector<double> pct_per_rank;   // pct of rank's total
  double cv_pct = 0;                  // coefficient of variation
};

static std::vector<ImbalanceEntry> analysis_imbalance_context_parallel(
    const std::vector<RankData>& all_data,
    OffsetConverter& converter,
    int num_ranks,
    int num_threads,
    bool use_symbols,
    size_t top_n = 20) {
  std::cout << "\n=== 上下文相关进程间均衡性分析 (Top " << top_n << ") ===\n\n";

  // Per-rank total samples
  std::vector<uint64_t> total_per_rank(num_ranks, 0);
  for (const auto& rd : all_data) {
    if (!rd.valid || rd.rank >= num_ranks) continue;
    rd.call_stacks.for_each([&](const CallStackType&, const uint64_t& c) {
      total_per_rank[rd.rank] += c;
    });
  }

  // Thread-local: map stack_key -> (samples_per_rank, representative_frames)
  using LocalMap = std::unordered_map<std::string,
      std::pair<std::vector<uint64_t>, std::vector<ResolvedFrame>>>;
  std::vector<LocalMap> thread_maps(num_threads);

  auto process_rank_range = [&](int start_rank, int end_rank, int tid) {
    LocalMap& local = thread_maps[tid];
    for (int r = start_rank; r < end_rank; ++r) {
      if (r >= static_cast<int>(all_data.size()) || !all_data[r].valid) continue;
      const auto& rd = all_data[r];
      rd.call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
        auto resolved = converter.convert(stack, rd.rank, use_symbols);
        if (resolved.empty()) return;
        // Exclude MPI/system libs for application-focused analysis
        const auto& leaf = resolved[0];
        if (is_exclude_lib(leaf.library_name)) return;

        std::string key = build_stack_key(resolved);
        auto it = local.find(key);
        if (it == local.end()) {
          std::vector<uint64_t> samples(num_ranks, 0);
          samples[r] = count;
          local[key] = std::make_pair(std::move(samples), resolved);
        } else {
          it->second.first[r] += count;
        }
      });
    }
  };

  int ranks_per_thread = (num_ranks + num_threads - 1) / num_threads;
  std::vector<std::future<void>> futures;
  for (int t = 0; t < num_threads; ++t) {
    int start = t * ranks_per_thread;
    int end = std::min(start + ranks_per_thread, num_ranks);
    if (start >= num_ranks) break;
    futures.push_back(std::async(std::launch::async, process_rank_range, start, end, t));
  }
  for (auto& f : futures) f.wait();

  // Merge thread-local maps
  std::unordered_map<std::string,
      std::pair<std::vector<uint64_t>, std::vector<ResolvedFrame>>> merged;
  for (auto& local : thread_maps) {
    for (auto& [key, val] : local) {
      auto& [samples, frames] = val;
      auto it = merged.find(key);
      if (it == merged.end()) {
        merged[key] = std::move(val);
      } else {
        for (int r = 0; r < num_ranks; ++r)
          it->second.first[r] += samples[r];
      }
    }
  }

  // Build ImbalanceEntry for each stack, compute cv
  std::vector<ImbalanceEntry> entries;
  for (auto& [key, val] : merged) {
    auto& [samples_per_rank, frames] = val;
    ImbalanceEntry e;
    e.stack_key = key;
    e.frames = std::move(frames);
    e.total_samples = 0;
    for (uint64_t s : samples_per_rank) e.total_samples += s;
    if (e.total_samples == 0) continue;

    e.pct_per_rank.resize(num_ranks);
    double sum_pct = 0;
    int active = 0;
    for (int r = 0; r < num_ranks; ++r) {
      double pct = (total_per_rank[r] > 0)
          ? 100.0 * samples_per_rank[r] / total_per_rank[r] : 0;
      e.pct_per_rank[r] = pct;
      if (total_per_rank[r] > 0) {
        sum_pct += pct;
        active++;
      }
    }
    double mean_pct = (active > 0) ? sum_pct / active : 0;
    double variance = 0;
    for (int r = 0; r < num_ranks; ++r) {
      if (total_per_rank[r] > 0) {
        double d = e.pct_per_rank[r] - mean_pct;
        variance += d * d;
      }
    }
    variance = (active > 1) ? variance / (active - 1) : 0;
    double stddev = std::sqrt(variance);
    e.cv_pct = (mean_pct > 0) ? 100.0 * stddev / mean_pct : (stddev > 0 ? 999.0 : 0);
    entries.push_back(std::move(e));
  }

  // Sort by cv_pct desc, then by total_samples desc
  std::sort(entries.begin(), entries.end(),
      [](const ImbalanceEntry& a, const ImbalanceEntry& b) {
        if (std::abs(a.cv_pct - b.cv_pct) > 0.01) return a.cv_pct > b.cv_pct;
        return a.total_samples > b.total_samples;
      });

  // Print Top N
  size_t n = std::min(top_n, entries.size());
  std::cout << "  Top " << n << " 最不均衡的函数栈 (按变异系数 CV 降序):\n\n";
  uint64_t global_total = 0;
  for (uint64_t s : total_per_rank) global_total += s;
  if (global_total == 0) global_total = 1;

  for (size_t i = 0; i < n; ++i) {
    const auto& e = entries[i];
    double total_pct = 100.0 * e.total_samples / global_total;
    std::cout << "  #" << (i + 1) << " CV=" << std::fixed << std::setprecision(1)
              << e.cv_pct << "%  总占比=" << std::setprecision(2) << total_pct
              << "%  samples=" << e.total_samples << "\n";
    for (size_t j = 0; j < e.frames.size(); ++j) {
      const auto& f = e.frames[j];
      std::cout << "    [" << j << "] ";
      if (!f.function_name.empty())
        std::cout << f.function_name;
      else
        std::cout << "0x" << std::hex << f.offset << std::dec;
      if (!f.filename.empty()) {
        std::cout << "\n         " << f.filename;
        if (f.line_number > 0) std::cout << ":" << f.line_number;
      }
      std::cout << "\n         " << f.library_name << "\n";
    }
    std::cout << "    各进程占比: ";
    for (int r = 0; r < num_ranks && r < 16; ++r)  // show first 16
      std::cout << "R" << r << "=" << std::setprecision(1) << e.pct_per_rank[r] << "% ";
    if (num_ranks > 16) std::cout << "...";
    std::cout << "\n\n";
  }
  return entries;
}

// ============================================================================
// Analysis 2: Computation vs Communication
// ============================================================================

static void analysis_compute_comm(const std::vector<RankData>& all_data,
                                   OffsetConverter& converter,
                                   bool use_symbols) {
  std::cout << "\n=== 计算与通信分析 (Computation vs Communication) ===\n\n";
  uint64_t total_comm = 0, total_comp = 0;
  for (const auto& rd : all_data) {
    if (!rd.valid) continue;
    rd.call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
      auto resolved = converter.convert(stack, rd.rank, use_symbols);
      bool in_comm = false;
      for (const auto& f : resolved) {
        if (is_communication_lib(f.library_name)) { in_comm = true; break; }
      }
      if (in_comm) total_comm += count;
      else total_comp += count;
    });
  }
  uint64_t total = total_comm + total_comp;
  if (total == 0) total = 1;
  std::cout << "  通信(MPI/UCX等): " << total_comm << " ("
            << std::fixed << std::setprecision(1)
            << 100.0 * total_comm / total << "%)\n";
  std::cout << "  计算(应用/库):   " << total_comp << " ("
            << 100.0 * total_comp / total << "%)\n";
}

// ============================================================================
// Analysis 3: Hotspot — top 20 function stacks with full symbol info
// ============================================================================

namespace {
struct CSHash {
  size_t operator()(const CallStackType& s) const noexcept { return s.hash(); }
};
struct CSEqual {
  bool operator()(const CallStackType& a, const CallStackType& b) const noexcept {
    if (a.depth() != b.depth()) return false;
    for (size_t i = 0; i < a.depth(); ++i)
      if (a.frame(i) != b.frame(i)) return false;
    return true;
  }
};
}

static void analysis_hotspot(const std::vector<RankData>& all_data,
                              OffsetConverter& converter,
                              bool use_symbols,
                              size_t top_n = 20) {
  std::cout << "\n=== 热点分析 (Hotspot Analysis, Top " << top_n << ") ===\n\n";
  std::cout << "  排除: MPI/系统/运行时库, 仅统计应用代码\n\n";

  std::map<std::string, uint64_t> lib_hotness, func_hotness;
  std::unordered_map<CallStackType, uint64_t, CSHash, CSEqual> stack_counts;
  uint64_t total_included = 0, total_excluded = 0;

  for (const auto& rd : all_data) {
    if (!rd.valid) continue;
    rd.call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
      auto resolved = converter.convert(stack, rd.rank, use_symbols);
      if (resolved.empty()) { total_excluded += count; return; }
      const auto& leaf = resolved[0];
      if (is_exclude_lib(leaf.library_name)) {
        total_excluded += count;
        return;
      }
      total_included += count;
      for (const auto& f : resolved) {
        lib_hotness[f.library_name] += count;
        if (!f.function_name.empty()) {
          std::string key = f.function_name;
          if (!f.filename.empty()) key += " @ " + f.filename;
          if (f.line_number > 0) key += ":" + std::to_string(f.line_number);
          func_hotness[key] += count;
        }
      }
      stack_counts[stack] += count;
    });
  }

  uint64_t total = total_included + total_excluded;
  std::cout << "  总采样: " << total
            << ", 纳入统计(应用): " << total_included
            << ", 排除(MPI/系统): " << total_excluded << "\n\n";

  // Top 15 hottest libraries
  std::cout << "  Top 15 热点库:\n";
  std::vector<std::pair<std::string, uint64_t>> sorted_lib(lib_hotness.begin(),
                                                            lib_hotness.end());
  std::sort(sorted_lib.begin(), sorted_lib.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  for (size_t i = 0; i < std::min(size_t(15), sorted_lib.size()); ++i) {
    double pct = (total_included > 0)
                     ? 100.0 * sorted_lib[i].second / total_included
                     : 0;
    std::string short_name = sorted_lib[i].first;
    size_t slash = short_name.rfind('/');
    if (slash != std::string::npos) short_name = short_name.substr(slash + 1);
    std::cout << "    " << (i + 1) << ". " << short_name << ": "
              << sorted_lib[i].second << " (" << std::fixed
              << std::setprecision(1) << pct << "%)\n";
  }

  // Top 20 hottest functions
  std::cout << "\n  Top " << std::min(top_n, func_hotness.size()) << " 热点函数:\n";
  std::vector<std::pair<std::string, uint64_t>> sorted_func(func_hotness.begin(),
                                                              func_hotness.end());
  std::sort(sorted_func.begin(), sorted_func.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  for (size_t i = 0; i < std::min(top_n, sorted_func.size()); ++i) {
    double pct = (total_included > 0)
                     ? 100.0 * sorted_func[i].second / total_included
                     : 0;
    std::cout << "    " << (i + 1) << ". " << sorted_func[i].first << ": "
              << sorted_func[i].second << " (" << std::fixed
              << std::setprecision(2) << pct << "%)\n";
  }

  // Top 20 hottest call stacks with full frame info
  std::vector<std::pair<CallStackType, uint64_t>> sorted_stacks(
      stack_counts.begin(), stack_counts.end());
  std::sort(sorted_stacks.begin(), sorted_stacks.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  std::cout << "\n  Top " << std::min(top_n, sorted_stacks.size())
            << " 最热点函数栈 (含函数、代码位置、时间占比):\n";
  int repr_rank = -1;
  for (const auto& rd : all_data) {
    if (rd.valid) { repr_rank = rd.rank; break; }
  }
  if (repr_rank < 0) repr_rank = 0;

  for (size_t i = 0; i < std::min(top_n, sorted_stacks.size()); ++i) {
    const auto& [stack, count] = sorted_stacks[i];
    double pct = (total_included > 0) ? 100.0 * count / total_included : 0;
    std::cout << "\n  #" << (i + 1) << " 采样数=" << count << " 占比="
              << std::fixed << std::setprecision(2) << pct << "%\n";
    auto resolved = converter.convert(stack, repr_rank, use_symbols);
    for (size_t j = 0; j < resolved.size(); ++j) {
      const auto& f = resolved[j];
      std::cout << "    [" << j << "] ";
      if (!f.function_name.empty())
        std::cout << f.function_name;
      else
        std::cout << "0x" << std::hex << f.offset << std::dec;
      if (!f.filename.empty()) {
        std::cout << "\n         " << f.filename;
        if (f.line_number > 0) std::cout << ":" << f.line_number;
      }
      std::cout << "\n         " << f.library_name << "\n";
    }
  }
}

// ============================================================================
// JSON export: performance tree for web visualization
// ============================================================================

static void write_json_string(std::ofstream& out, const std::string& s) {
  out << '"';
  for (char c : s) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\t': out << "\\t"; break;
      default: out << c;
    }
  }
  out << '"';
}

static void write_tree_node_json(std::ofstream& out,
                                  const std::shared_ptr<TreeNode>& node,
                                  uint64_t tree_total,
                                  int depth,
                                  int max_depth) {
  if (!node || depth > max_depth) return;

  std::string indent(depth * 2, ' ');
  out << indent << "{\n";
  out << indent << "  \"name\": ";
  write_json_string(out, node->frame().function_name);
  out << ",\n";
  out << indent << "  \"library\": ";
  write_json_string(out, node->frame().library_name);
  out << ",\n";
  out << indent << "  \"file\": ";
  write_json_string(out, node->frame().filename);
  out << ",\n";
  out << indent << "  \"line\": " << node->frame().line_number << ",\n";
  out << indent << "  \"total_samples\": " << node->total_samples() << ",\n";
  out << indent << "  \"self_samples\": " << node->self_samples() << ",\n";
  double pct = (tree_total > 0) ? 100.0 * node->total_samples() / tree_total : 0;
  double self_pct = (tree_total > 0) ? 100.0 * node->self_samples() / tree_total : 0;
  out << indent << "  \"total_pct\": " << std::fixed << std::setprecision(2) << pct << ",\n";
  out << indent << "  \"self_pct\": " << std::fixed << std::setprecision(2) << self_pct << ",\n";

  const auto& children = node->children();
  out << indent << "  \"children\": [";
  if (!children.empty() && depth < max_depth) {
    out << "\n";
    // Sort children by total_samples desc for better visualization
    std::vector<std::shared_ptr<TreeNode>> sorted_children(children.begin(),
                                                            children.end());
    std::sort(sorted_children.begin(), sorted_children.end(),
              [](const auto& a, const auto& b) {
                return a->total_samples() > b->total_samples();
              });
    for (size_t i = 0; i < sorted_children.size(); ++i) {
      write_tree_node_json(out, sorted_children[i], tree_total, depth + 1,
                           max_depth);
      if (i + 1 < sorted_children.size()) out << ",";
      out << "\n";
    }
    out << indent << "  ";
  }
  out << "]\n";
  out << indent << "}";
}

static bool export_tree_json(const PerformanceTree& tree,
                              const char* output_path,
                              int max_depth = 50) {
  std::ofstream out(output_path);
  if (!out.is_open()) {
    std::cerr << "  Failed to open " << output_path << " for writing\n";
    return false;
  }
  uint64_t total = tree.total_samples();
  write_tree_node_json(out, tree.root(), total, 0, max_depth);
  out << "\n";
  out.close();
  return true;
}

// ============================================================================
// JSON export: calltree + per-node per-rank imbalance (for web viewer)
// ============================================================================

using NodeRankSamplesSparse =
    std::unordered_map<std::string, std::unordered_map<int, uint64_t>>;

struct NodeImbalanceItem {
  uint64_t node_id = 0;
  std::vector<std::pair<int, double>> pct_nonzero;  // (rank, pct)
};

// Forward declaration: defined later in this file.
static void normalize_frames_topdown(std::vector<ResolvedFrame>& frames);

static void compute_node_rank_samples_sparse_parallel(
    const std::vector<RankData>& all_data, OffsetConverter& converter,
    int num_ranks, size_t num_threads, bool use_symbols, int max_depth,
    NodeRankSamplesSparse& out_node_rank_samples,
    std::vector<uint64_t>& out_total_samples_per_rank) {
  out_node_rank_samples.clear();
  out_total_samples_per_rank.assign(num_ranks, 0);

  const size_t actual_threads = std::max<size_t>(1, std::min(num_threads, all_data.size()));
  std::vector<NodeRankSamplesSparse> thread_local_maps(actual_threads);

  int ranks_per_thread = static_cast<int>(
      (num_ranks + static_cast<int>(actual_threads) - 1) / static_cast<int>(actual_threads));

  std::vector<std::future<void>> futures;
  futures.reserve(actual_threads);
  for (size_t tid = 0; tid < actual_threads; ++tid) {
    int start = static_cast<int>(tid) * ranks_per_thread;
    int end = std::min(start + ranks_per_thread, num_ranks);
    if (start >= end) continue;

    futures.push_back(std::async(std::launch::async, [&, tid, start, end]() {
      auto& local = thread_local_maps[tid];
      for (int r = start; r < end; ++r) {
        if (r >= static_cast<int>(all_data.size()) || !all_data[r].valid) continue;
        const auto& rd = all_data[r];

        rd.call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
          out_total_samples_per_rank[r] += count;

          auto frames = converter.convert(stack, rd.rank, use_symbols);
          if (frames.empty()) return;
          normalize_frames_topdown(frames);
          for (int depth = 1; depth <= max_depth && depth <= static_cast<int>(frames.size());
               ++depth) {
            const auto& f = frames[depth - 1];
            std::string key = build_node_key(depth, f);
            local[key][rd.rank] += count;
          }
        });
      }
    }));
  }
  for (auto& f : futures) f.wait();

  // Merge thread-local maps
  for (auto& local : thread_local_maps) {
    for (auto& [key, rank_map] : local) {
      auto& dst_rank_map = out_node_rank_samples[key];
      for (auto& [rank, samples] : rank_map) {
        dst_rank_map[rank] += samples;
      }
    }
  }
}

static void write_node_imbalance_json(
    const std::vector<NodeImbalanceItem>& items, int num_ranks,
    const char* output_path) {
  std::ofstream out(output_path);
  if (!out.is_open()) return;

  out << "{\n";
  out << "  \"num_ranks\": " << num_ranks << ",\n";
  out << "  \"items\": [\n";
  for (size_t i = 0; i < items.size(); ++i) {
    const auto& it = items[i];
    out << "    {\"node_id\": " << it.node_id << ", \"pct_nonzero\": [";
    for (size_t j = 0; j < it.pct_nonzero.size(); ++j) {
      const auto& [rank, pct] = it.pct_nonzero[j];
      if (j > 0) out << ", ";
      out << "[" << rank << ", " << std::fixed << std::setprecision(4) << pct << "]";
    }
    out << "]}";
    if (i + 1 < items.size()) out << ",";
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  out.close();
}

static void write_calltree_and_collect_node_imbalance(
    std::ofstream& out, const std::shared_ptr<TreeNode>& node, uint64_t& node_id_counter,
    uint64_t tree_total, int depth, int max_depth, int num_ranks,
    const NodeRankSamplesSparse& node_rank_samples,
    const std::vector<uint64_t>& total_samples_per_rank,
    std::vector<NodeImbalanceItem>& out_items) {
  if (!node || depth > max_depth) return;

  uint64_t node_id = node_id_counter++;

  std::string indent(depth * 2, ' ');
  out << indent << "{\n";
  out << indent << "  \"node_id\": " << node_id << ",\n";
  out << indent << "  \"name\": ";
  write_json_string(out, node->frame().function_name);
  out << ",\n";
  out << indent << "  \"library\": ";
  write_json_string(out, node->frame().library_name);
  out << ",\n";
  out << indent << "  \"file\": ";
  write_json_string(out, node->frame().filename);
  out << ",\n";
  out << indent << "  \"line\": " << node->frame().line_number << ",\n";
  out << indent << "  \"offset\": " << node->frame().offset << ",\n";
  out << indent << "  \"total_samples\": " << node->total_samples() << ",\n";
  out << indent << "  \"self_samples\": " << node->self_samples() << ",\n";
  double pct = (tree_total > 0) ? 100.0 * node->total_samples() / tree_total : 0;
  double self_pct = (tree_total > 0) ? 100.0 * node->self_samples() / tree_total : 0;
  out << indent << "  \"total_pct\": " << std::fixed << std::setprecision(2) << pct << ",\n";
  out << indent << "  \"self_pct\": " << std::fixed << std::setprecision(2) << self_pct << ",\n";

  // Collect per-node per-rank pct (sparse) for node_imbalance.json
  NodeImbalanceItem item;
  item.node_id = node_id;
  if (depth == 0) {
    // Root node corresponds to "everything": per-rank share is 100%.
    for (int r = 0; r < num_ranks; ++r) {
      if (total_samples_per_rank[r] == 0) continue;
      item.pct_nonzero.emplace_back(r, 100.0);
    }
  } else {
    std::string key = build_node_key(depth, node->frame());
    auto it = node_rank_samples.find(key);
    if (it != node_rank_samples.end()) {
      const auto& rank_map = it->second;
      for (const auto& [rank, samples] : rank_map) {
        if (samples == 0) continue;
        uint64_t denom = (rank >= 0 && rank < static_cast<int>(total_samples_per_rank.size()))
                            ? total_samples_per_rank[rank]
                            : 0;
        if (denom == 0) continue;
        // Per-rank percent: samples(node, rank) / total_samples(rank) * 100
        double r_pct = 100.0 * static_cast<double>(samples) /
                        static_cast<double>(denom);
        item.pct_nonzero.emplace_back(rank, r_pct);
      }
    }
  }
  out_items.push_back(std::move(item));

  const auto& children = node->children();
  out << indent << "  \"children\": [";
  if (!children.empty() && depth < max_depth) {
    out << "\n";
    // Sort children by total_samples desc for better visualization
    std::vector<std::shared_ptr<TreeNode>> sorted_children(children.begin(),
                                                            children.end());
    std::sort(sorted_children.begin(), sorted_children.end(),
              [](const auto& a, const auto& b) {
                return a->total_samples() > b->total_samples();
              });
    for (size_t i = 0; i < sorted_children.size(); ++i) {
      write_calltree_and_collect_node_imbalance(
          out, sorted_children[i], node_id_counter, tree_total, depth + 1, max_depth,
          num_ranks, node_rank_samples, total_samples_per_rank, out_items);
      if (i + 1 < sorted_children.size()) out << ",";
      out << "\n";
    }
    out << indent << "  ";
  }
  out << "]\n";
  out << indent << "}";
}

static bool export_calltree_and_node_imbalance_json(
    const PerformanceTree& tree, const char* calltree_output_path,
    const char* node_imbalance_output_path,
    const NodeRankSamplesSparse& node_rank_samples,
    const std::vector<uint64_t>& total_samples_per_rank, int num_ranks,
    int max_depth = 50) {
  std::ofstream out(calltree_output_path);
  if (!out.is_open()) {
    std::cerr << "  Failed to open " << calltree_output_path
              << " for writing\n";
    return false;
  }

  uint64_t tree_total = tree.total_samples();
  uint64_t node_id_counter = 0;
  std::vector<NodeImbalanceItem> items;
  items.reserve(1024);

  write_calltree_and_collect_node_imbalance(out, tree.root(), node_id_counter, tree_total, 0,
                                             max_depth, num_ranks, node_rank_samples,
                                             total_samples_per_rank, items);
  out << "\n";
  out.close();

  write_node_imbalance_json(items, num_ranks, node_imbalance_output_path);
  return true;
}

// ============================================================================
// JSON export: flat hotspot list for web visualization
// ============================================================================

static bool export_hotspot_json(const std::vector<RankData>& all_data,
                                 OffsetConverter& converter,
                                 const char* output_path) {
  std::map<std::string, uint64_t> func_hotness;
  uint64_t total_samples = 0;

  for (const auto& rd : all_data) {
    if (!rd.valid) continue;
    rd.call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
      total_samples += count;
      auto resolved = converter.convert(stack, rd.rank, true);
      for (const auto& f : resolved) {
        if (!f.function_name.empty() && !is_exclude_lib(f.library_name)) {
          std::string key = f.function_name + "|" + f.library_name + "|" +
                            f.filename + "|" + std::to_string(f.line_number);
          func_hotness[key] += count;
        }
      }
    });
  }

  std::vector<std::pair<std::string, uint64_t>> sorted(func_hotness.begin(),
                                                        func_hotness.end());
  std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  std::ofstream out(output_path);
  if (!out.is_open()) return false;
  out << "[\n";
  size_t limit = std::min(size_t(200), sorted.size());
  for (size_t i = 0; i < limit; ++i) {
    // Parse key: func|lib|file|line
    const std::string& key = sorted[i].first;
    size_t p1 = key.find('|');
    size_t p2 = key.find('|', p1 + 1);
    size_t p3 = key.find('|', p2 + 1);
    std::string func = key.substr(0, p1);
    std::string lib = key.substr(p1 + 1, p2 - p1 - 1);
    std::string file = key.substr(p2 + 1, p3 - p2 - 1);
    std::string line = key.substr(p3 + 1);
    double pct = (total_samples > 0)
                     ? 100.0 * sorted[i].second / total_samples
                     : 0;

    out << "  {\"function\": ";
    write_json_string(out, func);
    out << ", \"library\": ";
    write_json_string(out, lib);
    out << ", \"file\": ";
    write_json_string(out, file);
    out << ", \"line\": " << line;
    out << ", \"samples\": " << sorted[i].second;
    out << ", \"pct\": " << std::fixed << std::setprecision(2) << pct;
    out << "}";
    if (i + 1 < limit) out << ",";
    out << "\n";
  }
  out << "]\n";
  out.close();
  return true;
}

// ============================================================================
// JSON export: imbalance (Top 20) for web visualization
// ============================================================================

static bool export_imbalance_json(const std::vector<ImbalanceEntry>& entries,
                                  size_t limit,
                                  const char* output_path) {
  std::ofstream out(output_path);
  if (!out.is_open()) return false;
  out << "[\n";
  size_t n = std::min(limit, entries.size());
  for (size_t i = 0; i < n; ++i) {
    const auto& e = entries[i];
    out << "  {\"index\": " << (i + 1);
    out << ", \"cv_pct\": " << std::fixed << std::setprecision(2) << e.cv_pct;
    out << ", \"total_samples\": " << e.total_samples;
    out << ", \"frames\": [";
    for (size_t j = 0; j < e.frames.size(); ++j) {
      const auto& f = e.frames[j];
      if (j > 0) out << ", ";
      std::string func_display;
      if (f.function_name.empty()) {
        std::ostringstream oss;
        oss << "0x" << std::hex << f.offset << std::dec;
        func_display = oss.str();
      } else {
        func_display = f.function_name;
      }
      out << "{\"function\": ";
      write_json_string(out, func_display);
      out << ", \"library\": "; write_json_string(out, f.library_name);
      out << ", \"file\": "; write_json_string(out, f.filename);
      out << ", \"line\": " << f.line_number;
      out << ", \"offset\": " << f.offset << "}";
    }
    out << "], \"per_rank_pct\": [";
    for (size_t r = 0; r < e.pct_per_rank.size(); ++r) {
      if (r > 0) out << ", ";
      out << std::fixed << std::setprecision(2) << e.pct_per_rank[r];
    }
    out << "]}";
    if (i + 1 < n) out << ",";
    out << "\n";
  }
  out << "]\n";
  out.close();
  return true;
}

// ============================================================================
// Detect number of ranks from data directory
// ============================================================================

static int detect_num_ranks(const char* data_dir, int max_ranks = 1024) {
  int num_ranks = 0;
  for (int i = 0; i < max_ranks; ++i) {
    char path[512];
    std::snprintf(path, sizeof(path), "%s/perflow_mpi_rank_%d.pflw", data_dir, i);
    if (file_exists(path)) { num_ranks = i + 1; continue; }
    std::snprintf(path, sizeof(path), "%s/perflow_mpi_rank_%d.txt", data_dir, i);
    if (file_exists(path)) { num_ranks = i + 1; continue; }
    break;
  }
  return num_ranks;
}

// ============================================================================
// Normalize call stack order for PerformanceTree insertion (top-down)
// ============================================================================

static void normalize_frames_topdown(std::vector<perflow::sampling::ResolvedFrame>& frames) {
  if (frames.empty()) return;

  // OffsetConverter::convert() is typically leaf -> root in this example program,
  // while PerformanceTree expects bottom(main/_start) -> top(leaf).
  std::reverse(frames.begin(), frames.end());

  // Prefer starting from "_start" if present (trim any prefix before it).
  auto start_it = std::find_if(frames.begin(), frames.end(),
                               [](const perflow::sampling::ResolvedFrame& f) {
                                 return f.function_name == "_start";
                               });
  if (start_it != frames.end()) {
    frames.erase(frames.begin(), start_it);
  }

  // Ensure every frame has a stable name for JSON export & merging.
  for (auto& f : frames) {
    if (!f.function_name.empty()) continue;
    std::ostringstream oss;
    oss << "0x" << std::hex << f.offset << std::dec;
    f.function_name = oss.str();
  }
}

// ============================================================================
// Build performance tree from loaded RankData (parallel)
// ============================================================================

static std::unique_ptr<PerformanceTree> build_tree_from_rankdata_parallel(
    const std::vector<RankData>& all_data,
    OffsetConverter& converter,
    size_t num_threads,
    double time_per_sample_us = 1000.0) {
  // Thread-local merge gives stable results with no contention during build.
  auto tree = std::make_unique<PerformanceTree>(
      TreeBuildMode::kContextFree, SampleCountMode::kBoth,
      ConcurrencyModel::kThreadLocalMerge);
  tree->set_process_count(all_data.size());

  // Prepare work list of valid ranks
  std::vector<int> ranks;
  ranks.reserve(all_data.size());
  for (const auto& rd : all_data) {
    if (rd.valid) ranks.push_back(rd.rank);
  }
  if (ranks.empty()) return tree;

  size_t actual_threads = std::min(num_threads, ranks.size());
  if (actual_threads == 0) actual_threads = 1;
  size_t ranks_per_thread = (ranks.size() + actual_threads - 1) / actual_threads;

  std::vector<std::unique_ptr<PerformanceTree>> locals;
  locals.reserve(actual_threads);
  for (size_t t = 0; t < actual_threads; ++t) {
    locals.push_back(tree->create_thread_local_tree());
  }

  std::vector<std::future<void>> futures;
  for (size_t t = 0; t < actual_threads; ++t) {
    size_t start = t * ranks_per_thread;
    size_t end = std::min(start + ranks_per_thread, ranks.size());
    if (start >= ranks.size()) break;

    futures.push_back(std::async(std::launch::async, [&, t, start, end]() {
      PerformanceTree& local = *locals[t];
      for (size_t idx = start; idx < end; ++idx) {
        int r = ranks[idx];
        const auto& rd = all_data[r];
        rd.call_stacks.for_each([&](const CallStackType& stack, const uint64_t& count) {
          bool resolve_symbols = converter.has_symbol_resolver();
          auto frames = converter.convert(stack, r, resolve_symbols);
          normalize_frames_topdown(frames);
          double total_time = static_cast<double>(count) * time_per_sample_us;
          local.insert_call_stack(frames, r, count, total_time);
        });
      }
    }));
  }
  for (auto& f : futures) f.wait();

  for (auto& lt : locals) tree->merge_thread_local_tree(*lt);
  return tree;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
  auto t_start = std::chrono::steady_clock::now();
  std::cout << "PerFlow Post-Analysis\n";
  std::cout << "=====================\n\n";

  const char* data_dir = "/tmp";
  bool fast_mode = false;
  const char* json_dir = nullptr;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--fast") == 0) {
      fast_mode = true;
    } else if (std::strcmp(argv[i], "--json") == 0 && i + 1 < argc) {
      json_dir = argv[++i];
    } else if (argv[i][0] != '-') {
      data_dir = argv[i];
    }
  }

  int hw_threads = static_cast<int>(std::thread::hardware_concurrency());
  if (hw_threads <= 0) hw_threads = 4;
  int num_threads = std::min(hw_threads, 16);

  std::cout << "Data directory: " << data_dir << "\n";
  if (fast_mode) std::cout << "Mode: fast (skip symbol resolution)\n";
  if (json_dir) std::cout << "JSON output: " << json_dir << "\n";
  std::cout << "Threads: " << num_threads << "\n\n";

  // 1. Detect ranks
  int num_ranks = detect_num_ranks(data_dir);
  if (num_ranks == 0) {
    std::cerr << "Error: no perflow_mpi_rank_*.pflw files found in "
              << data_dir << "\n";
    return 1;
  }
  std::cout << "Detected " << num_ranks << " rank(s)\n";

  // 2. Parallel load all rank data
  std::cout << "\n--- Loading data (parallel) ---\n";
  auto t_load_start = std::chrono::steady_clock::now();
  std::vector<RankData> all_data =
      load_all_ranks_parallel(data_dir, num_ranks, num_threads);
  auto t_load_end = std::chrono::steady_clock::now();
  double load_sec = std::chrono::duration<double>(t_load_end - t_load_start).count();
  std::cout << "  Load time: " << std::fixed << std::setprecision(2)
            << load_sec << "s\n";

  // 3. Create OffsetConverter with SymbolResolver
  bool use_symbols = !fast_mode;
  auto resolver = std::make_shared<SymbolResolver>(
      SymbolResolver::Strategy::kAutoFallback, true);
  OffsetConverter converter(resolver);
  for (const auto& rd : all_data) {
    if (rd.valid) converter.add_map_snapshot(rd.rank, rd.lib_map);
  }

  // 4. Run analyses
  std::cout << "\n--- Running analyses ---\n";
  auto t_analysis_start = std::chrono::steady_clock::now();

  analysis_imbalance(all_data, num_ranks);
  auto imbalance_entries = analysis_imbalance_context_parallel(
      all_data, converter, num_ranks, num_threads, use_symbols, 20);
  analysis_compute_comm(all_data, converter, use_symbols);
  analysis_hotspot(all_data, converter, use_symbols, 20);

  auto t_analysis_end = std::chrono::steady_clock::now();
  double analysis_sec =
      std::chrono::duration<double>(t_analysis_end - t_analysis_start).count();
  std::cout << "\n  Analysis time: " << std::fixed << std::setprecision(2)
            << analysis_sec << "s\n";

  // 5. Build performance tree (parallel) + export JSON
  if (json_dir) {
    std::cout << "\n--- Building performance tree (parallel) ---\n";
    auto t_tree_start = std::chrono::steady_clock::now();

    // Build from already-loaded data (works for both binary + text inputs)
    auto tree = build_tree_from_rankdata_parallel(
        all_data, converter, static_cast<size_t>(num_threads), 1000.0);

    auto t_tree_end = std::chrono::steady_clock::now();
    double tree_sec =
        std::chrono::duration<double>(t_tree_end - t_tree_start).count();
    std::cout << "  Built tree from " << num_ranks << " rank(s), "
              << (tree ? tree->total_samples() : 0) << " total samples\n";
    std::cout << "  Tree build time: " << std::fixed << std::setprecision(2)
              << tree_sec << "s\n";

    // Export JSON
    char tree_json[512], hotspot_json[512];
    std::snprintf(tree_json, sizeof(tree_json), "%s/calltree.json", json_dir);
    std::snprintf(hotspot_json, sizeof(hotspot_json), "%s/hotspot.json",
                  json_dir);

    std::cout << "\n--- Exporting JSON ---\n";
    char node_imbalance_json[512];
    std::snprintf(node_imbalance_json, sizeof(node_imbalance_json),
                  "%s/node_imbalance.json", json_dir);

    if (tree) {
      std::cout << "\n--- Building node imbalance map ---\n";
      NodeRankSamplesSparse node_rank_samples;
      std::vector<uint64_t> total_samples_per_rank;
      // Note: max_depth must match export_calltree_and_node_imbalance_json()
      compute_node_rank_samples_sparse_parallel(
          all_data, converter, num_ranks, static_cast<size_t>(num_threads),
          use_symbols, /*max_depth*/ 50, node_rank_samples,
          total_samples_per_rank);

      if (export_calltree_and_node_imbalance_json(
              *tree, tree_json, node_imbalance_json, node_rank_samples,
              total_samples_per_rank, num_ranks, /*max_depth*/ 50)) {
        std::cout << "  Call tree:   " << tree_json << "\n";
        std::cout << "  Node imbalance: " << node_imbalance_json << "\n";
      }
    }
    if (export_hotspot_json(all_data, converter, hotspot_json)) {
      std::cout << "  Hotspot:    " << hotspot_json << "\n";
    }
    char imbalance_json[512];
    std::snprintf(imbalance_json, sizeof(imbalance_json), "%s/imbalance.json",
                  json_dir);
    if (export_imbalance_json(imbalance_entries, 20, imbalance_json)) {
      std::cout << "  Imbalance:  " << imbalance_json << "\n";
    }
  }

  auto t_end = std::chrono::steady_clock::now();
  double total_sec = std::chrono::duration<double>(t_end - t_start).count();
  std::cout << "\nTotal time: " << std::fixed << std::setprecision(2)
            << total_sec << "s\n";

  std::cout << "\nUsage: " << (argc > 0 ? argv[0] : "post_analysis_example")
            << " <data_dir> [--fast] [--json <output_dir>]\n";
  return 0;
}
