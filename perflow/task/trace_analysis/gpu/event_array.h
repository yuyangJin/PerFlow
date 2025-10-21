/**
 * @file event_array.h
 * @brief GPU-friendly array-based event data structures
 *
 * This file defines array-based data structures for events that are
 * suitable for GPU processing. Instead of using object-oriented classes,
 * we use arrays of primitives that can be efficiently processed in parallel.
 */

#ifndef PERFLOW_EVENT_ARRAY_H
#define PERFLOW_EVENT_ARRAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Event types enumeration
 */
typedef enum {
    EVENT_TYPE_UNKNOWN = 0,
    EVENT_TYPE_ENTER = 1,
    EVENT_TYPE_LEAVE = 2,
    EVENT_TYPE_SEND = 3,
    EVENT_TYPE_RECV = 4,
    EVENT_TYPE_COLLECTIVE = 5,
    EVENT_TYPE_BARRIER = 6,
    EVENT_TYPE_COMPUTE = 7,
    EVENT_TYPE_LOOP = 8,
    EVENT_TYPE_BRANCH = 9
} EventType;

/**
 * @brief Structure of Arrays (SoA) for events
 * 
 * This structure stores events in separate arrays for each field,
 * which is more GPU-friendly than Array of Structures (AoS).
 */
typedef struct {
    int32_t *types;           // Event types
    int32_t *indices;         // Event indices
    int32_t *pids;            // Process IDs
    int32_t *tids;            // Thread IDs
    double *timestamps;       // Event timestamps
    int32_t *replay_pids;     // Replay process IDs
    int32_t num_events;       // Total number of events
} EventArray;

/**
 * @brief Structure of Arrays (SoA) for MPI point-to-point events
 * 
 * Additional fields specific to MPI send/receive events.
 */
typedef struct {
    int32_t *communicators;   // MPI communicator IDs
    int32_t *tags;            // Message tags
    int32_t *partner_pids;    // Send: dest_pid, Recv: src_pid
    int32_t *partner_indices; // Index of matching send/recv event
    int32_t num_events;       // Total number of MPI events
} MpiEventArray;

/**
 * @brief Combined event structure for GPU processing
 * 
 * This structure combines base event data with MPI-specific data.
 */
typedef struct {
    EventArray base;
    MpiEventArray mpi;
} GPUEventData;

/**
 * @brief Search map for finding matching events
 * 
 * Maps (pid, tag, communicator) to event index for fast lookup.
 * Implemented as parallel arrays for GPU processing.
 */
typedef struct {
    int32_t *keys_pid;        // Process IDs in map
    int32_t *keys_tag;        // Tags in map
    int32_t *keys_comm;       // Communicator IDs in map
    int32_t *values;          // Event indices
    int32_t size;             // Number of entries in map
    int32_t capacity;         // Allocated capacity
} EventSearchMap;

/**
 * @brief Analysis results structure
 * 
 * Stores results from GPU analysis (e.g., late sender detection).
 */
typedef struct {
    int32_t *late_send_indices;  // Indices of late send events
    double *wait_times;           // Wait time for each event
    int32_t num_late_sends;       // Number of late sends detected
    double total_wait_time;       // Sum of all wait times
} AnalysisResults;

#ifdef __cplusplus
}
#endif

#endif // PERFLOW_EVENT_ARRAY_H
