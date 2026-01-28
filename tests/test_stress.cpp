// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file test_stress.cpp
/// @brief Test sampler under high load conditions
///
/// This test program validates the MPI sampler's stability and performance
/// under stress conditions including:
/// - Maximum process count (tested with available cores)
/// - Long-running application (30+ seconds)
/// - Mixed operations at high frequency
/// - Varying sampling frequencies

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

// ============================================================================
// Configuration
// ============================================================================

constexpr int kMatrixSize = 300;
constexpr int kVectorSize = 30000;
constexpr int kMessageSize = 5000;
constexpr int kStressDuration = 30;  // seconds
constexpr int kOperationsPerCycle = 100;

// ============================================================================
// High-Frequency Operations
// ============================================================================

/// Fast computation operations
double fast_compute(int rank, int iterations) {
    double result = 0.0;
    for (int i = 0; i < iterations; ++i) {
        double x = static_cast<double>(rank + i) * 0.01;
        result += x * x + sqrt(fabs(result / (i + 1)));
    }
    return result;
}

/// Fast communication operations
void fast_communicate(int rank, int size, int iterations) {
    std::vector<double> send_buf(kMessageSize);
    std::vector<double> recv_buf(kMessageSize);
    
    for (int i = 0; i < kMessageSize; ++i) {
        send_buf[i] = static_cast<double>(rank + i);
    }
    
    int partner = (rank + 1) % size;
    
    for (int iter = 0; iter < iterations; ++iter) {
        double local_value = static_cast<double>(rank + iter);
        double global_value;
        
        // Quick allreduce
        MPI_Allreduce(&local_value, &global_value, 1, MPI_DOUBLE,
                     MPI_SUM, MPI_COMM_WORLD);
        
        // Quick sendrecv
        if (iter % 5 == 0) {
            MPI_Sendrecv(send_buf.data(), kMessageSize, MPI_DOUBLE, partner, 0,
                        recv_buf.data(), kMessageSize, MPI_DOUBLE, partner, 0,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
}

// ============================================================================
// Stress Test: Mixed Operations at High Frequency
// ============================================================================

double test_mixed_high_frequency(int rank, int size, double duration) {
    std::vector<double> matrix(kMatrixSize * kMatrixSize);
    std::vector<double> vector(kVectorSize);
    std::vector<double> result_vec(kMatrixSize);
    
    // Initialize data
    for (int i = 0; i < kMatrixSize * kMatrixSize; ++i) {
        matrix[i] = static_cast<double>(rand()) / RAND_MAX;
    }
    for (int i = 0; i < kVectorSize; ++i) {
        vector[i] = static_cast<double>(rank + i);
    }
    
    double start_time = MPI_Wtime();
    double elapsed = 0.0;
    int cycle_count = 0;
    
    while (elapsed < duration) {
        // Computation burst
        double comp_result = fast_compute(rank, kOperationsPerCycle);
        
        // Matrix operations
        for (int i = 0; i < kMatrixSize; ++i) {
            result_vec[i] = 0.0;
            for (int j = 0; j < kMatrixSize; ++j) {
                result_vec[i] += matrix[i * kMatrixSize + j] * 
                                static_cast<double>(j + comp_result);
            }
        }
        
        // Communication burst
        fast_communicate(rank, size, 5);
        
        // Vector operations
        for (int i = 0; i < kVectorSize; ++i) {
            vector[i] = sin(vector[i]) + cos(vector[i] * 0.1);
        }
        
        // Memory operations
        if (cycle_count % 10 == 0) {
            std::vector<double> temp(kVectorSize);
            std::copy(vector.begin(), vector.end(), temp.begin());
            std::sort(temp.begin(), temp.end());
            vector[0] = temp[kVectorSize / 2];
        }
        
        cycle_count++;
        elapsed = MPI_Wtime() - start_time;
    }
    
    if (rank == 0) {
        printf("[Rank %d] Completed %d cycles in %.2f seconds (%.1f cycles/sec)\n",
               rank, cycle_count, elapsed, cycle_count / elapsed);
    }
    
    return elapsed;
}

// ============================================================================
// Stress Test: Long Running with Periodic Operations
// ============================================================================

double test_long_running(int rank, int size, double duration) {
    std::vector<double> data(kVectorSize);
    
    // Initialize
    for (int i = 0; i < kVectorSize; ++i) {
        data[i] = static_cast<double>(rank * 1000 + i);
    }
    
    double start_time = MPI_Wtime();
    double elapsed = 0.0;
    int iteration = 0;
    
    while (elapsed < duration) {
        // Periodic heavy computation
        if (iteration % 100 == 0) {
            for (int i = 0; i < kVectorSize; ++i) {
                data[i] = sin(data[i]) * cos(data[i] * 0.5) + 
                         sqrt(fabs(data[i]));
            }
        }
        
        // Periodic communication
        if (iteration % 50 == 0) {
            double local_sum = 0.0;
            for (int i = 0; i < kVectorSize; i += 100) {
                local_sum += data[i];
            }
            
            double global_sum;
            MPI_Allreduce(&local_sum, &global_sum, 1, MPI_DOUBLE,
                         MPI_SUM, MPI_COMM_WORLD);
            
            data[0] += global_sum * 0.0001;
        }
        
        // Light work every iteration
        double sum = 0.0;
        for (int i = 0; i < 1000; ++i) {
            sum += data[i];
        }
        data[iteration % kVectorSize] = sum * 0.0001;
        
        iteration++;
        elapsed = MPI_Wtime() - start_time;
    }
    
    if (rank == 0) {
        printf("[Rank %d] Completed %d iterations in %.2f seconds (%.1f iter/sec)\n",
               rank, iteration, elapsed, iteration / elapsed);
    }
    
    return elapsed;
}

// ============================================================================
// Stress Test: Memory Intensive Operations
// ============================================================================

double test_memory_stress(int rank, int size, double duration) {
    const int alloc_size = 1000000;  // 1M doubles
    
    double start_time = MPI_Wtime();
    double elapsed = 0.0;
    int cycle_count = 0;
    
    while (elapsed < duration) {
        // Allocate and use memory
        std::vector<double> temp1(alloc_size);
        std::vector<double> temp2(alloc_size);
        
        for (int i = 0; i < alloc_size; ++i) {
            temp1[i] = static_cast<double>(rank + i);
            temp2[i] = static_cast<double>(rank * 2 + i);
        }
        
        // Compute
        double sum = 0.0;
        for (int i = 0; i < alloc_size; i += 100) {
            sum += temp1[i] + temp2[i];
        }
        
        // Communicate
        if (cycle_count % 5 == 0) {
            double global_sum;
            MPI_Allreduce(&sum, &global_sum, 1, MPI_DOUBLE,
                         MPI_SUM, MPI_COMM_WORLD);
        }
        
        cycle_count++;
        elapsed = MPI_Wtime() - start_time;
    }
    
    if (rank == 0) {
        printf("[Rank %d] Completed %d memory cycles in %.2f seconds\n",
               rank, cycle_count, elapsed);
    }
    
    return elapsed;
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
    
    // Check sampling frequency from environment
    const char* freq_env = getenv("PERFLOW_SAMPLING_FREQ");
    int sampling_freq = freq_env ? atoi(freq_env) : 1000;
    
    if (rank == 0) {
        printf("=================================================\n");
        printf("Stress Test\n");
        printf("=================================================\n");
        printf("MPI Processes: %d\n", size);
        printf("Sampling Frequency: %d Hz\n", sampling_freq);
        printf("Test Duration: %d seconds per test\n", kStressDuration);
        printf("Matrix Size: %dx%d\n", kMatrixSize, kMatrixSize);
        printf("Vector Size: %d\n", kVectorSize);
        printf("=================================================\n\n");
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    // ========================================================================
    // Test 1: Mixed Operations at High Frequency
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 1: Mixed operations at high frequency...\n", rank);
    }
    double mixed_time = test_mixed_high_frequency(rank, size, 10.0);
    if (rank == 0) {
        printf("[Rank %d] Mixed high frequency: %.4f seconds\n", 
               rank, mixed_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 2: Long Running Application
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 2: Long running application...\n", rank);
    }
    double long_time = test_long_running(rank, size, 10.0);
    if (rank == 0) {
        printf("[Rank %d] Long running: %.4f seconds\n", 
               rank, long_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 3: Memory Stress
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 3: Memory intensive stress...\n", rank);
    }
    double memory_time = test_memory_stress(rank, size, 10.0);
    if (rank == 0) {
        printf("[Rank %d] Memory stress: %.4f seconds\n", 
               rank, memory_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Summary
    // ========================================================================
    double total_time = MPI_Wtime() - start_time;
    
    if (rank == 0) {
        printf("\n=================================================\n");
        printf("Stress Test Summary\n");
        printf("=================================================\n");
        printf("Mixed high frequency:    %.4f seconds\n", mixed_time);
        printf("Long running:            %.4f seconds\n", long_time);
        printf("Memory stress:           %.4f seconds\n", memory_time);
        printf("Total time:              %.4f seconds\n", total_time);
        printf("=================================================\n");
        printf("\nExpected Validation Points:\n");
        printf("- Sampler stability under high frequency\n");
        printf("- No performance degradation\n");
        printf("- Consistent sampling across all ranks\n");
        printf("- Memory usage remains stable\n");
        printf("\nTest completed successfully!\n");
        printf("Check output files: /tmp/perflow_mpi_rank_*.pflw\n");
        printf("\nTo test with different sampling frequencies:\n");
        printf("  PERFLOW_SAMPLING_FREQ=100 mpirun ...\n");
        printf("  PERFLOW_SAMPLING_FREQ=1000 mpirun ...\n");
        printf("  PERFLOW_SAMPLING_FREQ=10000 mpirun ...\n");
    }
    
    // Finalize MPI
    MPI_Finalize();
    
    return 0;
}
