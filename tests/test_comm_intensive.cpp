// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file test_comm_intensive.cpp
/// @brief Test sampler with MPI communication-heavy workloads
///
/// This test program validates the MPI sampler's ability to collect
/// performance data during communication-intensive workloads including:
/// - Point-to-point communications (MPI_Send/Recv)
/// - Collective operations (MPI_Allreduce, MPI_Alltoall)
/// - Non-blocking communication patterns
/// - Various communication topologies (ring, mesh, broadcast tree)

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

// ============================================================================
// Configuration
// ============================================================================

constexpr int kMessageSize = 10000;
constexpr int kNumIterations = 20;
constexpr int kSmallMessageSize = 100;

// ============================================================================
// Point-to-Point Communication Patterns
// ============================================================================

/// Ring communication pattern
double test_ring_communication(int rank, int size) {
    std::vector<double> send_buffer(kMessageSize);
    std::vector<double> recv_buffer(kMessageSize);
    
    // Initialize send buffer
    for (int i = 0; i < kMessageSize; ++i) {
        send_buffer[i] = static_cast<double>(rank * kMessageSize + i);
    }
    
    int next_rank = (rank + 1) % size;
    int prev_rank = (rank - 1 + size) % size;
    
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        // Send to next, receive from previous
        MPI_Sendrecv(send_buffer.data(), kMessageSize, MPI_DOUBLE, next_rank, 0,
                    recv_buffer.data(), kMessageSize, MPI_DOUBLE, prev_rank, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // Copy received data to send buffer for next iteration
        std::copy(recv_buffer.begin(), recv_buffer.end(), send_buffer.begin());
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime() - start_time;
}

/// Mesh communication pattern (all-to-all neighbor exchange)
double test_mesh_communication(int rank, int size) {
    std::vector<double> send_buffers(size * kSmallMessageSize);
    std::vector<double> recv_buffers(size * kSmallMessageSize);
    
    // Initialize send buffers
    for (int i = 0; i < size * kSmallMessageSize; ++i) {
        send_buffers[i] = static_cast<double>(rank * 1000 + i);
    }
    
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        // Send to all other processes
        for (int dest = 0; dest < size; ++dest) {
            if (dest != rank) {
                MPI_Send(&send_buffers[dest * kSmallMessageSize], 
                        kSmallMessageSize, MPI_DOUBLE, dest, iter,
                        MPI_COMM_WORLD);
            }
        }
        
        // Receive from all other processes
        for (int src = 0; src < size; ++src) {
            if (src != rank) {
                MPI_Recv(&recv_buffers[src * kSmallMessageSize],
                        kSmallMessageSize, MPI_DOUBLE, src, iter,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Collective Operations
// ============================================================================

/// Test various collective operations
double test_collective_operations(int rank, int size) {
    std::vector<double> send_data(kMessageSize);
    std::vector<double> recv_data(kMessageSize);
    
    // Initialize send data
    for (int i = 0; i < kMessageSize; ++i) {
        send_data[i] = static_cast<double>(rank + i);
    }
    
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        // MPI_Bcast
        if (rank == 0) {
            MPI_Bcast(send_data.data(), kMessageSize, MPI_DOUBLE, 0, 
                     MPI_COMM_WORLD);
        } else {
            MPI_Bcast(recv_data.data(), kMessageSize, MPI_DOUBLE, 0,
                     MPI_COMM_WORLD);
        }
        
        // MPI_Allreduce
        MPI_Allreduce(send_data.data(), recv_data.data(), kMessageSize,
                     MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        
        // MPI_Reduce
        MPI_Reduce(send_data.data(), recv_data.data(), kMessageSize,
                  MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        
        // MPI_Scatter
        std::vector<double> scatter_send;
        if (rank == 0) {
            scatter_send.resize(size * kSmallMessageSize);
            for (int i = 0; i < size * kSmallMessageSize; ++i) {
                scatter_send[i] = static_cast<double>(i);
            }
        }
        std::vector<double> scatter_recv(kSmallMessageSize);
        MPI_Scatter(scatter_send.data(), kSmallMessageSize, MPI_DOUBLE,
                   scatter_recv.data(), kSmallMessageSize, MPI_DOUBLE,
                   0, MPI_COMM_WORLD);
        
        // MPI_Gather
        std::vector<double> gather_recv;
        if (rank == 0) {
            gather_recv.resize(size * kSmallMessageSize);
        }
        MPI_Gather(scatter_recv.data(), kSmallMessageSize, MPI_DOUBLE,
                  gather_recv.data(), kSmallMessageSize, MPI_DOUBLE,
                  0, MPI_COMM_WORLD);
        
        // MPI_Allgather
        std::vector<double> allgather_recv(size * kSmallMessageSize);
        MPI_Allgather(scatter_recv.data(), kSmallMessageSize, MPI_DOUBLE,
                     allgather_recv.data(), kSmallMessageSize, MPI_DOUBLE,
                     MPI_COMM_WORLD);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Non-Blocking Communication
// ============================================================================

/// Test non-blocking communication patterns
double test_nonblocking_communication(int rank, int size) {
    std::vector<double> send_buffer(kMessageSize);
    std::vector<double> recv_buffer(kMessageSize);
    
    // Initialize send buffer
    for (int i = 0; i < kMessageSize; ++i) {
        send_buffer[i] = static_cast<double>(rank * 1000 + i);
    }
    
    int partner = (rank + size / 2) % size;
    
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        MPI_Request send_req, recv_req;
        
        // Post non-blocking receive
        MPI_Irecv(recv_buffer.data(), kMessageSize, MPI_DOUBLE, partner,
                 iter, MPI_COMM_WORLD, &recv_req);
        
        // Post non-blocking send
        MPI_Isend(send_buffer.data(), kMessageSize, MPI_DOUBLE, partner,
                 iter, MPI_COMM_WORLD, &send_req);
        
        // Do some computation while communication is in progress
        double computation = 0.0;
        for (int i = 0; i < 10000; ++i) {
            computation += static_cast<double>(i) * 0.001;
        }
        
        // Wait for communication to complete
        MPI_Wait(&send_req, MPI_STATUS_IGNORE);
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
        
        // Use received data
        send_buffer[0] += recv_buffer[0] + computation;
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime() - start_time;
}

// ============================================================================
// Alltoall Communication
// ============================================================================

/// Test MPI_Alltoall operations
double test_alltoall_communication(int rank, int size) {
    std::vector<double> send_buffer(size * kSmallMessageSize);
    std::vector<double> recv_buffer(size * kSmallMessageSize);
    
    // Initialize send buffer
    for (int i = 0; i < size * kSmallMessageSize; ++i) {
        send_buffer[i] = static_cast<double>(rank * 10000 + i);
    }
    
    double start_time = MPI_Wtime();
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        MPI_Alltoall(send_buffer.data(), kSmallMessageSize, MPI_DOUBLE,
                    recv_buffer.data(), kSmallMessageSize, MPI_DOUBLE,
                    MPI_COMM_WORLD);
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
    
    if (size < 2) {
        if (rank == 0) {
            fprintf(stderr, "Error: This test requires at least 2 MPI processes\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    if (rank == 0) {
        printf("=================================================\n");
        printf("Communication-Intensive Test\n");
        printf("=================================================\n");
        printf("MPI Processes: %d\n", size);
        printf("Message Size: %d doubles\n", kMessageSize);
        printf("Small Message Size: %d doubles\n", kSmallMessageSize);
        printf("Iterations: %d\n", kNumIterations);
        printf("=================================================\n\n");
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    // ========================================================================
    // Test 1: Ring Communication
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 1: Ring communication pattern...\n", rank);
    }
    double ring_time = test_ring_communication(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Ring communication: %.4f seconds\n", rank, ring_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 2: Mesh Communication
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 2: Mesh communication pattern...\n", rank);
    }
    double mesh_time = test_mesh_communication(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Mesh communication: %.4f seconds\n", rank, mesh_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 3: Collective Operations
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 3: Collective operations...\n", rank);
    }
    double collective_time = test_collective_operations(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Collective operations: %.4f seconds\n", 
               rank, collective_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 4: Non-Blocking Communication
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 4: Non-blocking communication...\n", rank);
    }
    double nonblocking_time = test_nonblocking_communication(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Non-blocking communication: %.4f seconds\n", 
               rank, nonblocking_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 5: Alltoall Communication
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 5: Alltoall communication...\n", rank);
    }
    double alltoall_time = test_alltoall_communication(rank, size);
    if (rank == 0) {
        printf("[Rank %d] Alltoall communication: %.4f seconds\n", 
               rank, alltoall_time);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Summary
    // ========================================================================
    double total_time = MPI_Wtime() - start_time;
    
    if (rank == 0) {
        printf("\n=================================================\n");
        printf("Communication-Intensive Test Summary\n");
        printf("=================================================\n");
        printf("Ring communication:      %.4f seconds\n", ring_time);
        printf("Mesh communication:      %.4f seconds\n", mesh_time);
        printf("Collective operations:   %.4f seconds\n", collective_time);
        printf("Non-blocking comm:       %.4f seconds\n", nonblocking_time);
        printf("Alltoall communication:  %.4f seconds\n", alltoall_time);
        printf("Total time:              %.4f seconds\n", total_time);
        printf("=================================================\n");
        printf("\nExpected Validation Points:\n");
        printf("- High MPI call counts\n");
        printf("- Communication time measurements\n");
        printf("- Overlap of computation and communication\n");
        printf("\nTest completed successfully!\n");
        printf("Check output files: /tmp/perflow_mpi_rank_*.pflw\n");
    }
    
    // Finalize MPI
    MPI_Finalize();
    
    return 0;
}
