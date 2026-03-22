// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file mpi_sampler_timer.cpp
/// @brief MPI Performance Sampler with PMU or Thread-Based Backend
///
/// Two sampling backends, selected at compile time:
///
/// 1. PMU backend (default): perf_event_open + SIGIO signal on HW counter
///    overflow.  Full call stacks via libunwind from the signal handler.
///    Very low overhead.  Requires PMU access (perf_event_paranoid <= 2).
///    Key: exclude_kernel=1 avoids firing signals during UCX/SDMA ops.
///
/// 2. Thread backend (fallback): a pthread polls /proc/self/task/<tid>/syscall
///    to read the main thread's IP.  Signal-free but higher overhead and
///    only captures a single IP per sample.
///    Compile with  -DPERFLOW_USE_THREAD_SAMPLER  to select this backend.

#define _GNU_SOURCE

#if defined(__APPLE__) && !defined(_XOPEN_SOURCE)
// Required for ucontext on macOS headers.
#define _XOPEN_SOURCE 700
#endif

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <atomic>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

#include <ucontext.h>

#if defined(__linux__) && !defined(PERFLOW_USE_THREAD_SAMPLER)
#include <linux/perf_event.h>
#endif

#if defined(__APPLE__)
#include <pthread.h>
#endif

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include "sampling/sampling.h"
#include "sampling/library_map.h"
#include "sampling/data_export.h"
#include "sampling/mpi_init_setup.h"

using namespace perflow::sampling;

// ============================================================================
// Shared Configuration
// ============================================================================

constexpr int kDefaultSamplingFreq = 1000;
constexpr size_t kMaxCallStackDepth = 100;
constexpr size_t kSampleMapCapacity = 10000;

extern int g_mpi_rank;

static bool g_module_initialized = false;
static int g_sampling_frequency = kDefaultSamplingFreq;

using SampleCallStack = CallStack<kMaxCallStackDepth>;
using SampleHashMap = StaticHashMap<SampleCallStack, uint64_t, kSampleMapCapacity>;

static std::unique_ptr<SampleHashMap> g_samples = nullptr;
static std::unique_ptr<LibraryMap> g_library_map = nullptr;
static pid_t g_main_tid = 0;
static pthread_t g_main_pthread;

static uint64_t get_current_thread_id() {
#if defined(__APPLE__)
    uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    return tid;
#elif defined(__linux__)
    return static_cast<uint64_t>(syscall(SYS_gettid));
#else
    return 0;
#endif
}

static uintptr_t get_ip_from_ucontext(void* context) {
    if (!context) return 0;
    ucontext_t* uc = static_cast<ucontext_t*>(context);
#if defined(__APPLE__) && defined(__aarch64__)
    return static_cast<uintptr_t>(uc->uc_mcontext->__ss.__pc);
#elif defined(__APPLE__) && defined(__x86_64__)
    return static_cast<uintptr_t>(uc->uc_mcontext->__ss.__rip);
#elif defined(__linux__) && defined(__aarch64__)
    return static_cast<uintptr_t>(uc->uc_mcontext.pc);
#elif defined(__linux__) && defined(__x86_64__)
    return static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
#else
    (void)uc;
    return 0;
#endif
}

static size_t capture_call_stack_in_signal(SampleCallStack& stack,
                                           size_t max_depth) {
    stack.clear();
    unw_cursor_t cur;
    unw_context_t ctx;
    unw_getcontext(&ctx);
    unw_init_local(&cur, &ctx);

    size_t depth = 0;
    uintptr_t addrs[kMaxCallStackDepth];
    while (unw_step(&cur) > 0 && depth < max_depth
           && depth < kMaxCallStackDepth) {
        unw_word_t pc;
        unw_get_reg(&cur, UNW_REG_IP, &pc);
        if (pc == 0) break;
        addrs[depth++] = static_cast<uintptr_t>(pc) - 2;
    }
    // Skip: [0] this func  [1] signal handler  [2] signal trampoline
    constexpr size_t kSkip = 3;
    for (size_t i = kSkip; i < depth; ++i)
        if (addrs[i]) stack.push(addrs[i]);
    return depth > kSkip ? depth - kSkip : 0;
}

// ############################################################################
// Backend Implementation
// ############################################################################

#if defined(__linux__) && !defined(PERFLOW_USE_THREAD_SAMPLER)
// ===== PMU Backend: perf_event_open + SIGIO + libunwind call stacks =========

static int g_perf_fd = -1;
static volatile sig_atomic_t g_in_signal_handler = 0;
static bool g_fast_mode = false;  // skip libunwind, only record ucontext PC

static constexpr double kDefaultCpuFreqGHz = 2.6;

static long long compute_sample_period(int freq_hz) {
    const char* p_env = std::getenv("PERFLOW_PMU_PERIOD");
    if (p_env) {
        long long p = std::atoll(p_env);
        if (p > 0) return p;
    }
    const char* c_env = std::getenv("PERFLOW_CPU_FREQ_GHZ");
    double ghz = kDefaultCpuFreqGHz;
    if (c_env) { double v = std::atof(c_env); if (v > 0.1) ghz = v; }
    double hz = ghz * 1e9;
    return static_cast<long long>(hz / (freq_hz > 0 ? freq_hz : kDefaultSamplingFreq));
}

static long perf_event_open_sys(struct perf_event_attr *a, pid_t pid,
                                int cpu, int gfd, unsigned long fl) {
    return syscall(__NR_perf_event_open, a, pid, cpu, gfd, fl);
}

static void pmu_overflow_handler(int /*sig*/, siginfo_t *info, void *context) {
    if (g_in_signal_handler) {
        if (info && info->si_fd == g_perf_fd)
            ioctl(g_perf_fd, PERF_EVENT_IOC_REFRESH, 1);
        return;
    }
    g_in_signal_handler = 1;

    SampleCallStack stack;
    uintptr_t ip = get_ip_from_ucontext(context);

    if (!g_fast_mode) {
        size_t depth = capture_call_stack_in_signal(stack, kMaxCallStackDepth);
        if (depth == 0 && ip)
            stack.push(ip);
    } else {
        if (ip) stack.push(ip);
    }

    if (g_samples && !stack.empty())
        g_samples->increment(stack);

    g_in_signal_handler = 0;
    ioctl(g_perf_fd, PERF_EVENT_IOC_REFRESH, 1);
}

static bool setup_backend() {
    const char* fast_env = std::getenv("PERFLOW_PMU_FAST");
    g_fast_mode = (fast_env && fast_env[0] == '1');

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = pmu_overflow_handler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGIO, &sa, nullptr) < 0) {
        fprintf(stderr, "[MPI Sampler PMU] sigaction failed: %s\n",
                strerror(errno));
        return false;
    }

    long long period = compute_sample_period(g_sampling_frequency);

    struct perf_event_attr pe;
    std::memset(&pe, 0, sizeof(pe));
    pe.type           = PERF_TYPE_HARDWARE;
    pe.size           = sizeof(pe);
    pe.config         = PERF_COUNT_HW_CPU_CYCLES;
    pe.sample_period  = static_cast<uint64_t>(period);
    pe.sample_type    = PERF_SAMPLE_IP;
    pe.disabled       = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv     = 1;
    pe.wakeup_events  = 1;

    int fd = static_cast<int>(
        perf_event_open_sys(&pe, 0, -1, -1, 0));
    if (fd < 0) {
        fprintf(stderr,
            "[MPI Sampler PMU] perf_event_open failed: %s\n",
            strerror(errno));
        return false;
    }

    struct f_owner_ex owner;
    owner.type = F_OWNER_TID;
    owner.pid  = g_main_tid;
    fcntl(fd, F_SETOWN_EX, &owner);
    fcntl(fd, F_SETFL, O_ASYNC);
    fcntl(fd, F_SETSIG, SIGIO);

    g_perf_fd = fd;
    fprintf(stderr,
        "[MPI Sampler PMU] perf_event fd=%d period=%lld cycles (~%d Hz)\n",
        fd, period, g_sampling_frequency);
    return true;
}

static void arm_backend() {
    if (g_perf_fd < 0) return;
    ioctl(g_perf_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(g_perf_fd, PERF_EVENT_IOC_REFRESH, 1);
    fprintf(stderr,
        "[MPI Sampler PMU] Armed (perf_event + SIGIO, ~%d Hz%s)\n",
        g_sampling_frequency, g_fast_mode ? ", fast/IP-only" : ", libunwind");
}

static void teardown_backend() {
    if (g_perf_fd >= 0) {
        ioctl(g_perf_fd, PERF_EVENT_IOC_DISABLE, 0);
        close(g_perf_fd);
        g_perf_fd = -1;
    }
}

#elif defined(__linux__) && defined(PERFLOW_USE_THREAD_SAMPLER)
// ===== Thread Backend: pthread + /proc polling (signal-free) ================

static pthread_t g_sampler_thread;
static std::atomic<bool> g_sampler_running{false};
static std::atomic<bool> g_sampler_armed{false};

static uintptr_t read_thread_ip(pid_t tid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/task/%d/syscall", tid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    if (buf[0] == 'r') return 0;
    char *sp = strrchr(buf, ' ');
    if (!sp) return 0;
    uintptr_t pc = 0;
    if (sscanf(sp + 1, "0x%lx", &pc) != 1)
        sscanf(sp + 1, "%lx", &pc);
    return pc;
}

static void* sampler_thread_func(void*) {
    long long ns = 1000000000LL / g_sampling_frequency;
    struct timespec ts;
    ts.tv_sec  = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;
    while (g_sampler_running.load(std::memory_order_relaxed)) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
        if (!g_sampler_armed.load(std::memory_order_relaxed)) continue;
        uintptr_t pc = read_thread_ip(g_main_tid);
        if (pc && g_samples) {
            SampleCallStack stack;
            stack.push(pc);
            g_samples->increment(stack);
        }
    }
    return nullptr;
}

static bool setup_backend() {
    g_sampler_running.store(true, std::memory_order_relaxed);
    if (pthread_create(&g_sampler_thread, nullptr,
                       sampler_thread_func, nullptr) != 0) {
        fprintf(stderr,
            "[MPI Sampler Thread] pthread_create failed: %s\n",
            strerror(errno));
        g_sampler_running.store(false, std::memory_order_relaxed);
        return false;
    }
    return true;
}

static void arm_backend() {
    if (g_sampler_armed.load(std::memory_order_relaxed)) return;
    g_sampler_armed.store(true, std::memory_order_release);
    fprintf(stderr,
        "[MPI Sampler Thread] Armed (%d Hz, thread-based)\n",
        g_sampling_frequency);
}

static bool g_thread_torn_down = false;

static void teardown_backend() {
    if (g_thread_torn_down) return;
    g_thread_torn_down = true;
    g_sampler_running.store(false, std::memory_order_relaxed);
    pthread_join(g_sampler_thread, nullptr);
}

#else
// ===== Portable Signal Timer Backend (macOS + non-Linux) =====================
//
// A dedicated thread periodically sends a signal to the main thread; the signal
// handler records IP / callstack. This avoids Linux-specific perf_event and
// /proc access and builds cleanly on macOS.

static pthread_t g_sampler_thread;
static std::atomic<bool> g_sampler_running{false};
static std::atomic<bool> g_sampler_armed{false};
static volatile sig_atomic_t g_in_signal_handler = 0;
static bool g_fast_mode = false;  // if set, record only IP from ucontext
static constexpr int kSampleSignal = SIGUSR1;

static void sample_signal_handler(int /*sig*/, siginfo_t* /*info*/, void* context) {
    if (g_in_signal_handler) return;
    g_in_signal_handler = 1;

    SampleCallStack stack;
    uintptr_t ip = get_ip_from_ucontext(context);

    if (!g_fast_mode) {
        size_t depth = capture_call_stack_in_signal(stack, kMaxCallStackDepth);
        if (depth == 0 && ip) stack.push(ip);
    } else {
        if (ip) stack.push(ip);
    }

    if (g_samples && !stack.empty()) g_samples->increment(stack);
    g_in_signal_handler = 0;
}

static void* sampler_thread_func(void*) {
    long long ns = 1000000000LL / (g_sampling_frequency > 0 ? g_sampling_frequency : kDefaultSamplingFreq);
    struct timespec ts;
    ts.tv_sec  = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;

    while (g_sampler_running.load(std::memory_order_relaxed)) {
        nanosleep(&ts, nullptr);
        if (!g_sampler_armed.load(std::memory_order_relaxed)) continue;
        pthread_kill(g_main_pthread, kSampleSignal);
    }
    return nullptr;
}

static bool setup_backend() {
    const char* fast_env = std::getenv("PERFLOW_SIGNAL_FAST");
    g_fast_mode = (fast_env && fast_env[0] == '1');

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sample_signal_handler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(kSampleSignal, &sa, nullptr) < 0) {
        fprintf(stderr, "[MPI Sampler Signal] sigaction failed: %s\n", strerror(errno));
        return false;
    }

    g_sampler_running.store(true, std::memory_order_relaxed);
    if (pthread_create(&g_sampler_thread, nullptr, sampler_thread_func, nullptr) != 0) {
        fprintf(stderr, "[MPI Sampler Signal] pthread_create failed: %s\n", strerror(errno));
        g_sampler_running.store(false, std::memory_order_relaxed);
        return false;
    }

    fprintf(stderr, "[MPI Sampler Signal] Using SIGUSR1 sampling at ~%d Hz%s\n",
            g_sampling_frequency, g_fast_mode ? " (IP-only)" : " (libunwind)");
    return true;
}

static void arm_backend() {
    if (g_sampler_armed.load(std::memory_order_relaxed)) return;
    g_sampler_armed.store(true, std::memory_order_release);
    fprintf(stderr, "[MPI Sampler Signal] Armed (~%d Hz)\n", g_sampling_frequency);
}

static bool g_thread_torn_down = false;

static void teardown_backend() {
    if (g_thread_torn_down) return;
    g_thread_torn_down = true;
    g_sampler_running.store(false, std::memory_order_relaxed);
    pthread_join(g_sampler_thread, nullptr);
}
#endif

// ############################################################################
// Initialization / Arm / Finalization  (shared across both backends)
// ############################################################################

static void initialize_sampler() __attribute__((constructor));
static void finalize_sampler()   __attribute__((destructor));
extern "C" void perflow_arm_timer();
extern "C" void perflow_disarm_sampler();

static struct timespec g_arm_time   = {0, 0};
static struct timespec g_disarm_time = {0, 0};

static void initialize_sampler() {
    if (g_module_initialized) return;
    g_module_initialized = true;
    g_mpi_rank  = -1;
    g_main_pthread = pthread_self();
    g_main_tid  = static_cast<pid_t>(get_current_thread_id());

    const char* freq_env = std::getenv("PERFLOW_SAMPLING_FREQ");
    if (freq_env) {
        g_sampling_frequency = std::atoi(freq_env);
        if (g_sampling_frequency <= 0)
            g_sampling_frequency = kDefaultSamplingFreq;
    }

#if defined(__linux__) && !defined(PERFLOW_USE_THREAD_SAMPLER)
    fprintf(stderr,
        "[MPI Sampler PMU] Init freq=%d Hz (perf_event + SIGIO)\n",
        g_sampling_frequency);
#elif defined(__linux__) && defined(PERFLOW_USE_THREAD_SAMPLER)
    fprintf(stderr,
        "[MPI Sampler Thread] Init freq=%d Hz (thread, signal-free)\n",
        g_sampling_frequency);
#else
    fprintf(stderr,
        "[MPI Sampler Signal] Init freq=%d Hz (SIGUSR1 + libunwind)\n",
        g_sampling_frequency);
#endif

    g_samples     = std::make_unique<SampleHashMap>();
    g_library_map = std::make_unique<LibraryMap>();
    if (!g_library_map->parse_current_process()) {
        fprintf(stderr,
            "[MPI Sampler] Warning: failed to parse library map\n");
    } else {
        fprintf(stderr, "[MPI Sampler] Captured %zu library mappings\n",
                g_library_map->size());
    }

    if (!setup_backend()) {
        fprintf(stderr, "[MPI Sampler] Backend setup failed\n");
        g_module_initialized = false;
        return;
    }

    fprintf(stderr, "[MPI Sampler] Init OK, will arm after MPI_Init\n");
}

extern "C" void perflow_arm_timer() {
    clock_gettime(CLOCK_MONOTONIC, &g_arm_time);
    arm_backend();
}

extern "C" void perflow_disarm_sampler() {
    clock_gettime(CLOCK_MONOTONIC, &g_disarm_time);
    teardown_backend();
    double elapsed = (g_disarm_time.tv_sec - g_arm_time.tv_sec)
                   + (g_disarm_time.tv_nsec - g_arm_time.tv_nsec) * 1e-9;
    fprintf(stderr, "[MPI Sampler] Disarmed before MPI_Finalize "
            "(sampled region: %.3f s)\n", elapsed);
}

static void finalize_sampler() {
    if (!g_module_initialized) return;
    fprintf(stderr, "[MPI Sampler] Finalizing\n");

    teardown_backend();

    if (g_mpi_rank < 0)
        fprintf(stderr,
            "[MPI Sampler] Warning: MPI rank not captured, using -1\n");

    if (g_samples) {
        if (g_mpi_rank == 0)
            fprintf(stderr,
                "[MPI Sampler] Rank %d - %zu unique call stacks\n",
                g_mpi_rank, g_samples->size());

        char output_dir[256];
        const char* dir_env = std::getenv("PERFLOW_OUTPUT_DIR");
        if (dir_env) {
            std::strncpy(output_dir, dir_env, sizeof(output_dir) - 1);
            output_dir[sizeof(output_dir) - 1] = '\0';
        } else {
            std::strcpy(output_dir, "/tmp");
        }

        char fname[256];
        std::snprintf(fname, sizeof(fname),
                      "perflow_mpi_rank_%d", g_mpi_rank);

        // Binary data
        DataExporter exporter(output_dir, fname, false);
        DataResult res = exporter.exportData(*g_samples);
        if (res == DataResult::kSuccess) {
            if (g_mpi_rank == 0)
                fprintf(stderr,
                    "[MPI Sampler] Rank %d - Data -> %s\n",
                    g_mpi_rank, exporter.filepath());
        } else {
            fprintf(stderr,
                "[MPI Sampler] Rank %d - export failed\n", g_mpi_rank);
        }

        // Human-readable text (rank 0 only)
        if (g_mpi_rank == 0) {
            DataResult tr = exporter.exportDataText(*g_samples);
            if (tr == DataResult::kSuccess)
                fprintf(stderr,
                    "[MPI Sampler] Rank 0 - text data exported\n");
        }

        // Library map
        if (g_library_map && !g_library_map->empty()) {
            LibraryMapExporter mexp(output_dir, fname);
            DataResult mr = mexp.exportMap(
                *g_library_map, static_cast<uint32_t>(g_mpi_rank));
            if (mr == DataResult::kSuccess && g_mpi_rank == 0)
                fprintf(stderr,
                    "[MPI Sampler] Rank 0 - libmap -> %s\n",
                    mexp.filepath());

            if (g_mpi_rank == 0)
                mexp.exportMapText(
                    *g_library_map, static_cast<uint32_t>(g_mpi_rank));
        }
    }

    g_samples.reset();
    g_library_map.reset();

    if (g_mpi_rank == 0)
        fprintf(stderr, "[MPI Sampler] Done\n");
}
