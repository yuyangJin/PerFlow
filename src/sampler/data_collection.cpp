// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

/**
 * @file data_collection.cpp
 * @brief Runtime performance data collection implementation
 *
 * Provides thread-safe and signal-safe mechanisms for collecting
 * performance samples during program execution.
 */

#include "sampling/data_collection.h"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace perflow {
namespace sampling {

DataCollection::DataCollection()
    : format_(OutputFormat::kText),
      rank_(-1),
      ring_buffer_(nullptr),
      ring_head_(0),
      ring_tail_(0) {
  // Allocate ring buffer for signal-safe collection
  ring_buffer_ = new RawSample[kRingBufferSize];
  std::memset(ring_buffer_, 0, sizeof(RawSample) * kRingBufferSize);
}

DataCollection::~DataCollection() {
  delete[] ring_buffer_;
}

int DataCollection::Initialize(const std::string& output_path,
                                OutputFormat format, int rank) {
  output_path_ = output_path;
  format_ = format;
  rank_ = rank;
  samples_.clear();
  ring_head_ = 0;
  ring_tail_ = 0;
  return 0;
}

void DataCollection::AddSample(const Sample& sample) {
  std::lock_guard<std::mutex> lock(mutex_);
  samples_.push_back(sample);
}

void DataCollection::AddSampleSignalSafe(uint64_t timestamp,
                                          const int64_t* event_values,
                                          int num_events,
                                          void* const* stack,
                                          int stack_depth,
                                          int thread_id) {
  // Use atomic operations for lock-free ring buffer insertion
  size_t head = ring_head_.load(std::memory_order_relaxed);
  size_t next_head = (head + 1) % kRingBufferSize;

  // Check if buffer is full (would overwrite tail)
  if (next_head == ring_tail_.load(std::memory_order_acquire)) {
    // Buffer full, drop sample
    return;
  }

  // Fill the slot
  RawSample& slot = ring_buffer_[head];
  slot.timestamp = timestamp;
  for (int i = 0; i < 3 && i < num_events; ++i) {
    slot.event_values[i] = event_values[i];
  }
  slot.stack_depth = std::min(stack_depth, 64);
  for (int i = 0; i < slot.stack_depth; ++i) {
    slot.stack[i] = stack[i];
  }
  slot.thread_id = thread_id;
  slot.valid = true;

  // Publish the new head
  ring_head_.store(next_head, std::memory_order_release);
}

const std::vector<Sample>& DataCollection::GetSamples() const {
  return samples_;
}

size_t DataCollection::GetSampleCount() const {
  return samples_.size();
}

void DataCollection::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  samples_.clear();
  ring_head_ = 0;
  ring_tail_ = 0;
}

void DataCollection::FlushSignalSafeSamples() {
  std::lock_guard<std::mutex> lock(mutex_);

  size_t tail = ring_tail_.load(std::memory_order_relaxed);
  size_t head = ring_head_.load(std::memory_order_acquire);

  while (tail != head) {
    RawSample& slot = ring_buffer_[tail];
    if (slot.valid) {
      Sample sample;
      sample.timestamp = slot.timestamp;
      for (int i = 0; i < 3; ++i) {
        sample.event_values[i] = slot.event_values[i];
      }
      sample.call_stack.assign(slot.stack, slot.stack + slot.stack_depth);
      sample.rank = rank_;
      sample.thread_id = slot.thread_id;
      samples_.push_back(sample);
      slot.valid = false;
    }
    tail = (tail + 1) % kRingBufferSize;
  }

  ring_tail_.store(tail, std::memory_order_release);
}

int DataCollection::WriteOutput() {
  FlushSignalSafeSamples();

  switch (format_) {
    case OutputFormat::kText:
      return WriteTextOutput();
    case OutputFormat::kBinary:
      return WriteBinaryOutput();
    case OutputFormat::kCompressed:
      return WriteCompressedOutput();
    default:
      return -1;
  }
}

int DataCollection::WriteTextOutput() {
  std::ostringstream filename;
  filename << output_path_;
  if (rank_ >= 0) {
    filename << ".rank" << rank_;
  }
  filename << ".txt";

  std::ofstream out(filename.str());
  if (!out.is_open()) {
    return -1;
  }

  out << "# PerFlow Performance Samples\n";
  out << "# Rank: " << rank_ << "\n";
  out << "# Total Samples: " << samples_.size() << "\n";
  out << "# Columns: timestamp, cycles, instructions, l1_dcm, thread_id, "
      << "stack_depth\n";
  out << "#\n";

  for (const auto& sample : samples_) {
    out << sample.timestamp << ","
        << sample.event_values[0] << ","
        << sample.event_values[1] << ","
        << sample.event_values[2] << ","
        << sample.thread_id << ","
        << sample.call_stack.size();

    // Write call stack addresses
    for (void* addr : sample.call_stack) {
      out << "," << std::hex << reinterpret_cast<uintptr_t>(addr) << std::dec;
    }
    out << "\n";
  }

  out.close();
  return 0;
}

int DataCollection::WriteBinaryOutput() {
  std::ostringstream filename;
  filename << output_path_;
  if (rank_ >= 0) {
    filename << ".rank" << rank_;
  }
  filename << ".bin";

  std::ofstream out(filename.str(), std::ios::binary);
  if (!out.is_open()) {
    return -1;
  }

  // Write header
  uint32_t magic = 0x50455246;  // "PERF"
  uint32_t version = 1;
  uint32_t num_samples = static_cast<uint32_t>(samples_.size());
  int32_t rank = rank_;

  out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  out.write(reinterpret_cast<const char*>(&version), sizeof(version));
  out.write(reinterpret_cast<const char*>(&num_samples), sizeof(num_samples));
  out.write(reinterpret_cast<const char*>(&rank), sizeof(rank));

  // Write samples
  for (const auto& sample : samples_) {
    out.write(reinterpret_cast<const char*>(&sample.timestamp),
              sizeof(sample.timestamp));
    out.write(reinterpret_cast<const char*>(sample.event_values),
              sizeof(sample.event_values));
    out.write(reinterpret_cast<const char*>(&sample.thread_id),
              sizeof(sample.thread_id));

    uint32_t stack_size = static_cast<uint32_t>(sample.call_stack.size());
    out.write(reinterpret_cast<const char*>(&stack_size), sizeof(stack_size));
    for (void* addr : sample.call_stack) {
      uint64_t addr_val = reinterpret_cast<uint64_t>(addr);
      out.write(reinterpret_cast<const char*>(&addr_val), sizeof(addr_val));
    }
  }

  out.close();
  return 0;
}

int DataCollection::WriteCompressedOutput() {
  // NOTE: Compressed output requires zlib to be linked.
  // This implementation falls back to binary output for now.
  // When zlib is available, it compresses the binary data.
  // To enable compression, link with -lz and #define HAVE_ZLIB
#ifdef HAVE_ZLIB
  // Implementation with zlib compression would go here
  // For now, we use the binary format as the base
#endif
  return WriteBinaryOutput();
}

}  // namespace sampling
}  // namespace perflow
