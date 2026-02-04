// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file mpi_sampler_timer.cpp
/// @brief MPI Performance Sampler using High-Precision Timers
///
/// This file implements a timer-based performance sampler for MPI programs
/// as an alternative to the PAPI hardware interrupt-based sampler.
/// It uses POSIX high-precision timers (timer_create) for software-based
/// sampling, making it compatible with platforms that don't support
/// hardware interrupts.
///
/// Key differences from mpi_sampler.cpp:
/// - Uses POSIX timers (timer_create/timer_settime) instead of PAPI overflow
/// - Software-based sampling instead of hardware PMU interrupts
/// - No dependency on PMU hardware support
/// - Maintains identical call stack capture and data export formats

#define _GNU_SOURCE

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

// libunwind for safe call stack capture
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include "sampling/sampling.h"
#include "sampling/library_map.h"
#include "sampling/data_export.h"
#include "sampling/mpi_init_setup.h"

using namespace perflow::sampling;

// ============================================================================
// Configuration Constants
// ============================================================================

/// Default sampling frequency in Hz
constexpr int kDefaultSamplingFreq = 1000;

/// Maximum call stack depth to capture
constexpr size_t kMaxCallStackDepth = 100;

/// Hash map capacity for storing samples
constexpr size_t kSampleMapCapacity = 10000;

/// Signal to use for timer (SIGRTMIN is recommended for POSIX timers)
#define SAMPLING_SIGNAL SIGRTMIN

// ============================================================================
// Global State
// ============================================================================

/// MPI rank is defined in mpi_init_setup.cpp and set during MPI_Init interception
extern int g_mpi_rank;

/// Module initialization flag
static bool g_module_initialized = false;

/// Sampling frequency (samples per second)
static int g_sampling_frequency = kDefaultSamplingFreq;

/// POSIX timer ID
static timer_t g_timer_id;

/// Timer created flag
static bool g_timer_created = false;

/// Type aliases for sampling data structures
using SampleCallStack = CallStack<kMaxCallStackDepth>;
using SampleHashMap = StaticHashMap<SampleCallStack, uint64_t, kSampleMapCapacity>;

/// Sample storage
static std::unique_ptr<SampleHashMap> g_samples = nullptr;

/// Library map for address resolution
static std::unique_ptr<LibraryMap> g_library_map = nullptr;

// ============================================================================
// Call Stack Capture
// ============================================================================

/// Capture current call stack using libunwind
/// This is signal-safe and based on the implementation in mpi_sampler.cpp
/// @param stack Output call stack
/// @param max_depth Maximum stack depth to capture
/// @return Number of frames captured
static size_t capture_call_stack(SampleCallStack& stack, size_t max_depth) {
    stack.clear();
    
    unw_cursor_t cursor;
    unw_context_t context;
    
    // Initialize cursor to current frame for local unwinding
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);
    
    // Unwind frames one by one, going up the frame stack
    size_t depth = 0;
    uintptr_t addresses[kMaxCallStackDepth];
    
    while (unw_step(&cursor) > 0 && depth < max_depth && depth < kMaxCallStackDepth) {
        unw_word_t pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0) {
            break;
        }
        // Store address (subtract 2 for return address adjustment)
        // This aligns with the behavior in the original mpi_sampler.cpp and demo/sampling/sampler.cpp
        // The adjustment accounts for the fact that the PC points after the call instruction
        addresses[depth] = (uintptr_t)pc - 2;
        depth++;
    }
    
    // Push captured frames onto stack (skip first frame which is this function)
    for (size_t i = 1; i < depth; ++i) {
        if (addresses[i] != 0) {
            stack.push(addresses[i]);
        }
    }
    
    return depth > 0 ? depth - 1 : 0;
}

// ============================================================================
// Timer Signal Handler
// ============================================================================

/// Timer signal handler - called when the timer expires
/// This function must be signal-safe (no dynamic allocation, no stdio, etc.)
/// 
/// This is functionally equivalent to papi_overflow_handler() in mpi_sampler.cpp
/// but is triggered by timer signals instead of hardware PMU overflow interrupts.
/// 
/// @param signum Signal number
/// @param info Signal information
/// @param context Signal context
static void timer_signal_handler(int signum, siginfo_t* info, void* context) {
    (void)signum;  // Unused
    (void)info;    // Unused
    (void)context; // Unused
    
    // Capture the call stack
    SampleCallStack stack;
    capture_call_stack(stack, kMaxCallStackDepth);
    
    // Record the sample (increment count for this call stack)
    if (g_samples != nullptr) {
        g_samples->increment(stack);
    }
}

// ============================================================================
// Initialization and Cleanup
// ============================================================================

/// Initialize timer-based sampler
/// Called automatically via constructor attribute
static void initialize_sampler() __attribute__((constructor));

/// Cleanup timer and export data
/// Called automatically via destructor attribute
static void finalize_sampler() __attribute__((destructor));

/// Initialize the sampler
static void initialize_sampler() {
    if (g_module_initialized) {
        return;  // Already initialized
    }
    
    // Mark as initialized early to prevent re-entry
    g_module_initialized = true;
    
    // Get MPI rank (must be called after MPI_Init)
    // Since we use constructor, MPI may not be initialized yet
    // We'll defer MPI rank retrieval until finalization
    g_mpi_rank = -1;
    
    // Check for environment variable to override sampling frequency
    const char* freq_env = std::getenv("PERFLOW_SAMPLING_FREQ");
    if (freq_env != nullptr) {
        g_sampling_frequency = std::atoi(freq_env);
        if (g_sampling_frequency <= 0) {
            g_sampling_frequency = kDefaultSamplingFreq;
        }
    }
    
    // Note: Cannot check MPI rank here as MPI_Init may not have been called yet
    fprintf(stderr, "[MPI Sampler Timer] Initializing with frequency %d Hz\n",
            g_sampling_frequency);
    
    // Set up signal handler for timer
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = timer_signal_handler;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SAMPLING_SIGNAL, &sa, nullptr) == -1) {
        fprintf(stderr, "[MPI Sampler Timer] Failed to set up signal handler: %s\n",
                strerror(errno));
        g_module_initialized = false;
        return;
    }
    
    // Create POSIX timer
    struct sigevent sev;
    std::memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SAMPLING_SIGNAL;
    sev.sigev_value.sival_ptr = &g_timer_id;
    
    if (timer_create(CLOCK_MONOTONIC, &sev, &g_timer_id) == -1) {
        fprintf(stderr, "[MPI Sampler Timer] Failed to create timer: %s\n",
                strerror(errno));
        g_module_initialized = false;
        return;
    }
    
    g_timer_created = true;
    
    // Calculate timer interval from sampling frequency
    // interval_ns = 1,000,000,000 / frequency
    long long interval_ns = 1000000000LL / g_sampling_frequency;
    
    // Set up timer to fire periodically
    struct itimerspec its;
    std::memset(&its, 0, sizeof(its));
    
    // Initial expiration
    its.it_value.tv_sec = interval_ns / 1000000000LL;
    its.it_value.tv_nsec = interval_ns % 1000000000LL;
    
    // Interval for periodic timer
    its.it_interval.tv_sec = interval_ns / 1000000000LL;
    its.it_interval.tv_nsec = interval_ns % 1000000000LL;
    
    if (timer_settime(g_timer_id, 0, &its, nullptr) == -1) {
        fprintf(stderr, "[MPI Sampler Timer] Failed to start timer: %s\n",
                strerror(errno));
        timer_delete(g_timer_id);
        g_timer_created = false;
        g_module_initialized = false;
        return;
    }
    
    // Allocate sample storage
    g_samples = std::make_unique<SampleHashMap>();
    
    // Capture library map snapshot
    g_library_map = std::make_unique<LibraryMap>();
    if (!g_library_map->parse_current_process()) {
        fprintf(stderr, "[MPI Sampler Timer] Warning: Failed to parse library map\n");
    } else {
        fprintf(stderr, "[MPI Sampler Timer] Captured %zu library mappings\n",
                g_library_map->size());
    }
    
    fprintf(stderr, "[MPI Sampler Timer] Initialization successful\n");
    fprintf(stderr, "[MPI Sampler Timer] Timer started with interval: %lld ns (%.2f Hz)\n",
            interval_ns, 1000000000.0 / interval_ns);
}

/// Finalize the sampler and export data
static void finalize_sampler() {
    if (!g_module_initialized) {
        return;  // Not initialized
    }
    
    fprintf(stderr, "[MPI Sampler Timer] Finalizing sampler\n");
    
    // Stop the timer
    if (g_timer_created) {
        struct itimerspec its;
        std::memset(&its, 0, sizeof(its));
        timer_settime(g_timer_id, 0, &its, nullptr);
        timer_delete(g_timer_id);
        g_timer_created = false;
    }
    
    // Use the rank captured during MPI_Init
    // Note: MPI_Finalize() may have been called already, so we can't call MPI_Comm_rank here
    if (g_mpi_rank < 0) {
        fprintf(stderr, "[MPI Sampler Timer] Warning: MPI rank was not captured, using rank -1\n");
    }
    
    // Print sample statistics
    if (g_samples != nullptr) {
        if (g_mpi_rank == 0) {
            fprintf(stderr, "[MPI Sampler Timer] Rank %d - Collected %zu unique call stacks\n",
                    g_mpi_rank, g_samples->size());
        }
        
        // Export data to file
        char output_dir[256];
        const char* dir_env = std::getenv("PERFLOW_OUTPUT_DIR");
        if (dir_env != nullptr) {
            std::strncpy(output_dir, dir_env, sizeof(output_dir) - 1);
            output_dir[sizeof(output_dir) - 1] = '\0';
        } else {
            std::strcpy(output_dir, "/tmp");
        }
        
        // Build output filename
        char output_filename[256];
        std::snprintf(output_filename, sizeof(output_filename),
                      "perflow_mpi_rank_%d", g_mpi_rank);
        
        // Export data
        DataExporter exporter(output_dir, output_filename, false);
        DataResult result = exporter.exportData(*g_samples);
        
        if (result == DataResult::kSuccess) {
            if (g_mpi_rank == 0) {
                fprintf(stderr, "[MPI Sampler Timer] Rank %d - Data exported to %s\n",
                        g_mpi_rank, exporter.filepath());
            }
        } else {
            fprintf(stderr, "[MPI Sampler Timer] Rank %d - Failed to export data\n",
                    g_mpi_rank);
        }
        
        if (g_mpi_rank == 0) {
            // Also export in human-readable text format
            DataResult text_result = exporter.exportDataText(*g_samples);
            
            if (text_result == DataResult::kSuccess) {
                fprintf(stderr, "[MPI Sampler Timer] Rank %d - Human-readable data exported to text file\n",
                        g_mpi_rank);
            } else {
                fprintf(stderr, "[MPI Sampler Timer] Rank %d - Failed to export human-readable data\n",
                        g_mpi_rank);
            }
        }
        
        // Export library map if available
        if (g_library_map != nullptr && !g_library_map->empty()) {
            LibraryMapExporter map_exporter(output_dir, output_filename);
            DataResult map_result = map_exporter.exportMap(*g_library_map,
                                                           static_cast<uint32_t>(g_mpi_rank));
            
            if (map_result == DataResult::kSuccess) {
                if (g_mpi_rank == 0) {
                    fprintf(stderr, "[MPI Sampler Timer] Rank %d - Library map exported to %s\n",
                            g_mpi_rank, map_exporter.filepath());
                }
            } else {
                fprintf(stderr, "[MPI Sampler Timer] Rank %d - Failed to export library map\n",
                        g_mpi_rank);
            }
            
            if (g_mpi_rank == 0) {
                // Also export in human-readable text format
                DataResult text_map_result = map_exporter.exportMapText(*g_library_map,
                                                                        static_cast<uint32_t>(g_mpi_rank));
                
                if (text_map_result == DataResult::kSuccess) {
                    fprintf(stderr, "[MPI Sampler Timer] Rank %d - Human-readable library map exported to text file\n",
                            g_mpi_rank);
                } else {
                    fprintf(stderr, "[MPI Sampler Timer] Rank %d - Failed to export human-readable library map\n",
                            g_mpi_rank);
                }
            }
        }
    }
    
    // Clean up sample storage
    g_samples.reset();
    
    // Clean up library map
    g_library_map.reset();
    
    if (g_mpi_rank == 0) {
        fprintf(stderr, "[MPI Sampler Timer] Finalization complete\n");
    }
}
