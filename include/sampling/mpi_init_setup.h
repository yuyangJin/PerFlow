// Copyright 2024 PerFlow Authors
// Licensed under the Apache License, Version 2.0

/// @file mpi_init_setup.h
/// @brief MPI initialization interception for C/C++ and Fortran
///
/// This header declares the shared MPI rank variable that is set during
/// MPI initialization and used by the MPI sampler.

#ifndef PERFLOW_MPI_INIT_SETUP_H_
#define PERFLOW_MPI_INIT_SETUP_H_

/// MPI rank of current process (-1 if not initialized)
/// This variable is set by MPI_Init interception in mpi_init_setup.cpp
/// and used by the sampler in mpi_sampler.cpp
extern int g_mpi_rank;

#endif  // PERFLOW_MPI_INIT_SETUP_H_
