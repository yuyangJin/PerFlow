// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file mpi_sampler.cpp
/// @brief MPI Performance Sampler using PAPI Library
///
/// This file implements a performance sampler for MPI programs using PAPI.
/// It uses dynamic instrumentation (LD_PRELOAD) via constructor/destructor
/// attributes to automatically collect performance data.

#define _GNU_SOURCE

#include <papi.h>
#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <signal.h>
#include <unistd.h>
#include <dlfcn.h>

// libunwind for safe call stack capture
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include "sampling/sampling.h"

using namespace perflow::sampling;

// ============================================================================
// Call Stack Capture Method Selection
// ============================================================================
// Two methods are available:
// 1. LIBUNWIND method (default, recommended) - uses libunwind library
// 2. PMU_SAMPLER method - uses captureCallStack from pmu_sampler.h
//
// To switch methods, define USE_PMU_SAMPLER_CALLSTACK before including this file
// or change the default here.
#ifndef USE_PMU_SAMPLER_CALLSTACK
#define USE_LIBUNWIND_CALLSTACK
#endif

// ============================================================================
// Configuration Constants
// ============================================================================

/// Number of PAPI events to monitor
constexpr int kNumEvents = 3;

/// Default sampling frequency in Hz (local to this file)
constexpr int kLocalDefaultSamplingFreq = 1000;

/// Maximum call stack depth to capture
constexpr size_t kMaxCallStackDepth = 100;

/// Hash map capacity for storing samples
constexpr size_t kSampleMapCapacity = 10000;

// ============================================================================
// Global State
// ============================================================================

/// MPI rank of current process (-1 if not initialized)
static int g_mpi_rank = -1;

/// Module initialization flag
static bool g_module_initialized = false;

/// PAPI event set
static int g_event_set = PAPI_NULL;

/// Sampling frequency (samples per second)
static int g_sampling_frequency = kLocalDefaultSamplingFreq;

/// Overflow threshold (cycles between samples)
static long long g_overflow_threshold = 0;

/// Number of events actually added successfully
static int g_events_added = 0;

/// Type aliases for sampling data structures
using SampleCallStack = CallStack<kMaxCallStackDepth>;
using SampleHashMap = StaticHashMap<SampleCallStack, uint64_t, kSampleMapCapacity>;

/// Sample storage
static std::unique_ptr<SampleHashMap> g_samples = nullptr;

// ============================================================================
// Helper Functions
// ============================================================================

/// Compute overflow threshold from sampling frequency
/// @param freq Desired sampling frequency in Hz
/// @return Overflow threshold in cycles
static long long compute_overflow_threshold(int freq) {
    // Assume 3.1 GHz CPU frequency
    // cycles_between_samples = cpu_freq / sampling_freq
    const double cpu_freq_ghz = 3.1;
    const double cpu_freq_hz = cpu_freq_ghz * 1e9;
    
    if (freq > 0) {
        return static_cast<long long>(cpu_freq_hz / freq);
    }
    return static_cast<long long>(cpu_freq_hz / kLocalDefaultSamplingFreq);
}

/// Check PAPI return code and print error if failed
/// @param retval PAPI return code
/// @param expected Expected return code
/// @param message Error message to print
/// @return true if successful, false otherwise
static bool check_papi(int retval, int expected, const char* message) {
    if (retval != expected) {
        fprintf(stderr, "[MPI Sampler] PAPI Error in %s: %s (code: %d)\n",
                message, PAPI_strerror(retval), retval);
        return false;
    }
    return true;
}

// ============================================================================
// Call Stack Capture
// ============================================================================

#ifdef USE_LIBUNWIND_CALLSTACK
/// Capture current call stack using libunwind (Method 1 - Recommended)
/// This is based on GetBacktrace() and my_backtrace() from demo/sampling/sampler.cpp
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
        // Store address (subtract 2 for return address adjustment as in demo code)
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

#elif defined(USE_PMU_SAMPLER_CALLSTACK)
/// Capture current call stack using PMU sampler approach (Method 2 - Alternative)
/// This uses the captureCallStack() method from include/sampling/pmu_sampler.h
/// @param stack Output call stack
/// @param max_depth Maximum stack depth to capture
/// @return Number of frames captured
/// 
/// WARNING: This method uses __builtin_frame_address which can cause
/// segmentation faults in some contexts. Use libunwind method instead.
static size_t capture_call_stack(SampleCallStack& stack, size_t max_depth) {
    stack.clear();
    
    // Use GCC/Clang built-in for basic unwinding
    // Note: This requires -fno-omit-frame-pointer for accurate results
    void* frame_ptrs[kMaxCallStackDepth];
    size_t depth = 0;
    
#define CAPTURE_FRAME(N) \
    do { \
        if (depth < max_depth) { \
            void* fp = __builtin_frame_address(N); \
            if (fp == nullptr) break; \
            void* ra = __builtin_return_address(N); \
            if (ra == nullptr) break; \
            frame_ptrs[depth++] = ra; \
        } \
    } while (0)
    
    // Capture frames (limited by compiler capabilities)
    CAPTURE_FRAME(0);
    CAPTURE_FRAME(1);
    CAPTURE_FRAME(2);
    CAPTURE_FRAME(3);
    CAPTURE_FRAME(4);
    CAPTURE_FRAME(5);
    CAPTURE_FRAME(6);
    CAPTURE_FRAME(7);
    CAPTURE_FRAME(8);
    CAPTURE_FRAME(9);
    CAPTURE_FRAME(10);
    CAPTURE_FRAME(11);
    CAPTURE_FRAME(12);
    CAPTURE_FRAME(13);
    CAPTURE_FRAME(14);
    CAPTURE_FRAME(15);
    
#undef CAPTURE_FRAME
    
    // Push captured frames onto stack
    for (size_t i = 0; i < depth; ++i) {
        stack.push(reinterpret_cast<uintptr_t>(frame_ptrs[i]));
    }
    
    return depth;
}

#else
#error "No call stack capture method defined. Define either USE_LIBUNWIND_CALLSTACK or USE_PMU_SAMPLER_CALLSTACK"
#endif

// ============================================================================
// PAPI Overflow Handler
// ============================================================================

/// PAPI overflow handler - called when performance counter overflows
/// This function must be signal-safe (no dynamic allocation, no stdio, etc.)
/// 
/// WARNING: PAPI_stop/PAPI_start are called here, which may not be fully
/// signal-safe. In production code, consider using a lock-free mechanism
/// to defer the actual stack capture to a safe context.
/// 
/// @param event_set PAPI event set
/// @param address Instruction pointer where overflow occurred
/// @param overflow_vector Bit vector indicating which events overflowed
/// @param context Signal context
static void papi_overflow_handler(int event_set, void* address,
                                   long long overflow_vector, void* context) {
    (void)address;  // Unused
    (void)context;  // Unused
    (void)overflow_vector;  // Could be used to identify which event overflowed
    
    // Stop counting to handle the overflow
    PAPI_stop(event_set, nullptr);
    
    // Capture the call stack
    SampleCallStack stack;
    capture_call_stack(stack, kMaxCallStackDepth);
    
    // Record the sample (increment count for this call stack)
    if (g_samples != nullptr) {
        g_samples->increment(stack);
    }
    
    // Restart counting
    PAPI_start(event_set);
}

// ============================================================================
// MPI Function Interception
// ============================================================================

// Original MPI_Init function pointer
static int (*real_MPI_Init)(int*, char***) = nullptr;

/// Intercepted MPI_Init to capture rank early
extern "C" int MPI_Init(int* argc, char*** argv) {
    // Call the real MPI_Init if we have the pointer
    if (real_MPI_Init == nullptr) {
        // Get the real MPI_Init function
        real_MPI_Init = (int (*)(int*, char***))dlsym(RTLD_NEXT, "MPI_Init");
        if (real_MPI_Init == nullptr) {
            fprintf(stderr, "[MPI Sampler] Error: Could not find real MPI_Init\n");
            return -1;
        }
    }
    
    // Call real MPI_Init
    int result = real_MPI_Init(argc, argv);
    
    // If successful, get our rank
    if (result == 0) {
        MPI_Comm_rank(MPI_COMM_WORLD, &g_mpi_rank);
        fprintf(stderr, "[MPI Sampler] Captured MPI rank: %d\n", g_mpi_rank);
    }
    
    return result;
}

// ============================================================================
// Initialization and Cleanup
// ============================================================================

/// Initialize PAPI and set up sampling
/// Called automatically via constructor attribute
static void initialize_sampler() __attribute__((constructor));

/// Cleanup PAPI and export data
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
            g_sampling_frequency = kLocalDefaultSamplingFreq;
        }
    }
    
    // Compute overflow threshold
    g_overflow_threshold = compute_overflow_threshold(g_sampling_frequency);
    
    fprintf(stderr, "[MPI Sampler] Initializing with frequency %d Hz (threshold: %lld cycles)\n",
            g_sampling_frequency, g_overflow_threshold);
    
    // Initialize PAPI library
    int retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT) {
        fprintf(stderr, "[MPI Sampler] PAPI library init failed: %s\n",
                PAPI_strerror(retval));
        g_module_initialized = false;
        return;
    }
    
    // Initialize thread support
    // Note: pthread_self without () is correct - we pass the function pointer
    retval = PAPI_thread_init((unsigned long (*)(void))pthread_self);
    if (!check_papi(retval, PAPI_OK, "PAPI_thread_init")) {
        g_module_initialized = false;
        return;
    }
    
    // Create event set
    g_event_set = PAPI_NULL;
    retval = PAPI_create_eventset(&g_event_set);
    if (!check_papi(retval, PAPI_OK, "PAPI_create_eventset")) {
        g_module_initialized = false;
        return;
    }
    
    // Add events to monitor - add them individually for better error handling
    int events_to_add[] = {
        PAPI_TOT_CYC,  // Total cycles (required for overflow)
        PAPI_TOT_INS,  // Total instructions
        PAPI_L1_DCM    // L1 data cache misses
    };
    
    g_events_added = 0;
    for (int i = 0; i < kNumEvents; ++i) {
        retval = PAPI_add_event(g_event_set, events_to_add[i]);
        if (retval == PAPI_OK) {
            g_events_added++;
            fprintf(stderr, "[MPI Sampler] Added event 0x%x\n", events_to_add[i]);
        } else {
            fprintf(stderr, "[MPI Sampler] Warning: Could not add event 0x%x: %s\n",
                    events_to_add[i], PAPI_strerror(retval));
        }
    }
    
    // We need at least PAPI_TOT_CYC for overflow sampling
    if (g_events_added == 0) {
        fprintf(stderr, "[MPI Sampler] Error: No events could be added\n");
        PAPI_destroy_eventset(&g_event_set);
        g_module_initialized = false;
        return;
    }
    
    fprintf(stderr, "[MPI Sampler] Successfully added %d/%d events\n",
            g_events_added, kNumEvents);
    
    // Set up overflow handler for PAPI_TOT_CYC
    retval = PAPI_overflow(g_event_set, PAPI_TOT_CYC, g_overflow_threshold, 0,
                           papi_overflow_handler);
    if (!check_papi(retval, PAPI_OK, "PAPI_overflow")) {
        PAPI_destroy_eventset(&g_event_set);
        g_module_initialized = false;
        return;
    }
    
    // Allocate sample storage
    g_samples = std::make_unique<SampleHashMap>();
    
    // Start counting
    retval = PAPI_start(g_event_set);
    if (!check_papi(retval, PAPI_OK, "PAPI_start")) {
        PAPI_destroy_eventset(&g_event_set);
        g_samples.reset();
        g_module_initialized = false;
        return;
    }
    
    fprintf(stderr, "[MPI Sampler] Initialization successful\n");
}

/// Finalize the sampler and export data
static void finalize_sampler() {
    if (!g_module_initialized) {
        return;  // Not initialized
    }
    
    fprintf(stderr, "[MPI Sampler] Finalizing sampler\n");
    
    // Use the rank captured during MPI_Init
    // Note: MPI_Finalize() may have been called already, so we can't call MPI_Comm_rank here
    if (g_mpi_rank < 0) {
        g_mpi_rank = 0;  // Default to rank 0 if rank was never captured
        fprintf(stderr, "[MPI Sampler] Warning: MPI rank was not captured, using rank 0\n");
    }
    
    // Stop counting - allocate array based on actual events added
    long long* values = new long long[g_events_added > 0 ? g_events_added : 1]();
    int retval = PAPI_stop(g_event_set, values);
    check_papi(retval, PAPI_OK, "PAPI_stop");
    
    // Print final counter values (only for events that were actually added)
    fprintf(stderr, "[MPI Sampler] Rank %d - Final counters (%d events):\n", 
            g_mpi_rank, g_events_added);
    const char* event_names[] = {"TOT_CYC", "TOT_INS", "L1_DCM"};
    for (int i = 0; i < g_events_added && i < kNumEvents; ++i) {
        fprintf(stderr, "  Counter %d (%s): %lld\n", i, event_names[i], values[i]);
    }
    
    delete[] values;
    
    // Print sample statistics
    if (g_samples != nullptr) {
        fprintf(stderr, "[MPI Sampler] Rank %d - Collected %zu unique call stacks\n",
                g_mpi_rank, g_samples->size());
        
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
            fprintf(stderr, "[MPI Sampler] Rank %d - Data exported to %s\n",
                    g_mpi_rank, exporter.filepath());
        } else {
            fprintf(stderr, "[MPI Sampler] Rank %d - Failed to export data\n",
                    g_mpi_rank);
        }
    }
    
    // Clean up PAPI
    PAPI_overflow(g_event_set, PAPI_TOT_CYC, 0, 0, papi_overflow_handler);
    PAPI_destroy_eventset(&g_event_set);
    
    // Clean up sample storage
    g_samples.reset();
    
    fprintf(stderr, "[MPI Sampler] Rank %d - Finalization complete\n", g_mpi_rank);
}
