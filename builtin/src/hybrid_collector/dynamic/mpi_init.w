// -*- c++ -*-
//
// mpi_init
// Yuyang Jin
//
// This file is to generate code to get mpi rank of each process.
//
// To build:
//    ./wrap.py -f mpi_init.w -o mpi_init.cpp
//    mpicc -c mpi_init.cpp
//    #ar cr libmpi_init.a mpi_init.o
//    #ranlib libmpi_init.a
//
// Link your application with libcommData.a, or build it as a shared lib
// and LD_PRELOAD it to try out this tool.
//
// In BaguaTool, we link mpi_init.o with other .o files to generate get runtime data.
//
#include <mpi.h>
#include "mpi_init.h"
#include <iostream>

using namespace std;
//int mpi_rank = 0;

// MPI_Init does all the communicator setup
//
{{fn func MPI_Init MPI_Init_thread MPI_Reduce MPI_Send}}{
    // First call PMPI_Init()
    {{callfn}}

    PMPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    //cout << "mpi_rank = "<< mpi_rank << "\n";


}{{endfn}}
