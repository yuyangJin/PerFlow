/**
 * @file trace_replay_kernel.cu
 * @brief CUDA kernels for parallel trace replay and analysis
 *
 * This file implements GPU kernels for replaying traces in parallel
 * and performing various trace analysis tasks such as late sender detection.
 */

#include "event_array.h"
#include <cuda_runtime.h>
#include <stdio.h>

// Error checking macro
#define CUDA_CHECK(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d: %s\n", __FILE__, __LINE__, \
                    cudaGetErrorString(error)); \
            return error; \
        } \
    } while(0)

/**
 * @brief Kernel for detecting late senders in parallel
 * 
 * Each thread processes one receive event and checks if its corresponding
 * send event arrived late (send timestamp > receive timestamp).
 * 
 * @param base_types Event types array
 * @param base_timestamps Event timestamps array
 * @param mpi_partner_indices Array mapping recv events to their send event indices
 * @param num_events Total number of events
 * @param late_send_indices Output: indices of late send events
 * @param wait_times Output: wait time for each late send
 * @param num_late_sends Output: counter for number of late sends
 */
__global__ void detect_late_senders_kernel(
    const int32_t *base_types,
    const double *base_timestamps,
    const int32_t *mpi_partner_indices,
    int32_t num_events,
    int32_t *late_send_indices,
    double *wait_times,
    int32_t *num_late_sends)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx >= num_events) return;
    
    // Check if this is a receive event
    if (base_types[idx] == EVENT_TYPE_RECV) {
        int32_t send_idx = mpi_partner_indices[idx];
        
        // Check if send event is matched
        if (send_idx >= 0 && send_idx < num_events) {
            double recv_time = base_timestamps[idx];
            double send_time = base_timestamps[send_idx];
            
            // Late sender: send happens after receive is ready
            if (send_time > recv_time) {
                double wait_time = send_time - recv_time;
                
                // Use atomic operation to safely update results
                int result_idx = atomicAdd(num_late_sends, 1);
                
                if (result_idx < num_events) {  // Bounds check
                    late_send_indices[result_idx] = send_idx;
                    wait_times[result_idx] = wait_time;
                }
            }
        }
    }
}

/**
 * @brief Kernel for detecting late receivers in parallel
 * 
 * Each thread processes one send event and checks if its corresponding
 * receive event was late (receive timestamp much later than send timestamp).
 * 
 * @param base_types Event types array
 * @param base_timestamps Event timestamps array
 * @param mpi_partner_indices Array mapping send events to their recv event indices
 * @param num_events Total number of events
 * @param late_recv_indices Output: indices of late receive events
 * @param wait_times Output: wait time for each late receive
 * @param num_late_recvs Output: counter for number of late receives
 */
__global__ void detect_late_receivers_kernel(
    const int32_t *base_types,
    const double *base_timestamps,
    const int32_t *mpi_partner_indices,
    int32_t num_events,
    int32_t *late_recv_indices,
    double *wait_times,
    int32_t *num_late_recvs)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx >= num_events) return;
    
    // Check if this is a send event
    if (base_types[idx] == EVENT_TYPE_SEND) {
        int32_t recv_idx = mpi_partner_indices[idx];
        
        // Check if receive event is matched
        if (recv_idx >= 0 && recv_idx < num_events) {
            double send_time = base_timestamps[idx];
            double recv_time = base_timestamps[recv_idx];
            
            // Late receiver: receive happens after send is ready
            if (recv_time > send_time) {
                double wait_time = recv_time - send_time;
                
                // Use atomic operation to safely update results
                int result_idx = atomicAdd(num_late_recvs, 1);
                
                if (result_idx < num_events) {  // Bounds check
                    late_recv_indices[result_idx] = recv_idx;
                    wait_times[result_idx] = wait_time;
                }
            }
        }
    }
}

/**
 * @brief Optimized kernel using shared memory for late sender detection
 * 
 * This version uses shared memory to cache frequently accessed data,
 * improving performance for larger traces.
 * 
 * @param base_types Event types array
 * @param base_timestamps Event timestamps array
 * @param mpi_partner_indices Array mapping recv events to their send event indices
 * @param num_events Total number of events
 * @param late_send_indices Output: indices of late send events
 * @param wait_times Output: wait time for each late send
 * @param num_late_sends Output: counter for number of late sends
 */
__global__ void detect_late_senders_shared_kernel(
    const int32_t *base_types,
    const double *base_timestamps,
    const int32_t *mpi_partner_indices,
    int32_t num_events,
    int32_t *late_send_indices,
    double *wait_times,
    int32_t *num_late_sends)
{
    // Shared memory for caching event data within a block
    extern __shared__ char shared_mem[];
    int32_t *shared_types = (int32_t *)shared_mem;
    double *shared_timestamps = (double *)(shared_mem + blockDim.x * sizeof(int32_t));
    int32_t *shared_partners = (int32_t *)(shared_mem + blockDim.x * (sizeof(int32_t) + sizeof(double)));
    
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int local_idx = threadIdx.x;
    
    // Load data into shared memory
    if (idx < num_events) {
        shared_types[local_idx] = base_types[idx];
        shared_timestamps[local_idx] = base_timestamps[idx];
        shared_partners[local_idx] = mpi_partner_indices[idx];
    }
    
    __syncthreads();
    
    if (idx >= num_events) return;
    
    // Check if this is a receive event
    if (shared_types[local_idx] == EVENT_TYPE_RECV) {
        int32_t send_idx = shared_partners[local_idx];
        
        // Check if send event is matched
        if (send_idx >= 0 && send_idx < num_events) {
            double recv_time = shared_timestamps[local_idx];
            double send_time = base_timestamps[send_idx];  // Global memory access
            
            // Late sender: send happens after receive is ready
            if (send_time > recv_time) {
                double wait_time = send_time - recv_time;
                
                // Use atomic operation to safely update results
                int result_idx = atomicAdd(num_late_sends, 1);
                
                if (result_idx < num_events) {  // Bounds check
                    late_send_indices[result_idx] = send_idx;
                    wait_times[result_idx] = wait_time;
                }
            }
        }
    }
}

/**
 * @brief Reduction kernel to compute total wait time
 * 
 * Uses parallel reduction to sum all wait times efficiently.
 * 
 * @param wait_times Array of individual wait times
 * @param num_items Number of wait times
 * @param total_wait Output: total wait time
 */
__global__ void reduce_sum_kernel(
    const double *wait_times,
    int32_t num_items,
    double *total_wait)
{
    extern __shared__ double shared_data[];
    
    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Load data into shared memory
    shared_data[tid] = (idx < num_items) ? wait_times[idx] : 0.0;
    __syncthreads();
    
    // Parallel reduction in shared memory
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            shared_data[tid] += shared_data[tid + stride];
        }
        __syncthreads();
    }
    
    // Write result for this block
    if (tid == 0) {
        atomicAdd(total_wait, shared_data[0]);
    }
}

// C interface functions for Python binding

extern "C" {

/**
 * @brief Launch late sender detection kernel
 * 
 * @param event_data GPU event data structure
 * @param results Output results structure
 * @param use_shared_mem Whether to use shared memory optimization
 * @return CUDA error code
 */
cudaError_t launch_late_sender_kernel(
    GPUEventData *event_data,
    AnalysisResults *results,
    bool use_shared_mem)
{
    int num_events = event_data->base.num_events;
    int threads_per_block = 256;
    int num_blocks = (num_events + threads_per_block - 1) / threads_per_block;
    
    // Initialize result counter to 0
    int32_t zero = 0;
    CUDA_CHECK(cudaMemcpy(results->late_send_indices, &zero, sizeof(int32_t), cudaMemcpyHostToDevice));
    
    if (use_shared_mem) {
        size_t shared_mem_size = threads_per_block * (2 * sizeof(int32_t) + sizeof(double));
        detect_late_senders_shared_kernel<<<num_blocks, threads_per_block, shared_mem_size>>>(
            event_data->base.types,
            event_data->base.timestamps,
            event_data->mpi.partner_indices,
            num_events,
            results->late_send_indices,
            results->wait_times,
            &results->num_late_sends
        );
    } else {
        detect_late_senders_kernel<<<num_blocks, threads_per_block>>>(
            event_data->base.types,
            event_data->base.timestamps,
            event_data->mpi.partner_indices,
            num_events,
            results->late_send_indices,
            results->wait_times,
            &results->num_late_sends
        );
    }
    
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    
    // Compute total wait time using reduction
    if (results->num_late_sends > 0) {
        double zero_double = 0.0;
        CUDA_CHECK(cudaMemcpy(&results->total_wait_time, &zero_double, sizeof(double), cudaMemcpyHostToDevice));
        
        int reduce_blocks = (results->num_late_sends + threads_per_block - 1) / threads_per_block;
        size_t reduce_shared_mem = threads_per_block * sizeof(double);
        reduce_sum_kernel<<<reduce_blocks, threads_per_block, reduce_shared_mem>>>(
            results->wait_times,
            results->num_late_sends,
            &results->total_wait_time
        );
        
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }
    
    return cudaSuccess;
}

/**
 * @brief Launch late receiver detection kernel
 * 
 * @param event_data GPU event data structure
 * @param results Output results structure
 * @param use_shared_mem Whether to use shared memory optimization
 * @return CUDA error code
 */
cudaError_t launch_late_receiver_kernel(
    GPUEventData *event_data,
    AnalysisResults *results,
    bool use_shared_mem)
{
    int num_events = event_data->base.num_events;
    int threads_per_block = 256;
    int num_blocks = (num_events + threads_per_block - 1) / threads_per_block;
    
    // Initialize result counter to 0
    int32_t zero = 0;
    CUDA_CHECK(cudaMemcpy(results->late_send_indices, &zero, sizeof(int32_t), cudaMemcpyHostToDevice));
    
    detect_late_receivers_kernel<<<num_blocks, threads_per_block>>>(
        event_data->base.types,
        event_data->base.timestamps,
        event_data->mpi.partner_indices,
        num_events,
        results->late_send_indices,  // Reusing same structure, actually recv indices
        results->wait_times,
        &results->num_late_sends     // Reusing same structure, actually num recvs
    );
    
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    
    // Compute total wait time using reduction
    if (results->num_late_sends > 0) {
        double zero_double = 0.0;
        CUDA_CHECK(cudaMemcpy(&results->total_wait_time, &zero_double, sizeof(double), cudaMemcpyHostToDevice));
        
        int threads_per_block_reduce = 256;
        int reduce_blocks = (results->num_late_sends + threads_per_block_reduce - 1) / threads_per_block_reduce;
        size_t reduce_shared_mem = threads_per_block_reduce * sizeof(double);
        reduce_sum_kernel<<<reduce_blocks, threads_per_block_reduce, reduce_shared_mem>>>(
            results->wait_times,
            results->num_late_sends,
            &results->total_wait_time
        );
        
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }
    
    return cudaSuccess;
}

} // extern "C"
