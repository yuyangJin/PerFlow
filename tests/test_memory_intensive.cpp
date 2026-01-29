// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file test_memory_intensive.cpp
/// @brief Test sampler with memory allocation/deallocation patterns
///
/// This test program validates the MPI sampler's ability to collect
/// performance data during memory-intensive workloads including:
/// - Memory allocations and deallocations
/// - Large memory operations
/// - Memory access patterns (streaming, random)
/// - Cache-sensitive operations
///
/// IMPORTANT: Carefully designed to avoid deadlocks since sampler
/// may disable sampling during malloc operations.

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>

// ============================================================================
// Configuration
// ============================================================================

constexpr int kLargeSize = 10000000;  // 10M elements = ~80MB
constexpr int kMediumSize = 1000000;  // 1M elements = ~8MB
constexpr int kSmallSize = 10000;     // 10K elements
constexpr int kNumIterations = 10;

// ============================================================================
// Memory Allocation Patterns
// ============================================================================

/// Test repeated allocation and deallocation
double test_repeated_allocations(int rank) {
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < kNumIterations * 5; ++iter) {
        // Allocate medium-sized arrays
        double* array1 = new double[kMediumSize];
        double* array2 = new double[kMediumSize];
        
        // Initialize memory
        for (int i = 0; i < kMediumSize; ++i) {
            array1[i] = static_cast<double>(rank + i);
            array2[i] = static_cast<double>(rank * 2 + i);
        }
        
        // Perform some operations
        double sum = 0.0;
        for (int i = 0; i < kMediumSize; ++i) {
            sum += array1[i] + array2[i];
        }
        
        // Deallocate
        delete[] array1;
        delete[] array2;
        
        // Prevent optimization
        if (sum < 0) printf("");
    }
    
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Memory Access Patterns
// ============================================================================

/// Sequential (streaming) memory access
double test_streaming_access(int rank) {
    std::vector<double> array(kLargeSize);
    
    // Initialize
    for (int i = 0; i < kLargeSize; ++i) {
        array[i] = static_cast<double>(rank + i);
    }
    
    double start_time = MPI_Wtime();
    
    // Sequential read and write
    for (int iter = 0; iter < kNumIterations; ++iter) {
        double sum = 0.0;
        for (int i = 0; i < kLargeSize; ++i) {
            sum += array[i];
            array[i] = sum * 0.5;
        }
    }
    
    return MPI_Wtime() - start_time;
}

/// Random memory access pattern
double test_random_access(int rank) {
    std::vector<double> array(kLargeSize);
    std::vector<int> indices(kLargeSize);
    
    // Initialize array
    for (int i = 0; i < kLargeSize; ++i) {
        array[i] = static_cast<double>(i);
        indices[i] = i;
    }
    
    // Shuffle indices for random access
    std::mt19937 gen(rank * 12345);
    std::shuffle(indices.begin(), indices.end(), gen);
    
    double start_time = MPI_Wtime();
    
    // Random access pattern
    for (int iter = 0; iter < kNumIterations; ++iter) {
        double sum = 0.0;
        for (int i = 0; i < kLargeSize; ++i) {
            int idx = indices[i];
            sum += array[idx];
            array[idx] = sum * 0.5;
        }
    }
    
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Cache-Sensitive Operations
// ============================================================================

/// Cache-friendly operations (good locality)
double test_cache_friendly(int rank) {
    const int rows = 2000;
    const int cols = 2000;
    std::vector<double> matrix(rows * cols);
    
    // Initialize matrix
    for (int i = 0; i < rows * cols; ++i) {
        matrix[i] = static_cast<double>(rank + i);
    }
    
    double start_time = MPI_Wtime();
    
    // Row-major access (cache-friendly)
    for (int iter = 0; iter < kNumIterations; ++iter) {
        for (int i = 0; i < rows; ++i) {
            double sum = 0.0;
            for (int j = 0; j < cols; ++j) {
                sum += matrix[i * cols + j];
                matrix[i * cols + j] = sum * 0.1;
            }
        }
    }
    
    return MPI_Wtime() - start_time;
}

/// Cache-unfriendly operations (poor locality)
double test_cache_unfriendly(int rank) {
    const int rows = 2000;
    const int cols = 2000;
    std::vector<double> matrix(rows * cols);
    
    // Initialize matrix
    for (int i = 0; i < rows * cols; ++i) {
        matrix[i] = static_cast<double>(rank + i);
    }
    
    double start_time = MPI_Wtime();
    
    // Column-major access (cache-unfriendly for row-major storage)
    for (int iter = 0; iter < kNumIterations; ++iter) {
        for (int j = 0; j < cols; ++j) {
            double sum = 0.0;
            for (int i = 0; i < rows; ++i) {
                sum += matrix[i * cols + j];
                matrix[i * cols + j] = sum * 0.1;
            }
        }
    }
    
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Memory Copy and Move Operations
// ============================================================================

/// Test memory copy operations
double test_memory_copy(int rank) {
    std::vector<double> src(kLargeSize);
    std::vector<double> dst(kLargeSize);
    
    // Initialize source
    for (int i = 0; i < kLargeSize; ++i) {
        src[i] = static_cast<double>(rank + i);
    }
    
    double start_time = MPI_Wtime();
    
    // Perform memory copies
    for (int iter = 0; iter < kNumIterations * 2; ++iter) {
        std::memcpy(dst.data(), src.data(), kLargeSize * sizeof(double));
        
        // Modify destination to prevent optimization
        for (int i = 0; i < kLargeSize; i += 1000) {
            dst[i] *= 1.01;
        }
        
        // Copy back
        std::memcpy(src.data(), dst.data(), kLargeSize * sizeof(double));
    }
    
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Main Test Program
// ============================================================================

int main(int argc, char** argv) {
    // Initialize MPI
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (rank == 0) {
        printf("=================================================\n");
        printf("Memory-Intensive Test\n");
        printf("=================================================\n");
        printf("MPI Processes: %d\n", size);
        printf("Large array size: %d elements (~%.1f MB)\n", 
               kLargeSize, kLargeSize * sizeof(double) / 1024.0 / 1024.0);
        printf("Medium array size: %d elements (~%.1f MB)\n",
               kMediumSize, kMediumSize * sizeof(double) / 1024.0 / 1024.0);
        printf("Iterations: %d\n", kNumIterations);
        printf("=================================================\n\n");
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    // ========================================================================
    // Test 1: Repeated Allocations
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 1: Repeated allocations/deallocations...\n", rank);
    }
    double alloc_time = test_repeated_allocations(rank);
    if (rank == 0) {
        printf("[Rank %d] Repeated allocations: %.4f seconds\n", 
               rank, alloc_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 2: Streaming Access
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 2: Sequential (streaming) memory access...\n", rank);
    }
    double stream_time = test_streaming_access(rank);
    if (rank == 0) {
        printf("[Rank %d] Streaming access: %.4f seconds\n", 
               rank, stream_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 3: Random Access
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 3: Random memory access pattern...\n", rank);
    }
    double random_time = test_random_access(rank);
    if (rank == 0) {
        printf("[Rank %d] Random access: %.4f seconds\n", 
               rank, random_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 4: Cache-Friendly Operations
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 4: Cache-friendly operations...\n", rank);
    }
    double cache_friendly_time = test_cache_friendly(rank);
    if (rank == 0) {
        printf("[Rank %d] Cache-friendly: %.4f seconds\n", 
               rank, cache_friendly_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 5: Cache-Unfriendly Operations
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 5: Cache-unfriendly operations...\n", rank);
    }
    double cache_unfriendly_time = test_cache_unfriendly(rank);
    if (rank == 0) {
        printf("[Rank %d] Cache-unfriendly: %.4f seconds\n", 
               rank, cache_unfriendly_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 6: Memory Copy Operations
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 6: Memory copy operations...\n", rank);
    }
    double copy_time = test_memory_copy(rank);
    if (rank == 0) {
        printf("[Rank %d] Memory copy: %.4f seconds\n", 
               rank, copy_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Summary
    // ========================================================================
    double total_time = MPI_Wtime() - start_time;
    
    if (rank == 0) {
        printf("\n=================================================\n");
        printf("Memory-Intensive Test Summary\n");
        printf("=================================================\n");
        printf("Repeated allocations:    %.4f seconds\n", alloc_time);
        printf("Streaming access:        %.4f seconds\n", stream_time);
        printf("Random access:           %.4f seconds\n", random_time);
        printf("Cache-friendly:          %.4f seconds\n", cache_friendly_time);
        printf("Cache-unfriendly:        %.4f seconds\n", cache_unfriendly_time);
        printf("Memory copy:             %.4f seconds\n", copy_time);
        printf("Total time:              %.4f seconds\n", total_time);
        printf("=================================================\n");
        printf("\nExpected Validation Points:\n");
        printf("- High cache miss counters (PAPI_L1_DCM, PAPI_L2_DCM)\n");
        printf("- Memory bandwidth measurements\n");
        printf("- Stable operation despite frequent memory operations\n");
        printf("- Cache-unfriendly should show higher miss rate than cache-friendly\n");
        printf("\nTest completed successfully!\n");
        printf("Check output files: /tmp/perflow_mpi_rank_*.pflw\n");
    }
    
    // Finalize MPI
    MPI_Finalize();
    
    return 0;
}
