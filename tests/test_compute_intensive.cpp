// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file test_compute_intensive.cpp
/// @brief Test sampler with CPU-bound computations
///
/// This test program validates the MPI sampler's ability to collect
/// performance data during compute-intensive workloads including:
/// - Dense matrix multiplication (500x500 matrices)
/// - Floating-point intensive operations
/// - Iterative numerical methods

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

// ============================================================================
// Configuration
// ============================================================================

constexpr int kMatrixSize = 500;
constexpr int kVectorSize = 50000;
constexpr int kNumIterations = 5;

// ============================================================================
// Matrix Operations
// ============================================================================

/// Dense matrix multiplication: C = A * B
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
void init_matrix(double* matrix, int size, int seed) {
    srand(seed);
    for (int i = 0; i < size * size; ++i) {
        matrix[i] = static_cast<double>(rand()) / RAND_MAX;
    }
}

// ============================================================================
// Floating-Point Intensive Operations
// ============================================================================

/// Simple FFT-like operations (butterfly operations)
void fft_operations(double* data, int size) {
    for (int step = 1; step < size; step *= 2) {
        for (int i = 0; i < size; i += 2 * step) {
            for (int j = 0; j < step; ++j) {
                int idx1 = i + j;
                int idx2 = i + j + step;
                if (idx2 < size) {
                    double temp1 = data[idx1];
                    double temp2 = data[idx2];
                    data[idx1] = temp1 + temp2;
                    data[idx2] = temp1 - temp2;
                }
            }
        }
    }
}

/// Trigonometric operations
double trigonometric_ops(int iterations) {
    double sum = 0.0;
    for (int i = 0; i < iterations; ++i) {
        double x = static_cast<double>(i) * 0.01;
        sum += sin(x) * cos(x) + tan(x / 2.0);
        sum += sqrt(fabs(sum / (i + 1)));
    }
    return sum;
}

// ============================================================================
// Iterative Numerical Methods
// ============================================================================

/// Jacobi iteration for solving linear system
void jacobi_iteration(const double* A, const double* b, double* x, 
                     double* x_new, int size, int iterations) {
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < size; ++i) {
            double sum = 0.0;
            for (int j = 0; j < size; ++j) {
                if (i != j) {
                    sum += A[i * size + j] * x[j];
                }
            }
            x_new[i] = (b[i] - sum) / (A[i * size + i] + 1e-10);
        }
        // Copy x_new to x
        for (int i = 0; i < size; ++i) {
            x[i] = x_new[i];
        }
    }
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
        printf("Compute-Intensive Test\n");
        printf("=================================================\n");
        printf("MPI Processes: %d\n", size);
        printf("Matrix Size: %dx%d\n", kMatrixSize, kMatrixSize);
        printf("Vector Size: %d\n", kVectorSize);
        printf("Iterations: %d\n", kNumIterations);
        printf("=================================================\n\n");
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    
    // ========================================================================
    // Test 1: Dense Matrix Multiplication
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 1: Matrix multiplication...\n", rank);
    }
    
    std::vector<double> A(kMatrixSize * kMatrixSize);
    std::vector<double> B(kMatrixSize * kMatrixSize);
    std::vector<double> C(kMatrixSize * kMatrixSize);
    
    init_matrix(A.data(), kMatrixSize, rank * 1000);
    init_matrix(B.data(), kMatrixSize, rank * 1000 + 1);
    
    double mat_start = MPI_Wtime();
    for (int iter = 0; iter < kNumIterations; ++iter) {
        matrix_multiply(A.data(), B.data(), C.data(), kMatrixSize);
    }
    double mat_time = MPI_Wtime() - mat_start;
    
    // Compute checksum
    double mat_sum = 0.0;
    for (int i = 0; i < kMatrixSize * kMatrixSize; ++i) {
        mat_sum += C[i];
    }
    
    if (rank == 0) {
        printf("[Rank %d] Matrix multiplication: %.4f seconds (checksum: %.2e)\n", 
               rank, mat_time, mat_sum);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 2: FFT-like Operations
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 2: FFT-like operations...\n", rank);
    }
    
    std::vector<double> fft_data(kVectorSize);
    for (int i = 0; i < kVectorSize; ++i) {
        fft_data[i] = sin(2.0 * M_PI * i / kVectorSize);
    }
    
    double fft_start = MPI_Wtime();
    for (int iter = 0; iter < kNumIterations * 10; ++iter) {
        fft_operations(fft_data.data(), kVectorSize);
    }
    double fft_time = MPI_Wtime() - fft_start;
    
    double fft_sum = 0.0;
    for (int i = 0; i < kVectorSize; ++i) {
        fft_sum += fft_data[i];
    }
    
    if (rank == 0) {
        printf("[Rank %d] FFT operations: %.4f seconds (checksum: %.2e)\n", 
               rank, fft_time, fft_sum);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 3: Trigonometric Operations
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 3: Trigonometric operations...\n", rank);
    }
    
    double trig_start = MPI_Wtime();
    double trig_result = 0.0;
    for (int iter = 0; iter < kNumIterations; ++iter) {
        trig_result += trigonometric_ops(100000);
    }
    double trig_time = MPI_Wtime() - trig_start;
    
    if (rank == 0) {
        printf("[Rank %d] Trigonometric ops: %.4f seconds (result: %.2e)\n", 
               rank, trig_time, trig_result);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Test 4: Jacobi Iteration
    // ========================================================================
    if (rank == 0) {
        printf("[Rank %d] Test 4: Jacobi iteration...\n", rank);
    }
    
    const int jacobi_size = 200;
    std::vector<double> A_jacobi(jacobi_size * jacobi_size);
    std::vector<double> b_jacobi(jacobi_size);
    std::vector<double> x_jacobi(jacobi_size, 0.0);
    std::vector<double> x_new_jacobi(jacobi_size);
    
    // Initialize diagonal-dominant matrix
    for (int i = 0; i < jacobi_size; ++i) {
        for (int j = 0; j < jacobi_size; ++j) {
            if (i == j) {
                A_jacobi[i * jacobi_size + j] = 10.0;
            } else {
                A_jacobi[i * jacobi_size + j] = 1.0 / (jacobi_size + 1);
            }
        }
        b_jacobi[i] = static_cast<double>(i + 1);
    }
    
    double jacobi_start = MPI_Wtime();
    jacobi_iteration(A_jacobi.data(), b_jacobi.data(), x_jacobi.data(),
                    x_new_jacobi.data(), jacobi_size, 100);
    double jacobi_time = MPI_Wtime() - jacobi_start;
    
    double jacobi_sum = 0.0;
    for (int i = 0; i < jacobi_size; ++i) {
        jacobi_sum += x_jacobi[i];
    }
    
    if (rank == 0) {
        printf("[Rank %d] Jacobi iteration: %.4f seconds (solution sum: %.2e)\n", 
               rank, jacobi_time, jacobi_sum);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // ========================================================================
    // Summary
    // ========================================================================
    double total_time = MPI_Wtime() - start_time;
    
    if (rank == 0) {
        printf("\n=================================================\n");
        printf("Compute-Intensive Test Summary\n");
        printf("=================================================\n");
        printf("Matrix multiplication:   %.4f seconds\n", mat_time);
        printf("FFT operations:          %.4f seconds\n", fft_time);
        printf("Trigonometric ops:       %.4f seconds\n", trig_time);
        printf("Jacobi iteration:        %.4f seconds\n", jacobi_time);
        printf("Total time:              %.4f seconds\n", total_time);
        printf("=================================================\n");
        printf("\nExpected Validation Points:\n");
        printf("- High PAPI_FP_OPS counts\n");
        printf("- High PAPI_TOT_INS counts\n");
        printf("- Low MPI communication counters\n");
        printf("- Consistent sampling across all ranks\n");
        printf("\nTest completed successfully!\n");
        printf("Check output files: /tmp/perflow_mpi_rank_*.pflw\n");
    }
    
    // Finalize MPI
    MPI_Finalize();
    
    return 0;
}
