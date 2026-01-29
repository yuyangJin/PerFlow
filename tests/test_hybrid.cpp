// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file test_hybrid.cpp
/// @brief Test sampler with mixed computation/communication patterns
///
/// This test program validates the MPI sampler's ability to collect
/// performance data during hybrid workloads including:
/// - Alternating phases of computation and communication
/// - Overlapped computation and communication
/// - Load imbalance scenarios
/// - Realistic HPC application patterns

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

// ============================================================================
// Configuration
// ============================================================================

constexpr int kMatrixSize = 400;
constexpr int kVectorSize = 50000;
constexpr int kMessageSize = 10000;
constexpr int kNumPhases = 10;

// ============================================================================
// Computation Functions
// ============================================================================

/// Simple matrix-vector multiplication
void matrix_vector_multiply(const double* A, const double* x, double* y, 
                            int rows, int cols) {
    for (int i = 0; i < rows; ++i) {
        y[i] = 0.0;
        for (int j = 0; j < cols; ++j) {
            y[i] += A[i * cols + j] * x[j];
        }
    }
}

/// Computation phase
double compute_work(int rank, int workload_size) {
    double sum = 0.0;
    for (int i = 0; i < workload_size; ++i) {
        double x = static_cast<double>(rank + i) * 0.01;
        sum += sin(x) * cos(x) + sqrt(fabs(sum / (i + 1)));
    }
    return sum;
}

// ============================================================================
// Test 1: Alternating Computation and Communication
// ============================================================================

double test_alternating_phases(int rank, int size) {
    std::vector<double> local_data(kVectorSize);
    std::vector<double> recv_data(kVectorSize);
    
    // Initialize local data
    for (int i = 0; i < kVectorSize; ++i) {
        local_data[i] = static_cast<double>(rank * kVectorSize + i);
    }
    
    double start_time = MPI_Wtime();
    
    for (int phase = 0; phase < kNumPhases; ++phase) {
        // Computation phase
        double result = 0.0;
        for (int i = 0; i < kVectorSize; ++i) {
            result += sin(local_data[i]) + cos(local_data[i]);
            local_data[i] = result * 0.001;
        }
        
        // Communication phase - Allreduce to synchronize
        double global_result;
        MPI_Allreduce(&result, &global_result, 1, MPI_DOUBLE, 
                     MPI_SUM, MPI_COMM_WORLD);
        
        // Use the global result
        for (int i = 0; i < kVectorSize; i += 100) {
            local_data[i] += global_result * 0.0001;
        }
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Test 2: Overlapped Computation and Communication
// ============================================================================

double test_overlapped_comp_comm(int rank, int size) {
    std::vector<double> local_data(kVectorSize);
    std::vector<double> send_buffer(kMessageSize);
    std::vector<double> recv_buffer(kMessageSize);
    
    // Initialize data
    for (int i = 0; i < kVectorSize; ++i) {
        local_data[i] = static_cast<double>(rank + i);
    }
    for (int i = 0; i < kMessageSize; ++i) {
        send_buffer[i] = static_cast<double>(rank * 1000 + i);
    }
    
    int next_rank = (rank + 1) % size;
    int prev_rank = (rank - 1 + size) % size;
    
    double start_time = MPI_Wtime();
    
    for (int phase = 0; phase < kNumPhases; ++phase) {
        MPI_Request send_req, recv_req;
        
        // Post non-blocking communication (ring pattern)
        MPI_Irecv(recv_buffer.data(), kMessageSize, MPI_DOUBLE, prev_rank,
                 phase, MPI_COMM_WORLD, &recv_req);
        MPI_Isend(send_buffer.data(), kMessageSize, MPI_DOUBLE, next_rank,
                 phase, MPI_COMM_WORLD, &send_req);
        
        // Do computation while communication is in progress
        double result = 0.0;
        for (int i = 0; i < kVectorSize; ++i) {
            result += local_data[i] * local_data[i];
            local_data[i] = sqrt(fabs(result / (i + 1)));
        }
        
        // Wait for communication to complete
        MPI_Wait(&send_req, MPI_STATUS_IGNORE);
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
        
        // Use received data
        for (int i = 0; i < kMessageSize; ++i) {
            send_buffer[i] = (send_buffer[i] + recv_buffer[i]) * 0.5;
        }
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Test 3: Load Imbalance Scenarios
// ============================================================================

double test_load_imbalance(int rank, int size) {
    // Create artificial load imbalance
    // Rank 0 does more work, other ranks do less
    int workload_factor = (rank == 0) ? 5 : 1;
    int local_workload = kVectorSize * workload_factor;
    
    std::vector<double> local_data(local_workload);
    
    // Initialize
    for (int i = 0; i < local_workload; ++i) {
        local_data[i] = static_cast<double>(rank + i);
    }
    
    double start_time = MPI_Wtime();
    
    for (int phase = 0; phase < kNumPhases; ++phase) {
        // Each rank does different amount of work
        double result = 0.0;
        for (int i = 0; i < local_workload; ++i) {
            result += sin(local_data[i]) * cos(local_data[i]);
            local_data[i] = result * 0.001;
        }
        
        // Barrier to synchronize (some ranks will wait)
        MPI_Barrier(MPI_COMM_WORLD);
        
        // Gather results to rank 0
        double global_result;
        MPI_Reduce(&result, &global_result, 1, MPI_DOUBLE,
                  MPI_SUM, 0, MPI_COMM_WORLD);
        
        // Broadcast result
        MPI_Bcast(&global_result, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Test 4: Stencil-like Pattern (Neighbor Communication)
// ============================================================================

double test_stencil_pattern(int rank, int size) {
    std::vector<double> grid(kVectorSize);
    std::vector<double> new_grid(kVectorSize);
    
    // Initialize grid
    for (int i = 0; i < kVectorSize; ++i) {
        grid[i] = static_cast<double>(rank * kVectorSize + i);
    }
    
    int left = (rank - 1 + size) % size;
    int right = (rank + 1) % size;
    
    const int boundary_size = 100;
    std::vector<double> left_boundary(boundary_size);
    std::vector<double> right_boundary(boundary_size);
    
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < kNumPhases * 2; ++iter) {
        // Exchange boundaries with neighbors
        MPI_Sendrecv(&grid[0], boundary_size, MPI_DOUBLE, left, 0,
                    right_boundary.data(), boundary_size, MPI_DOUBLE, right, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        MPI_Sendrecv(&grid[kVectorSize - boundary_size], boundary_size, 
                    MPI_DOUBLE, right, 1,
                    left_boundary.data(), boundary_size, MPI_DOUBLE, left, 1,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // Update interior points (stencil computation)
        for (int i = 1; i < kVectorSize - 1; ++i) {
            new_grid[i] = 0.25 * (grid[i-1] + 2.0 * grid[i] + grid[i+1]);
        }
        
        // Update boundary points using neighbor data
        new_grid[0] = 0.25 * (left_boundary[boundary_size-1] + 
                             2.0 * grid[0] + grid[1]);
        new_grid[kVectorSize-1] = 0.25 * (grid[kVectorSize-2] + 
                                         2.0 * grid[kVectorSize-1] + 
                                         right_boundary[0]);
        
        // Swap grids
        std::swap(grid, new_grid);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Test 5: Pipeline Pattern
// ============================================================================

double test_pipeline_pattern(int rank, int size) {
    std::vector<double> data(kMessageSize);
    
    // Initialize data on rank 0
    if (rank == 0) {
        for (int i = 0; i < kMessageSize; ++i) {
            data[i] = static_cast<double>(i);
        }
    }
    
    double start_time = MPI_Wtime();
    
    for (int stage = 0; stage < kNumPhases; ++stage) {
        // Pipeline: receive from previous, process, send to next
        if (rank > 0) {
            MPI_Recv(data.data(), kMessageSize, MPI_DOUBLE, rank - 1,
                    stage, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        // Process data
        double result = compute_work(rank, 50000);
        for (int i = 0; i < kMessageSize; ++i) {
            data[i] = data[i] + result * 0.0001;
        }
        
        // Send to next rank
        if (rank < size - 1) {
            MPI_Send(data.data(), kMessageSize, MPI_DOUBLE, rank + 1,
                    stage, MPI_COMM_WORLD);
        }
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
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
        printf("Hybrid Workload Test\n");
        printf("=================================================\n");
        printf("MPI Processes: %d\n", size);
        printf("Matrix Size: %dx%d\n", kMatrixSize, kMatrixSize);
        printf("Vector Size: %d\n", kVectorSize);
        printf("Number of Phases: %d\n", kNumPhases);
        printf("=================================================\n\n");
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    // ========================================================================
    // Test 1: Alternating Computation and Communication
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 1: Alternating computation/communication...\n", rank);
    }
    double alternating_time = test_alternating_phases(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Alternating phases: %.4f seconds\n", 
               rank, alternating_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 2: Overlapped Computation and Communication
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 2: Overlapped computation/communication...\n", rank);
    }
    double overlapped_time = test_overlapped_comp_comm(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Overlapped comp/comm: %.4f seconds\n", 
               rank, overlapped_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 3: Load Imbalance
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 3: Load imbalance scenario...\n", rank);
    }
    double imbalance_time = test_load_imbalance(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Load imbalance: %.4f seconds\n", 
               rank, imbalance_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 4: Stencil Pattern
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 4: Stencil communication pattern...\n", rank);
    }
    double stencil_time = test_stencil_pattern(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Stencil pattern: %.4f seconds\n", 
               rank, stencil_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 5: Pipeline Pattern
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 5: Pipeline pattern...\n", rank);
    }
    double pipeline_time = test_pipeline_pattern(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Pipeline pattern: %.4f seconds\n", 
               rank, pipeline_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Summary
    // ========================================================================
    double total_time = MPI_Wtime() - start_time;
    
    if (rank == 0) {
        printf("\n=================================================\n");
        printf("Hybrid Workload Test Summary\n");
        printf("=================================================\n");
        printf("Alternating phases:      %.4f seconds\n", alternating_time);
        printf("Overlapped comp/comm:    %.4f seconds\n", overlapped_time);
        printf("Load imbalance:          %.4f seconds\n", imbalance_time);
        printf("Stencil pattern:         %.4f seconds\n", stencil_time);
        printf("Pipeline pattern:        %.4f seconds\n", pipeline_time);
        printf("Total time:              %.4f seconds\n", total_time);
        printf("=================================================\n");
        printf("\nExpected Validation Points:\n");
        printf("- Balanced collection of computation and communication events\n");
        printf("- Correct attribution of time to different phases\n");
        printf("- Load imbalance visible in per-rank statistics\n");
        printf("\nTest completed successfully!\n");
        printf("Check output files: /tmp/perflow_mpi_rank_*.pflw\n");
    }
    
    // Finalize MPI
    MPI_Finalize();
    
    return 0;
}
