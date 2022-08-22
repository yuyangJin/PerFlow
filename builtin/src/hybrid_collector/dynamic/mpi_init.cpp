
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _EXTERN_C_
#ifdef __cplusplus
#define _EXTERN_C_ extern "C"
#else /* __cplusplus */
#define _EXTERN_C_
#endif /* __cplusplus */
#endif /* _EXTERN_C_ */

#ifdef MPICH_HAS_C2F
_EXTERN_C_ void *MPIR_ToPointer(int);
#endif // MPICH_HAS_C2F

#ifdef PIC
/* For shared libraries, declare these weak and figure out which one was linked
   based on which init wrapper was called.  See mpi_init wrappers.  */
#pragma weak pmpi_init
#pragma weak PMPI_INIT
#pragma weak pmpi_init_
#pragma weak pmpi_init__
#endif /* PIC */

_EXTERN_C_ void pmpi_init(MPI_Fint *ierr);
_EXTERN_C_ void PMPI_INIT(MPI_Fint *ierr);
_EXTERN_C_ void pmpi_init_(MPI_Fint *ierr);
_EXTERN_C_ void pmpi_init__(MPI_Fint *ierr);

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
// In BaguaTool, we link mpi_init.o with other .o files to generate get runtime
// data.
//
#include "mpi_init.h"
#include <iostream>
#include <mpi.h>

using namespace std;
// int mpi_rank = 0;

// MPI_Init does all the communicator setup
//
static int fortran_init = 0;
/* ================== C Wrappers for MPI_Init ================== */
_EXTERN_C_ int PMPI_Init(int *argc, char ***argv);
_EXTERN_C_ int MPI_Init(int *argc, char ***argv) {
  int _wrap_py_return_val = 0;
  {
    // First call PMPI_Init()
    if (fortran_init) {
#ifdef PIC
      if (!PMPI_INIT && !pmpi_init && !pmpi_init_ && !pmpi_init__) {
        fprintf(stderr, "ERROR: Couldn't find fortran pmpi_init function.  "
                        "Link against static library instead.\n");
        exit(1);
      }
      switch (fortran_init) {
      case 1:
        PMPI_INIT(&_wrap_py_return_val);
        break;
      case 2:
        pmpi_init(&_wrap_py_return_val);
        break;
      case 3:
        pmpi_init_(&_wrap_py_return_val);
        break;
      case 4:
        pmpi_init__(&_wrap_py_return_val);
        break;
      default:
        fprintf(stderr, "NO SUITABLE FORTRAN MPI_INIT BINDING\n");
        break;
      }
#else  /* !PIC */
      pmpi_init_(&_wrap_py_return_val);
#endif /* !PIC */
    } else {
      _wrap_py_return_val = PMPI_Init(argc, argv);
    }

    PMPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    // cout << "mpi_rank = "<< mpi_rank << "\n";
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Init =============== */
static void MPI_Init_fortran_wrapper(MPI_Fint *ierr) {
  int argc = 0;
  char **argv = NULL;
  int _wrap_py_return_val = 0;
  _wrap_py_return_val = MPI_Init(&argc, &argv);
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_INIT(MPI_Fint *ierr) {
  fortran_init = 1;
  MPI_Init_fortran_wrapper(ierr);
}

_EXTERN_C_ void mpi_init(MPI_Fint *ierr) {
  fortran_init = 2;
  MPI_Init_fortran_wrapper(ierr);
}

_EXTERN_C_ void mpi_init_(MPI_Fint *ierr) {
  fortran_init = 3;
  MPI_Init_fortran_wrapper(ierr);
}

_EXTERN_C_ void mpi_init__(MPI_Fint *ierr) {
  fortran_init = 4;
  MPI_Init_fortran_wrapper(ierr);
}

/* ================= End Wrappers for MPI_Init ================= */

/* ================== C Wrappers for MPI_Init_thread ================== */
_EXTERN_C_ int PMPI_Init_thread(int *argc, char ***argv, int required,
                                int *provided);
_EXTERN_C_ int MPI_Init_thread(int *argc, char ***argv, int required,
                               int *provided) {
  int _wrap_py_return_val = 0;
  {
    // First call PMPI_Init()
    _wrap_py_return_val = PMPI_Init_thread(argc, argv, required, provided);

    PMPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    // cout << "mpi_rank = "<< mpi_rank << "\n";
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Init_thread =============== */
static void MPI_Init_thread_fortran_wrapper(MPI_Fint *argc, MPI_Fint ***argv,
                                            MPI_Fint *required,
                                            MPI_Fint *provided,
                                            MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
  _wrap_py_return_val =
      MPI_Init_thread((int *)argc, (char ***)argv, *required, (int *)provided);
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_INIT_THREAD(MPI_Fint *argc, MPI_Fint ***argv,
                                MPI_Fint *required, MPI_Fint *provided,
                                MPI_Fint *ierr) {
  MPI_Init_thread_fortran_wrapper(argc, argv, required, provided, ierr);
}

_EXTERN_C_ void mpi_init_thread(MPI_Fint *argc, MPI_Fint ***argv,
                                MPI_Fint *required, MPI_Fint *provided,
                                MPI_Fint *ierr) {
  MPI_Init_thread_fortran_wrapper(argc, argv, required, provided, ierr);
}

_EXTERN_C_ void mpi_init_thread_(MPI_Fint *argc, MPI_Fint ***argv,
                                 MPI_Fint *required, MPI_Fint *provided,
                                 MPI_Fint *ierr) {
  MPI_Init_thread_fortran_wrapper(argc, argv, required, provided, ierr);
}

_EXTERN_C_ void mpi_init_thread__(MPI_Fint *argc, MPI_Fint ***argv,
                                  MPI_Fint *required, MPI_Fint *provided,
                                  MPI_Fint *ierr) {
  MPI_Init_thread_fortran_wrapper(argc, argv, required, provided, ierr);
}

/* ================= End Wrappers for MPI_Init_thread ================= */

/* ================== C Wrappers for MPI_Reduce ================== */
_EXTERN_C_ int PMPI_Reduce(const void *sendbuf, void *recvbuf, int count,
                           MPI_Datatype datatype, MPI_Op op, int root,
                           MPI_Comm comm);
_EXTERN_C_ int MPI_Reduce(const void *sendbuf, void *recvbuf, int count,
                          MPI_Datatype datatype, MPI_Op op, int root,
                          MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call PMPI_Init()
    _wrap_py_return_val =
        PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);

    PMPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    // cout << "mpi_rank = "<< mpi_rank << "\n";
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Reduce =============== */
static void MPI_Reduce_fortran_wrapper(MPI_Fint *sendbuf, MPI_Fint *recvbuf,
                                       MPI_Fint *count, MPI_Fint *datatype,
                                       MPI_Fint *op, MPI_Fint *root,
                                       MPI_Fint *comm, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) &&                         \
     (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Reduce((const void *)sendbuf, (void *)recvbuf,
                                   *count, (MPI_Datatype)(*datatype),
                                   (MPI_Op)(*op), *root, (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Reduce((const void *)sendbuf, (void *)recvbuf,
                                   *count, MPI_Type_f2c(*datatype),
                                   MPI_Op_f2c(*op), *root, MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_REDUCE(MPI_Fint *sendbuf, MPI_Fint *recvbuf,
                           MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                           MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Reduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, root, comm,
                             ierr);
}

_EXTERN_C_ void mpi_reduce(MPI_Fint *sendbuf, MPI_Fint *recvbuf,
                           MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                           MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Reduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, root, comm,
                             ierr);
}

_EXTERN_C_ void mpi_reduce_(MPI_Fint *sendbuf, MPI_Fint *recvbuf,
                            MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                            MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Reduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, root, comm,
                             ierr);
}

_EXTERN_C_ void mpi_reduce__(MPI_Fint *sendbuf, MPI_Fint *recvbuf,
                             MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                             MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Reduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, root, comm,
                             ierr);
}

/* ================= End Wrappers for MPI_Reduce ================= */

/* ================== C Wrappers for MPI_Send ================== */
_EXTERN_C_ int PMPI_Send(const void *buf, int count, MPI_Datatype datatype,
                         int dest, int tag, MPI_Comm comm);
_EXTERN_C_ int MPI_Send(const void *buf, int count, MPI_Datatype datatype,
                        int dest, int tag, MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call PMPI_Init()
    _wrap_py_return_val = PMPI_Send(buf, count, datatype, dest, tag, comm);

    PMPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    // cout << "mpi_rank = "<< mpi_rank << "\n";
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Send =============== */
static void MPI_Send_fortran_wrapper(MPI_Fint *buf, MPI_Fint *count,
                                     MPI_Fint *datatype, MPI_Fint *dest,
                                     MPI_Fint *tag, MPI_Fint *comm,
                                     MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) &&                         \
     (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val =
      MPI_Send((const void *)buf, *count, (MPI_Datatype)(*datatype), *dest,
               *tag, (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val =
      MPI_Send((const void *)buf, *count, MPI_Type_f2c(*datatype), *dest, *tag,
               MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_SEND(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype,
                         MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                         MPI_Fint *ierr) {
  MPI_Send_fortran_wrapper(buf, count, datatype, dest, tag, comm, ierr);
}

_EXTERN_C_ void mpi_send(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype,
                         MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                         MPI_Fint *ierr) {
  MPI_Send_fortran_wrapper(buf, count, datatype, dest, tag, comm, ierr);
}

_EXTERN_C_ void mpi_send_(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype,
                          MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                          MPI_Fint *ierr) {
  MPI_Send_fortran_wrapper(buf, count, datatype, dest, tag, comm, ierr);
}

_EXTERN_C_ void mpi_send__(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype,
                           MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                           MPI_Fint *ierr) {
  MPI_Send_fortran_wrapper(buf, count, datatype, dest, tag, comm, ierr);
}

/* ================= End Wrappers for MPI_Send ================= */
