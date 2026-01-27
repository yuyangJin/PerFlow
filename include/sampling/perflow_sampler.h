// Copyright 2024 PerFlow Authors
// Licensed under the MIT License

/**
 * @file perflow_sampler.h
 * @brief C API for the PerFlow MPI Performance Sampler
 *
 * This header provides a C-compatible interface for using the MPI
 * performance sampler in C and C++ applications.
 */

#ifndef PERFLOW_SAMPLING_PERFLOW_SAMPLER_H_
#define PERFLOW_SAMPLING_PERFLOW_SAMPLER_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new MPI sampler instance
 * @return Pointer to new sampler instance, or NULL on failure
 */
void* perflow_sampler_create(void);

/**
 * @brief Initialize the sampler
 * @param sampler Sampler instance
 * @param frequency Sampling frequency in Hz (e.g., 1000 for 1000 samples/sec)
 * @param output_path Output file path prefix (can be NULL for default)
 * @return 0 on success, negative error code on failure
 */
int perflow_sampler_init(void* sampler, int frequency, const char* output_path);

/**
 * @brief Start sampling
 * @param sampler Sampler instance
 * @return 0 on success, negative error code on failure
 */
int perflow_sampler_start(void* sampler);

/**
 * @brief Stop sampling
 * @param sampler Sampler instance
 * @return 0 on success, negative error code on failure
 */
int perflow_sampler_stop(void* sampler);

/**
 * @brief Write sampler output to files
 * @param sampler Sampler instance
 * @return 0 on success, negative error code on failure
 */
int perflow_sampler_write_output(void* sampler);

/**
 * @brief Finalize and destroy the sampler
 * @param sampler Sampler instance
 */
void perflow_sampler_destroy(void* sampler);

/**
 * @brief Get the number of samples collected
 * @param sampler Sampler instance
 * @return Number of samples collected
 */
size_t perflow_sampler_get_sample_count(void* sampler);

#ifdef __cplusplus
}
#endif

#endif  /* PERFLOW_SAMPLING_PERFLOW_SAMPLER_H_ */
