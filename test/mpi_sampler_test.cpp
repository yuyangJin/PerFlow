// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

/**
 * @file mpi_sampler_test.cpp
 * @brief Test application for MPI Performance Sampler (Dynamic Instrumentation)
 *
 * This is a simple MPI program that performs computational workloads.
 * It does NOT contain any instrumentation code - instead, it is designed
 * to be profiled using the LD_PRELOAD mechanism with libperflow_sampler.so.
 *
 * Usage with dynamic instrumentation:
 *   export PERFLOW_ENABLE_SAMPLING=1
 *   export PERFLOW_SAMPLING_FREQ=1000
 *   export PERFLOW_OUTPUT_PATH=./samples
 *   mpirun -np 4 -x LD_PRELOAD=/path/to/libperflow_sampler.so ./mpi_sampler_test
 *
 * The test performs:
 * - Matrix multiplication (100x100)
 * - Vector operations
 * - Collective operations (MPI_Bcast, MPI_Reduce)
 */

#include <mpi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

constexpr int MATRIX_SIZE = 100;
constexpr int VECTOR_SIZE = 10000;
constexpr int NUM_ITERATIONS = 10;

/**
 * @brief Matrix multiplication: C = A * B
 */
void matrix_multiply(const std::vector<double>& A,
                     const std::vector<double>& B,
                     std::vector<double>& C,
                     int n) {
  std::fill(C.begin(), C.end(), 0.0);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      double sum = 0.0;
      for (int k = 0; k < n; ++k) {
        sum += A[i * n + k] * B[k * n + j];
      }
      C[i * n + j] = sum;
    }
  }
}

/**
 * @brief Vector dot product
 */
double vector_dot_product(const std::vector<double>& a,
                          const std::vector<double>& b) {
  double result = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    result += a[i] * b[i];
  }
  return result;
}

/**
 * @brief Vector addition: c = a + b
 */
void vector_add(const std::vector<double>& a,
                const std::vector<double>& b,
                std::vector<double>& c) {
  for (size_t i = 0; i < a.size(); ++i) {
    c[i] = a[i] + b[i];
  }
}

/**
 * @brief Vector scalar multiply: b = alpha * a
 */
void vector_scale(const std::vector<double>& a, double alpha,
                  std::vector<double>& b) {
  for (size_t i = 0; i < a.size(); ++i) {
    b[i] = alpha * a[i];
  }
}

/**
 * @brief Perform matrix operations
 */
void do_matrix_operations(int rank) {
  std::vector<double> A(MATRIX_SIZE * MATRIX_SIZE);
  std::vector<double> B(MATRIX_SIZE * MATRIX_SIZE);
  std::vector<double> C(MATRIX_SIZE * MATRIX_SIZE);

  // Initialize matrices with some data
  for (int i = 0; i < MATRIX_SIZE * MATRIX_SIZE; ++i) {
    A[i] = static_cast<double>(i % 100) / 100.0 + rank;
    B[i] = static_cast<double>((i + 1) % 100) / 100.0;
  }

  // Perform matrix multiplication multiple times
  for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
    matrix_multiply(A, B, C, MATRIX_SIZE);
    // Use result to prevent optimization
    if (C[0] < -1e10) {
      std::cout << "Unexpected result" << std::endl;
    }
  }
}

/**
 * @brief Perform vector operations
 */
double do_vector_operations(int rank) {
  std::vector<double> a(VECTOR_SIZE);
  std::vector<double> b(VECTOR_SIZE);
  std::vector<double> c(VECTOR_SIZE);

  // Initialize vectors
  for (int i = 0; i < VECTOR_SIZE; ++i) {
    a[i] = static_cast<double>(i) / VECTOR_SIZE + rank;
    b[i] = static_cast<double>(VECTOR_SIZE - i) / VECTOR_SIZE;
  }

  double total = 0.0;
  for (int iter = 0; iter < NUM_ITERATIONS * 10; ++iter) {
    // Dot product
    double dot = vector_dot_product(a, b);
    total += dot;

    // Vector addition
    vector_add(a, b, c);

    // Vector scaling
    vector_scale(c, 0.5, a);

    // Restore a for next iteration
    for (int i = 0; i < VECTOR_SIZE; ++i) {
      a[i] = static_cast<double>(i) / VECTOR_SIZE + rank;
    }
  }

  return total;
}

/**
 * @brief Perform MPI collective operations
 */
void do_collective_operations(int rank) {
  std::vector<double> data(1000);
  std::vector<double> reduced(1000);

  // Initialize data
  for (int i = 0; i < 1000; ++i) {
    data[i] = static_cast<double>(rank * 1000 + i);
  }

  for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
    // Broadcast from root
    MPI_Bcast(data.data(), 1000, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Modify local data
    for (int i = 0; i < 1000; ++i) {
      data[i] += rank;
    }

    // Reduce to root
    MPI_Reduce(data.data(), reduced.data(), 1000, MPI_DOUBLE, MPI_SUM, 0,
               MPI_COMM_WORLD);

    // All-reduce
    MPI_Allreduce(data.data(), reduced.data(), 1000, MPI_DOUBLE, MPI_SUM,
                  MPI_COMM_WORLD);

    // Barrier
    MPI_Barrier(MPI_COMM_WORLD);

    // Reset data for next iteration
    for (int i = 0; i < 1000; ++i) {
      data[i] = static_cast<double>(rank * 1000 + i);
    }
  }
}

int main(int argc, char* argv[]) {
  // Initialize MPI
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0) {
    std::cout << "=== MPI Test Program ===" << std::endl;
    std::cout << "Running with " << size << " processes" << std::endl;
    std::cout << "This program is designed to be profiled with LD_PRELOAD" << std::endl;
    std::cout << std::endl;
  }

  // Synchronize before starting workload
  MPI_Barrier(MPI_COMM_WORLD);

  auto start_time = std::chrono::high_resolution_clock::now();

  if (rank == 0) {
    std::cout << "Starting workload..." << std::endl;
  }

  // Perform workload
  do_matrix_operations(rank);
  double vec_result = do_vector_operations(rank);
  do_collective_operations(rank);

  // Use result to prevent optimization
  if (vec_result < -1e10) {
    std::cout << "Unexpected vector result" << std::endl;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                      end_time - start_time)
                      .count();

  // Synchronize before reporting
  MPI_Barrier(MPI_COMM_WORLD);

  if (rank == 0) {
    std::cout << "Workload completed in " << duration << " ms" << std::endl;
    std::cout << "=== Test Complete ===" << std::endl;
  }

  MPI_Finalize();
  return 0;
}
