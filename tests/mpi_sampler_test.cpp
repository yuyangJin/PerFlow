// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file mpi_sampler_test.cpp
/// @brief Test program for MPI Performance Sampler
///
/// This test program performs various computational and communication
/// operations to generate samples for the MPI sampler to collect.

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

// ============================================================================
// Configuration
// ============================================================================

constexpr int kMatrixSize = 100;
constexpr int kVectorSize = 10000;
constexpr int kNumIterations = 10;

// ============================================================================
// Matrix Operations
// ============================================================================

/// Perform matrix multiplication: C = A * B
/// @param A Input matrix A (size x size)
/// @param B Input matrix B (size x size)
/// @param C Output matrix C (size x size)
/// @param size Matrix dimension
void matrix_multiply(const double* A, const double* B, double* C, int size) {
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            C[i * size + j] = 0.0;
            for (int k = 0; k < size; ++k) {
                C[i * size + j] += A[i * size + k] * B[k * size + j];
            }
        }
    }
}

/// Initialize matrix with random values
/// @param matrix Output matrix
/// @param size Matrix dimension
/// @param seed Random seed
void init_matrix(double* matrix, int size, int seed) {
    srand(seed);
    for (int i = 0; i < size * size; ++i) {
        matrix[i] = static_cast<double>(rand()) / RAND_MAX;
    }
}

/// Test matrix multiplication workload
/// @param rank MPI rank
void test_matrix_multiplication(int rank) {
    printf("[Rank %d] Testing matrix multiplication (%dx%d)...\n",
           rank, kMatrixSize, kMatrixSize);
    
    std::vector<double> A(kMatrixSize * kMatrixSize);
    std::vector<double> B(kMatrixSize * kMatrixSize);
    std::vector<double> C(kMatrixSize * kMatrixSize);
    
    // Initialize matrices
    init_matrix(A.data(), kMatrixSize, rank * 1000);
    init_matrix(B.data(), kMatrixSize, rank * 1000 + 1);
    
    // Perform multiplication multiple times to generate samples
    for (int iter = 0; iter < kNumIterations; ++iter) {
        matrix_multiply(A.data(), B.data(), C.data(), kMatrixSize);
    }
    
    // Verify result (sanity check)
    double sum = 0.0;
    for (int i = 0; i < kMatrixSize * kMatrixSize; ++i) {
        sum += C[i];
    }
    
    printf("[Rank %d] Matrix multiplication complete (checksum: %.6f)\n",
           rank, sum);
}

// ============================================================================
// Vector Operations
// ============================================================================

/// Vector dot product
/// @param a First vector
/// @param b Second vector
/// @param size Vector size
/// @return Dot product result
double vector_dot_product(const double* a, const double* b, int size) {
    double result = 0.0;
    for (int i = 0; i < size; ++i) {
        result += a[i] * b[i];
    }
    return result;
}

/// Vector addition: c = a + b
/// @param a First vector
/// @param b Second vector
/// @param c Output vector
/// @param size Vector size
void vector_add(const double* a, const double* b, double* c, int size) {
    for (int i = 0; i < size; ++i) {
        c[i] = a[i] + b[i];
    }
}

/// Vector scaling: b = alpha * a
/// @param a Input vector
/// @param b Output vector
/// @param alpha Scaling factor
/// @param size Vector size
void vector_scale(const double* a, double* b, double alpha, int size) {
    for (int i = 0; i < size; ++i) {
        b[i] = alpha * a[i];
    }
}

/// Test vector operations workload
/// @param rank MPI rank
void test_vector_operations(int rank) {
    printf("[Rank %d] Testing vector operations (size %d)...\n",
           rank, kVectorSize);
    
    std::vector<double> a(kVectorSize);
    std::vector<double> b(kVectorSize);
    std::vector<double> c(kVectorSize);
    
    // Initialize vectors
    srand(rank * 2000);
    for (int i = 0; i < kVectorSize; ++i) {
        a[i] = static_cast<double>(rand()) / RAND_MAX;
        b[i] = static_cast<double>(rand()) / RAND_MAX;
    }
    
    // Perform various vector operations
    double dot_result = 0.0;
    for (int iter = 0; iter < kNumIterations * 10; ++iter) {
        vector_add(a.data(), b.data(), c.data(), kVectorSize);
        vector_scale(c.data(), c.data(), 0.5, kVectorSize);
        dot_result += vector_dot_product(a.data(), c.data(), kVectorSize);
    }
    
    printf("[Rank %d] Vector operations complete (dot product sum: %.6f)\n",
           rank, dot_result);
}

// ============================================================================
// MPI Collective Operations
// ============================================================================

/// Test MPI collective operations
/// @param rank MPI rank
/// @param size Number of MPI processes
void test_mpi_collectives(int rank, int size) {
    printf("[Rank %d] Testing MPI collective operations...\n", rank);
    
    // Test MPI_Bcast
    std::vector<double> bcast_data(1000);
    if (rank == 0) {
        for (int i = 0; i < 1000; ++i) {
            bcast_data[i] = static_cast<double>(i);
        }
    }
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        MPI_Bcast(bcast_data.data(), 1000, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
    
    // Test MPI_Reduce
    std::vector<double> send_data(1000);
    std::vector<double> recv_data(1000);
    
    for (int i = 0; i < 1000; ++i) {
        send_data[i] = static_cast<double>(rank + i);
    }
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        MPI_Reduce(send_data.data(), recv_data.data(), 1000, MPI_DOUBLE,
                   MPI_SUM, 0, MPI_COMM_WORLD);
    }
    
    // Test MPI_Allreduce
    double local_value = static_cast<double>(rank + 1);
    double global_sum = 0.0;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        MPI_Allreduce(&local_value, &global_sum, 1, MPI_DOUBLE,
                      MPI_SUM, MPI_COMM_WORLD);
    }
    
    // Test MPI_Gather
    std::vector<double> gather_send(100);
    std::vector<double> gather_recv(size * 100);
    
    for (int i = 0; i < 100; ++i) {
        gather_send[i] = static_cast<double>(rank * 100 + i);
    }
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        MPI_Gather(gather_send.data(), 100, MPI_DOUBLE,
                   gather_recv.data(), 100, MPI_DOUBLE,
                   0, MPI_COMM_WORLD);
    }
    
    // Test MPI_Scatter
    std::vector<double> scatter_send(size * 100);
    std::vector<double> scatter_recv(100);
    
    if (rank == 0) {
        for (int i = 0; i < size * 100; ++i) {
            scatter_send[i] = static_cast<double>(i);
        }
    }
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        MPI_Scatter(scatter_send.data(), 100, MPI_DOUBLE,
                    scatter_recv.data(), 100, MPI_DOUBLE,
                    0, MPI_COMM_WORLD);
    }
    
    // Verify some results
    if (rank == 0) {
        printf("[Rank %d] MPI collectives complete (global sum: %.6f)\n",
               rank, global_sum);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    printf("[Rank %d] MPI collective operations complete\n", rank);
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
    
    printf("[Rank %d] MPI Sampler Test Started (total processes: %d)\n",
           rank, size);
    
    // Give the sampler time to initialize
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Run tests
    test_matrix_multiplication(rank);
    MPI_Barrier(MPI_COMM_WORLD);
    
    test_vector_operations(rank);
    MPI_Barrier(MPI_COMM_WORLD);
    
    test_mpi_collectives(rank, size);
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Final synchronization
    if (rank == 0) {
        printf("\n[Rank 0] All tests completed successfully!\n");
        printf("[Rank 0] Check output files: /tmp/perflow_mpi_rank_*.pflw\n");
    }
    
    // Finalize MPI
    MPI_Finalize();
    
    return 0;
}
