// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file mpi_init_setup.cpp
/// @brief MPI initialization interception for C/C++ and Fortran
///
/// This file implements MPI_Init and MPI_Init_thread interception for both
/// C/C++ and Fortran language bindings. It handles all Fortran name mangling
/// conventions and captures the MPI rank for use by the sampler.

#define _GNU_SOURCE

#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

#include "mpi_init_setup.h"

// ============================================================================
// Global State
// ============================================================================

/// MPI rank of current process (-1 if not initialized)
int g_mpi_rank = -1;

// ============================================================================
// MPI Function Interception
// ============================================================================

// Original MPI_Init function pointer
static int (*real_MPI_Init)(int*, char***) = nullptr;

// MPI Fortran integer type
#ifndef MPI_Fint
typedef int MPI_Fint;
#endif

// Fortran init tracking
static int fortran_init = 0;

// Weak symbols for Fortran PMPI functions (for shared libraries)
#ifdef PIC
#pragma weak pmpi_init
#pragma weak PMPI_INIT
#pragma weak pmpi_init_
#pragma weak pmpi_init__
#endif

// Forward declarations for Fortran PMPI functions
extern "C" void pmpi_init(MPI_Fint *ierr);
extern "C" void PMPI_INIT(MPI_Fint *ierr);
extern "C" void pmpi_init_(MPI_Fint *ierr);
extern "C" void pmpi_init__(MPI_Fint *ierr);

/// Intercepted MPI_Init to capture rank early
extern "C" int MPI_Init(int* argc, char*** argv) {
    // Handle Fortran initialization if flagged
    if (fortran_init) {
#ifdef PIC
        if (!PMPI_INIT && !pmpi_init && !pmpi_init_ && !pmpi_init__) {
            fprintf(stderr, "[MPI Sampler] ERROR: Couldn't find fortran pmpi_init function. "
                            "Link against static library instead.\n");
            exit(1);
        }
        MPI_Fint ierr = 0;
        switch (fortran_init) {
        case 1:
            PMPI_INIT(&ierr);
            break;
        case 2:
            pmpi_init(&ierr);
            break;
        case 3:
            pmpi_init_(&ierr);
            break;
        case 4:
            pmpi_init__(&ierr);
            break;
        default:
            fprintf(stderr, "[MPI Sampler] NO SUITABLE FORTRAN MPI_INIT BINDING\n");
            break;
        }
        // Reset fortran_init flag after calling PMPI
        int result = static_cast<int>(ierr);
        fortran_init = 0;  // Reset flag to prevent reuse
        
        // If successful, get our rank
        if (result == 0) {
            MPI_Comm_rank(MPI_COMM_WORLD, &g_mpi_rank);
            fprintf(stderr, "[MPI Sampler] Captured MPI rank: %d (Fortran init variant %d)\n", 
                    g_mpi_rank, fortran_init);
        }
        
        return result;
#else  /* !PIC */
        MPI_Fint ierr = 0;
        pmpi_init_(&ierr);
        int result = static_cast<int>(ierr);
        fortran_init = 0;  // Reset flag to prevent reuse
        
        // If successful, get our rank
        if (result == 0) {
            MPI_Comm_rank(MPI_COMM_WORLD, &g_mpi_rank);
            fprintf(stderr, "[MPI Sampler] Captured MPI rank: %d (Fortran static init)\n", 
                    g_mpi_rank);
        }
        
        return result;
#endif /* !PIC */
    }
    
    // Handle C/C++ initialization
    // Call the real MPI_Init if we have the pointer
    if (real_MPI_Init == nullptr) {
        // Get the real MPI_Init function
        real_MPI_Init = (int (*)(int*, char***))dlsym(RTLD_NEXT, "MPI_Init");
        if (real_MPI_Init == nullptr) {
            fprintf(stderr, "[MPI Sampler] Error: Could not find real MPI_Init\n");
            return -1;
        }
    }
    
    // Call real MPI_Init
    int result = real_MPI_Init(argc, argv);
    
    // If successful, get our rank
    if (result == 0) {
        MPI_Comm_rank(MPI_COMM_WORLD, &g_mpi_rank);
        fprintf(stderr, "[MPI Sampler] Captured MPI rank: %d (C/C++ init)\n", g_mpi_rank);
    }
    
    return result;
}

// ============================================================================
// Fortran MPI_Init Wrappers
// ============================================================================

/// Fortran wrapper helper function
static void MPI_Init_fortran_wrapper(MPI_Fint *ierr) {
    int argc = 0;
    char **argv = nullptr;
    int result = MPI_Init(&argc, &argv);
    *ierr = static_cast<MPI_Fint>(result);
}

/// Fortran MPI_Init wrappers for different name mangling conventions
extern "C" void MPI_INIT(MPI_Fint *ierr) {
    fortran_init = 1;
    MPI_Init_fortran_wrapper(ierr);
}

extern "C" void mpi_init(MPI_Fint *ierr) {
    fortran_init = 2;
    MPI_Init_fortran_wrapper(ierr);
}

extern "C" void mpi_init_(MPI_Fint *ierr) {
    fortran_init = 3;
    MPI_Init_fortran_wrapper(ierr);
}

extern "C" void mpi_init__(MPI_Fint *ierr) {
    fortran_init = 4;
    MPI_Init_fortran_wrapper(ierr);
}

// ============================================================================
// MPI_Init_thread Support
// ============================================================================

// Original MPI_Init_thread function pointer
static int (*real_MPI_Init_thread)(int*, char***, int, int*) = nullptr;

/// Intercepted MPI_Init_thread to capture rank early
extern "C" int MPI_Init_thread(int* argc, char*** argv, int required, int* provided) {
    // Note: Fortran MPI_Init_thread has different signature and is handled separately
    // through the Fortran wrappers below. If fortran_init is set here, it means
    // there's an unexpected call path - just clear the flag and proceed with C/C++ init.
    if (fortran_init) {
        fprintf(stderr, "[MPI Sampler] Warning: Unexpected fortran_init flag in C MPI_Init_thread, clearing\n");
        fortran_init = 0;
    }
    
    // Handle C/C++ initialization
    if (real_MPI_Init_thread == nullptr) {
        real_MPI_Init_thread = (int (*)(int*, char***, int, int*))dlsym(RTLD_NEXT, "MPI_Init_thread");
        if (real_MPI_Init_thread == nullptr) {
            fprintf(stderr, "[MPI Sampler] Error: Could not find real MPI_Init_thread\n");
            return -1;
        }
    }
    
    // Call real MPI_Init_thread
    int result = real_MPI_Init_thread(argc, argv, required, provided);
    
    // If successful, get our rank
    if (result == 0) {
        MPI_Comm_rank(MPI_COMM_WORLD, &g_mpi_rank);
        fprintf(stderr, "[MPI Sampler] Captured MPI rank: %d (C/C++ MPI_Init_thread)\n", g_mpi_rank);
    }
    
    return result;
}

// ============================================================================
// Fortran MPI_Init_thread Wrappers
// ============================================================================

/// Fortran MPI_Init_thread wrapper helper function
/// Note: Fortran MPI_Init_thread has simpler signature - just required, provided, and ierr
static void MPI_Init_thread_fortran_wrapper(MPI_Fint *required, MPI_Fint *provided, MPI_Fint *ierr) {
    int argc = 0;
    char **argv = nullptr;
    int c_required = static_cast<int>(*required);
    int c_provided = 0;
    
    // Note: This calls the C MPI_Init_thread wrapper above, which will handle rank capture
    int result = MPI_Init_thread(&argc, &argv, c_required, &c_provided);
    
    *provided = static_cast<MPI_Fint>(c_provided);
    *ierr = static_cast<MPI_Fint>(result);
}

/// Fortran MPI_Init_thread wrappers for different name mangling conventions
extern "C" void MPI_INIT_THREAD(MPI_Fint *required, MPI_Fint *provided, MPI_Fint *ierr) {
    MPI_Init_thread_fortran_wrapper(required, provided, ierr);
}

extern "C" void mpi_init_thread(MPI_Fint *required, MPI_Fint *provided, MPI_Fint *ierr) {
    MPI_Init_thread_fortran_wrapper(required, provided, ierr);
}

extern "C" void mpi_init_thread_(MPI_Fint *required, MPI_Fint *provided, MPI_Fint *ierr) {
    MPI_Init_thread_fortran_wrapper(required, provided, ierr);
}

extern "C" void mpi_init_thread__(MPI_Fint *required, MPI_Fint *provided, MPI_Fint *ierr) {
    MPI_Init_thread_fortran_wrapper(required, provided, ierr);
}
