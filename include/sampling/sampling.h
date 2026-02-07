// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

#ifndef PERFLOW_SAMPLING_SAMPLING_H_
#define PERFLOW_SAMPLING_SAMPLING_H_

/// @file sampling.h
/// @brief Main header for PerFlow sampling module
///
/// This header includes all components of the PerFlow sampling framework:
/// - CallStack: Function call stack representation
/// - StaticHashMap: Pre-allocated hash map for sample storage
/// - DataExporter/DataImporter: Data serialization
/// - PmuSampler: PMU-based sampling interface

#include "sampling/call_stack.h"
#include "sampling/data_export.h"
#include "sampling/pmu_sampler.h"
#include "sampling/static_hash_map.h"

namespace perflow {
namespace sampling {

/// Library version information
constexpr int kVersionMajor = 0;
constexpr int kVersionMinor = 1;
constexpr int kVersionPatch = 0;

/// Get library version string
inline const char* getVersionString() noexcept {
  return "0.1.0";
}

}  // namespace sampling
}  // namespace perflow

#endif  // PERFLOW_SAMPLING_SAMPLING_H_
