// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file mpi_sampler_cycle_counter.cpp
/// @brief MPI Performance Sampler using ARM Cycle Counter
///
/// This file implements a cycle counter-based performance sampler for MPI programs
/// as an alternative to the POSIX timer-based sampler. It uses the ARMv9 cycle
/// counter register (cntvct_el0) for high-precision sampling based on fixed
/// cycle intervals.
///
/// Key features:
/// - Uses ARM cycle counter (cntvct_el0) for precise timing on ARM architectures
/// - Combines with POSIX timer for periodic checking of cycle threshold
/// - Provides fallback to pure POSIX timer on non-ARM architectures
/// - Maintains identical call stack capture and data export formats
///
/// Architecture support:
/// - Primary: ARMv9 (and compatible ARMv8.x) architectures
/// - Fallback: Any POSIX-compliant system (uses POSIX timer only)
///
/// Environment variables:
/// - PERFLOW_SAMPLING_FREQ: Sampling frequency in Hz (default: 1000)
/// - PERFLOW_OUTPUT_DIR: Output directory for sampling data (default: /tmp)
/// - PERFLOW_TIMER_METHOD: Timer method selection
///   - "cycle": Use cycle counter (ARM only, with automatic fallback)
///   - "posix": Use POSIX timer only
///   - "auto": Auto-detect based on architecture (default)

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
#include <atomic>

// libunwind for safe call stack capture
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include "sampling/sampling.h"
#include "sampling/library_map.h"
#include "sampling/data_export.h"
#include "sampling/mpi_init_setup.h"

using namespace perflow::sampling;

// ============================================================================
// Architecture Detection
// ============================================================================

/// Check if we're running on ARM architecture
#if defined(__aarch64__) || defined(__ARM_ARCH)
#define PERFLOW_ARM_ARCH 1
#else
#define PERFLOW_ARM_ARCH 0
#endif

// ============================================================================
// Cycle Counter Functions (ARM-specific)
// ============================================================================

#if PERFLOW_ARM_ARCH

/// Read the ARM cycle counter register (cntvct_el0)
/// This provides a high-resolution timestamp based on the system counter.
/// @return Current cycle counter value
static inline uint64_t read_cycle_counter() {
    uint64_t counter;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(counter));
    return counter;
}

/// Read the ARM counter frequency register (cntfrq_el0)
/// This provides the frequency at which cntvct_el0 increments.
/// @return Counter frequency in Hz
static inline uint64_t read_counter_frequency() {
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

#else

/// Fallback for non-ARM architectures: use clock_gettime
/// @return Current time in nanoseconds
static inline uint64_t read_cycle_counter() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + 
           static_cast<uint64_t>(ts.tv_nsec);
}

/// Fallback frequency for non-ARM: 1GHz (nanosecond resolution)
/// @return 1,000,000,000 (nanoseconds per second)
static inline uint64_t read_counter_frequency() {
    return 1000000000ULL;
}

#endif

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

/// Timer check interval in microseconds (how often we check the cycle counter)
/// Using 100 microseconds for good balance between precision and overhead
constexpr long kTimerCheckIntervalUs = 100;

// ============================================================================
// Timer Method Enum
// ============================================================================

/// Timer method selection
enum class TimerMethod {
    kAuto,    ///< Auto-detect based on architecture
    kCycle,   ///< Use cycle counter (ARM only)
    kPosix    ///< Use POSIX timer only
};

// ============================================================================
// Global State
// ============================================================================

/// MPI rank is defined in mpi_init_setup.cpp and set during MPI_Init interception
extern int g_mpi_rank;

/// Module initialization flag
static bool g_module_initialized = false;

/// Sampling frequency (samples per second)
static int g_sampling_frequency = kDefaultSamplingFreq;

/// Selected timer method
static TimerMethod g_timer_method = TimerMethod::kAuto;

/// Whether ARM cycle counter is available
static bool g_arm_cycle_counter_available = false;

/// Counter frequency (from cntfrq_el0 or fallback)
static uint64_t g_counter_frequency = 0;

/// Cycle threshold for sampling (cycles between samples)
static uint64_t g_cycle_threshold = 0;

/// Last sample cycle counter value
static std::atomic<uint64_t> g_last_sample_counter{0};

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
/// This is signal-safe and based on the implementation in mpi_sampler_timer.cpp
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
        // This aligns with the behavior in the original mpi_sampler_timer.cpp
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
/// This function checks if enough cycles have elapsed since the last sample.
/// It uses the cycle counter for precise timing on ARM, or falls back to
/// time-based sampling on other architectures.
/// 
/// This function must be signal-safe (no dynamic allocation, no stdio, etc.)
/// 
/// @param signum Signal number
/// @param info Signal information
/// @param context Signal context
static void timer_signal_handler(int signum, siginfo_t* info, void* context) {
    (void)signum;  // Unused
    (void)info;    // Unused
    (void)context; // Unused
    
    // Read current cycle counter
    uint64_t current_counter = read_cycle_counter();
    uint64_t last_counter = g_last_sample_counter.load(std::memory_order_relaxed);
    
    // Check if enough cycles have elapsed for a sample
    // For cycle counter mode, we check the actual cycle threshold
    // For POSIX-only mode, this will always be true (threshold is set to timer interval)
    if (current_counter - last_counter >= g_cycle_threshold) {
        // Update last sample counter
        g_last_sample_counter.store(current_counter, std::memory_order_relaxed);
        
        // Capture the call stack
        SampleCallStack stack;
        capture_call_stack(stack, kMaxCallStackDepth);
        
        // Record the sample (increment count for this call stack)
        if (g_samples != nullptr) {
            g_samples->increment(stack);
        }
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

/// Parse timer method from environment variable
/// @param env_value Value of PERFLOW_TIMER_METHOD environment variable
/// @return Parsed timer method
static TimerMethod parse_timer_method(const char* env_value) {
    if (env_value == nullptr) {
        return TimerMethod::kAuto;
    }
    
    if (strcmp(env_value, "cycle") == 0) {
        return TimerMethod::kCycle;
    } else if (strcmp(env_value, "posix") == 0) {
        return TimerMethod::kPosix;
    } else if (strcmp(env_value, "auto") == 0) {
        return TimerMethod::kAuto;
    }
    
    // Default to auto for unknown values
    return TimerMethod::kAuto;
}

/// Get timer method name for logging
/// @param method Timer method
/// @return Human-readable method name
static const char* get_timer_method_name(TimerMethod method) {
    switch (method) {
        case TimerMethod::kAuto:  return "auto";
        case TimerMethod::kCycle: return "cycle";
        case TimerMethod::kPosix: return "posix";
        default: return "unknown";
    }
}

/// Check if ARM cycle counter is available and working
/// @return true if cycle counter is available and returns sensible values
static bool check_arm_cycle_counter_available() {
#if PERFLOW_ARM_ARCH
    // Try to read the counter and frequency
    uint64_t counter1 = read_cycle_counter();
    uint64_t freq = read_counter_frequency();
    uint64_t counter2 = read_cycle_counter();
    
    // Verify that:
    // 1. Counter is incrementing (counter2 > counter1)
    // 2. Frequency is reasonable (1MHz to 10GHz)
    return (counter2 > counter1) && (freq >= 1000000ULL) && (freq <= 10000000000ULL);
#else
    return false;
#endif
}

// ============================================================================
// Initialization and Cleanup
// ============================================================================

/// Initialize cycle counter-based sampler
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
    
    // Check for timer method override
    const char* method_env = std::getenv("PERFLOW_TIMER_METHOD");
    g_timer_method = parse_timer_method(method_env);
    
    // Check ARM cycle counter availability
    g_arm_cycle_counter_available = check_arm_cycle_counter_available();
    
    // Determine effective timer method
    TimerMethod effective_method;
    if (g_timer_method == TimerMethod::kAuto) {
        effective_method = g_arm_cycle_counter_available ? 
                           TimerMethod::kCycle : TimerMethod::kPosix;
    } else if (g_timer_method == TimerMethod::kCycle && !g_arm_cycle_counter_available) {
        // Requested cycle counter but not available, fallback to POSIX
        fprintf(stderr, "[MPI Sampler Cycle] Warning: Cycle counter requested but not available, "
                        "falling back to POSIX timer\n");
        effective_method = TimerMethod::kPosix;
    } else {
        effective_method = g_timer_method;
    }
    
    // Get counter frequency
    g_counter_frequency = read_counter_frequency();
    
    // Calculate cycle threshold based on effective method
    if (effective_method == TimerMethod::kCycle) {
        // Use actual cycle threshold for cycle counter mode
        // cycles_per_sample = counter_frequency / sampling_frequency
        g_cycle_threshold = g_counter_frequency / static_cast<uint64_t>(g_sampling_frequency);
    } else {
        // For POSIX-only mode, set threshold to 0 to trigger on every timer interrupt
        // The timer interval itself controls the sampling frequency
        g_cycle_threshold = 0;
    }
    
    fprintf(stderr, "[MPI Sampler Cycle] Initializing with frequency %d Hz\n",
            g_sampling_frequency);
    fprintf(stderr, "[MPI Sampler Cycle] Timer method: %s (requested: %s)\n",
            get_timer_method_name(effective_method),
            get_timer_method_name(g_timer_method));
    fprintf(stderr, "[MPI Sampler Cycle] ARM cycle counter: %s\n",
            g_arm_cycle_counter_available ? "available" : "not available");
    
    if (effective_method == TimerMethod::kCycle) {
        fprintf(stderr, "[MPI Sampler Cycle] Counter frequency: %lu Hz\n",
                (unsigned long)g_counter_frequency);
        fprintf(stderr, "[MPI Sampler Cycle] Cycle threshold: %lu cycles\n",
                (unsigned long)g_cycle_threshold);
    }
    
    // Set up signal handler for timer
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = timer_signal_handler;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SAMPLING_SIGNAL, &sa, nullptr) == -1) {
        fprintf(stderr, "[MPI Sampler Cycle] Failed to set up signal handler: %s\n",
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
        fprintf(stderr, "[MPI Sampler Cycle] Failed to create timer: %s\n",
                strerror(errno));
        g_module_initialized = false;
        return;
    }
    
    g_timer_created = true;
    
    // Calculate timer interval
    // For cycle counter mode: use fast check interval
    // For POSIX-only mode: use direct sampling interval
    long long interval_ns;
    if (effective_method == TimerMethod::kCycle) {
        // Check frequently, but only sample when cycle threshold is reached
        interval_ns = kTimerCheckIntervalUs * 1000LL;  // Convert microseconds to nanoseconds
    } else {
        // Direct POSIX timer sampling
        interval_ns = 1000000000LL / g_sampling_frequency;
    }
    
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
        fprintf(stderr, "[MPI Sampler Cycle] Failed to start timer: %s\n",
                strerror(errno));
        timer_delete(g_timer_id);
        g_timer_created = false;
        g_module_initialized = false;
        return;
    }
    
    // Initialize last sample counter
    g_last_sample_counter.store(read_cycle_counter(), std::memory_order_relaxed);
    
    // Allocate sample storage
    g_samples = std::make_unique<SampleHashMap>();
    
    // Capture library map snapshot
    g_library_map = std::make_unique<LibraryMap>();
    if (!g_library_map->parse_current_process()) {
        fprintf(stderr, "[MPI Sampler Cycle] Warning: Failed to parse library map\n");
    } else {
        fprintf(stderr, "[MPI Sampler Cycle] Captured %zu library mappings\n",
                g_library_map->size());
    }
    
    fprintf(stderr, "[MPI Sampler Cycle] Initialization successful\n");
    fprintf(stderr, "[MPI Sampler Cycle] Timer started with interval: %lld ns (%.2f Hz effective)\n",
            interval_ns, 1000000000.0 / interval_ns);
}

/// Finalize the sampler and export data
static void finalize_sampler() {
    if (!g_module_initialized) {
        return;  // Not initialized
    }
    
    fprintf(stderr, "[MPI Sampler Cycle] Finalizing sampler\n");
    
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
        fprintf(stderr, "[MPI Sampler Cycle] Warning: MPI rank was not captured, using rank -1\n");
    }
    
    // Print sample statistics
    if (g_samples != nullptr) {
        if (g_mpi_rank == 0) {
            fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Collected %zu unique call stacks\n",
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
                fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Data exported to %s\n",
                        g_mpi_rank, exporter.filepath());
            }
        } else {
            fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Failed to export data\n",
                    g_mpi_rank);
        }
        
        if (g_mpi_rank == 0) {
            // Also export in human-readable text format
            DataResult text_result = exporter.exportDataText(*g_samples);
            
            if (text_result == DataResult::kSuccess) {
                fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Human-readable data exported to text file\n",
                        g_mpi_rank);
            } else {
                fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Failed to export human-readable data\n",
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
                    fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Library map exported to %s\n",
                            g_mpi_rank, map_exporter.filepath());
                }
            } else {
                fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Failed to export library map\n",
                        g_mpi_rank);
            }
            
            if (g_mpi_rank == 0) {
                // Also export in human-readable text format
                DataResult text_map_result = map_exporter.exportMapText(*g_library_map,
                                                                        static_cast<uint32_t>(g_mpi_rank));
                
                if (text_map_result == DataResult::kSuccess) {
                    fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Human-readable library map exported to text file\n",
                            g_mpi_rank);
                } else {
                    fprintf(stderr, "[MPI Sampler Cycle] Rank %d - Failed to export human-readable library map\n",
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
        fprintf(stderr, "[MPI Sampler Cycle] Finalization complete\n");
    }
}
