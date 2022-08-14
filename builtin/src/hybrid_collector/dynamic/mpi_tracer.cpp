
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
#endif  // MPICH_HAS_C2F

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
// commDependence
// Yuyang Jin
//
// This file is to generate code to collect runtime communication data.
//
// To build:
//    ./wrap.py -f commDependence.w > commDependence.cpp
//    mpicc -c commDependence.cpp
//    #ar cr libcommDependence.a commDependence.o
//    #ranlib libcommDependence.a
//
// Link your application with libcommData.a, or build it as a shared lib
// and LD_PRELOAD it to try out this tool.
//
// v1.1 :
// Use index to record all communication traces
//

#define UNW_LOCAL_ONLY // must define before including libunwind.h

#include <libunwind.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include "mpi_init.h"

using namespace std;

#define MODULE_INITED 1
#define LOG_SIZE 10000
#define MAX_STACK_DEPTH 100
#define MAX_CALL_PATH_LEN 1300  // MAX_STACK_DEPTH * 13
#define MAX_NAME_LEN 20
#define MAX_ARGS_LEN 20
#define MAX_COMM_WORLD_SIZE 100000
#define MAX_WAIT_REQ 100
#define MAX_TRACE_SIZE 25000000
#define TRACE_LOG_LINE_SIZE 100
#define MY_BT

// #define DEBUG

#ifdef MPICH2
#define SHIFT(COMM_ID) (((COMM_ID & 0xf0000000) >> 24) + (COMM_ID & 0x0000000f))
#else
#define SHIFT(COMM_ID) -1
#endif

typedef struct CollInfoStruct {
  char call_path[MAX_CALL_PATH_LEN] = {0};
  MPI_Comm comm;
  unsigned long long int count = 0;
  double exe_time = 0.0;
} CIS;

typedef struct P2PInfoStruct {
  char type = 0;  // 'r' 's' :blocking recv/send  | 'R' 'S': non-blocking recv/send
  char call_path[MAX_CALL_PATH_LEN] = {0};
  int request_count = 0;
  int source[MAX_WAIT_REQ] = {0};
  int dest[MAX_WAIT_REQ] = {0};
  int tag[MAX_WAIT_REQ] = {0};
  unsigned long long int count = 0;
  double exe_time = 0.0;
} PIS;

struct RequestConverter {
  char data[sizeof(MPI_Request)];
  RequestConverter(MPI_Request *mpi_request) { memcpy(data, mpi_request, sizeof(MPI_Request)); }
  RequestConverter() {}
  RequestConverter(const RequestConverter &req) { memcpy(data, req.data, sizeof(MPI_Request)); }
  RequestConverter &operator=(const RequestConverter &req) {
    memcpy(data, req.data, sizeof(MPI_Request));
    return *this;
  }
  bool operator<(const RequestConverter &request) const {
    for (size_t i = 0; i < sizeof(MPI_Request); i++) {
      if (data[i] != request.data[i]) {
        return data[i] < request.data[i];
      }
    }
    return false;
  }
};

static CIS coll_mpi_info_log[LOG_SIZE];
static PIS p2p_mpi_info_log[LOG_SIZE];
static unsigned long long int coll_mpi_info_log_pointer = 0;
static unsigned long long int p2p_mpi_info_log_pointer = 0;
static unsigned int trace_log[MAX_TRACE_SIZE] = {0};
static unsigned long long int trace_log_pointer = 0;

map<RequestConverter, pair<int, int>> request_converter;

// int mpi_rank = -1;
static int module_init = 0;
// static char *addr_threshold;
bool mpi_finalize_flag = false;

// int mpi_rank = 0;


int my_backtrace(unw_word_t* buffer, int max_depth) {
  unw_cursor_t cursor;
  unw_context_t context;

  // Initialize cursor to current frame for local unwinding.
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);

  // Unwind frames one by one, going up the frame stack.
  int depth = 0;
  while (unw_step(&cursor) > 0 && depth < max_depth) {
    unw_word_t pc;
    unw_get_reg(&cursor, UNW_REG_IP, &pc);
    if (pc == 0) {
      break;
    }
    buffer[depth] = pc;
    depth++;
  }
  return depth;
}

// Dump mpi info log
static void writeCollMpiInfoLog() {
  ofstream outputStream((string("MPID") + to_string(mpi_rank) + string(".TXT")), ios_base::app);
  if (!outputStream.good()) {
    cout << "Failed to open sample file\n";
    return;
  }

  int comm_size = 0;
  MPI_Group group1, group2;
  MPI_Comm_group(MPI_COMM_WORLD, &group1);
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  int ranks1[MAX_COMM_WORLD_SIZE] = {0};
  int ranks2[MAX_COMM_WORLD_SIZE] = {0};
  for (int i = 0; i < comm_size; i++) {
    ranks1[i] = i;
  }

  // TODO: output different comm
  for (int i = 0; i < coll_mpi_info_log_pointer; i++) {
    MPI_Comm_group(coll_mpi_info_log[i].comm, &group2);
    MPI_Group_translate_ranks(group1, comm_size, ranks1, group2, ranks2);
    outputStream << "c ";
    outputStream << coll_mpi_info_log[i].call_path << " | ";
    for (int j = 0; j < comm_size; j++) {
      outputStream << ranks2[j] << " ";
    }
    outputStream << " | " << coll_mpi_info_log[i].count;
    outputStream << " | " << coll_mpi_info_log[i].exe_time << '\n';
  }

  coll_mpi_info_log_pointer = 0;

  outputStream.close();
}

static void writeP2PMpiInfoLog() {
  ofstream outputStream((string("MPID") + to_string(mpi_rank) + string(".TXT")), ios_base::app);
  if (!outputStream.good()) {
    cout << "Failed to open sample file\n";
    return;
  }

  // TODO: output different comm
  for (int i = 0; i < p2p_mpi_info_log_pointer; i++) {
    outputStream << p2p_mpi_info_log[i].type << " ";
    outputStream << p2p_mpi_info_log[i].call_path << " | ";
    int request_count = p2p_mpi_info_log[i].request_count;
    for (int j = 0; j < request_count; j++) {
      outputStream << p2p_mpi_info_log[i].source[j] << " " << p2p_mpi_info_log[i].dest[j] << " "
                   << p2p_mpi_info_log[i].tag[j] << " , ";
    }
    outputStream << " | " << p2p_mpi_info_log[i].count;
    outputStream << " | " << p2p_mpi_info_log[i].exe_time << '\n';
  }

  p2p_mpi_info_log_pointer = 0;

  outputStream.close();
}

static void writeTraceLog() {
  ofstream outputStream((string("MPIT") + to_string(mpi_rank) + string(".TXT")), ios_base::app);
  if (!outputStream.good()) {
    cout << "Failed to open sample file\n";
    return;
  }

  for (int i = 0; i < trace_log_pointer; i += TRACE_LOG_LINE_SIZE) {
    for (int j = i; j < i + TRACE_LOG_LINE_SIZE && j < trace_log_pointer; j++) {
      outputStream << trace_log[j] << " ";
    }
    outputStream << '\n';
  }

  trace_log_pointer = 0;

  outputStream.close();
}

// static void init() __attribute__((constructor));
// static void init() {
//   if (module_init == MODULE_INITED) return;
//   module_init = MODULE_INITED;
//   addr_threshold = (char *)malloc(sizeof(char));
// }

// // Dump mpi info at the end of program's execution
// static void fini() __attribute__((destructor));
// static void fini() {
//   if (!mpi_finalize_flag) {
//     // writeCollMpiInfoLog();
//     // writeP2PMpiInfoLog();
//     // printf("mpi_finalize_flag is false\n");
//   }
// }

// Record mpi info to log

void TRACE_COLL(MPI_Comm comm, double exe_time) {

#ifdef MY_BT
  unw_word_t buffer[MAX_STACK_DEPTH] = {0};
#else
  void* buffer[MAX_STACK_DEPTH];
  memset(buffer, 0, sizeof(buffer));
#endif
  unsigned int i, depth = 0;
  // memset(buffer, 0, sizeof(buffer));
#ifdef MY_BT
  depth = my_backtrace(buffer, MAX_STACK_DEPTH);
#else
  depth = unw_backtrace(buffer, MAX_STACK_DEPTH);
#endif
  char call_path[MAX_CALL_PATH_LEN] = {0};
  int offset = 0;

  for (i = 0; i < depth; ++i) {
    if ((void *)buffer[i] != NULL && (char *)buffer[i] < addr_threshold) {
      offset += snprintf(call_path + offset, MAX_CALL_PATH_LEN - offset - 4, "%lx ", (void *)(buffer[i] - 2));
    }
  }

  bool hasRecordFlag = false;

  for (i = 0; i < coll_mpi_info_log_pointer; i++) {
    if (strcmp(call_path, coll_mpi_info_log[i].call_path) == 0) {
      int cmp_result;
      MPI_Comm_compare(comm, coll_mpi_info_log[i].comm, &cmp_result);
      if (cmp_result == MPI_CONGRUENT) {
        coll_mpi_info_log[i].count++;
        coll_mpi_info_log[i].exe_time += exe_time;
        trace_log[trace_log_pointer++] = i * 2;
        hasRecordFlag = true;
        break;
      }
    }
  }

  if (hasRecordFlag == false) {
    if (strlen(call_path) >= MAX_CALL_PATH_LEN) {
      // LOG_WARN("Call path string length (%d) is longer than %d", strlen(call_path),MAX_CALL_PATH_LEN);
    } else {
      strcpy(coll_mpi_info_log[coll_mpi_info_log_pointer].call_path, call_path);
      MPI_Comm_dup(comm, &coll_mpi_info_log[coll_mpi_info_log_pointer].comm);
      coll_mpi_info_log[coll_mpi_info_log_pointer].count = 1;
      coll_mpi_info_log[coll_mpi_info_log_pointer].exe_time = exe_time;
      trace_log[trace_log_pointer++] = coll_mpi_info_log_pointer * 2;
      coll_mpi_info_log_pointer++;
    }
  }

  if (coll_mpi_info_log_pointer >= LOG_SIZE - 5) {
    writeCollMpiInfoLog();
  }
  if (trace_log_pointer >= MAX_TRACE_SIZE - 5) {
    writeTraceLog();
  }
}

void TRACE_P2P(char type, int request_count, int *source, int *dest, int *tag, double exe_time) {

#ifdef MY_BT
  unw_word_t buffer[MAX_STACK_DEPTH] = {0};
#else
  void* buffer[MAX_STACK_DEPTH];
  memset(buffer, 0, sizeof(buffer));
#endif
  unsigned int i, j, depth = 0;
#ifdef MY_BT
  depth = my_backtrace(buffer, MAX_STACK_DEPTH);
#else
  depth = unw_backtrace(buffer, MAX_STACK_DEPTH);
#endif
  char call_path[MAX_CALL_PATH_LEN] = {0};
  int offset = 0;

  for (i = 0; i < depth; ++i) {
    if ((void *)buffer[i] != NULL && (char *)buffer[i] < addr_threshold) {
      offset += snprintf(call_path + offset, MAX_CALL_PATH_LEN - offset - 4, "%lx ", (void *)(buffer[i] - 2));
    }
  }

  bool hasRecordFlag = false;

  for (i = 0; i < p2p_mpi_info_log_pointer; i++) {
    if (p2p_mpi_info_log[i].type == type && strcmp(call_path, p2p_mpi_info_log[i].call_path) == 0) {
      if (p2p_mpi_info_log[i].request_count == request_count) {
        for (j = 0; j < request_count; j++) {
          if (p2p_mpi_info_log[i].source[j] != source[j] || p2p_mpi_info_log[i].dest[j] != dest[j] ||
              p2p_mpi_info_log[i].tag[j] != tag[j]) {
            goto k1;
          }
        }
        p2p_mpi_info_log[i].count++;
        p2p_mpi_info_log[i].exe_time += exe_time;
        hasRecordFlag = true;
        trace_log[trace_log_pointer++] = i * 2 + 1;
        break;
      k1:
        continue;
      }
    }
  }

  if (hasRecordFlag == false) {
    if (strlen(call_path) >= MAX_CALL_PATH_LEN) {
      // LOG_WARN("Call path string length (%d) is longer than %d", strlen(call_path),MAX_CALL_PATH_LEN);
    } else {
      p2p_mpi_info_log[p2p_mpi_info_log_pointer].type = type;
      strcpy(p2p_mpi_info_log[p2p_mpi_info_log_pointer].call_path, call_path);
      p2p_mpi_info_log[p2p_mpi_info_log_pointer].request_count = request_count;
      for (i = 0; i < request_count; i++) {
        p2p_mpi_info_log[p2p_mpi_info_log_pointer].source[i] = source[i];
        p2p_mpi_info_log[p2p_mpi_info_log_pointer].dest[i] = dest[i];
        p2p_mpi_info_log[p2p_mpi_info_log_pointer].tag[i] = tag[i];
      }
      p2p_mpi_info_log[p2p_mpi_info_log_pointer].count = 1;
      p2p_mpi_info_log[p2p_mpi_info_log_pointer].exe_time = exe_time;
      trace_log[trace_log_pointer++] = p2p_mpi_info_log_pointer * 2 + 1;
      p2p_mpi_info_log_pointer++;
    }
  }

  if (p2p_mpi_info_log_pointer >= LOG_SIZE - 5) {
    writeP2PMpiInfoLog();
  }
  if (trace_log_pointer >= MAX_TRACE_SIZE - 5) {
    writeTraceLog();
  }
}

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
        fprintf(stderr, "ERROR: Couldn't find fortran pmpi_init function.  Link against static library instead.\n");
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
_EXTERN_C_ int PMPI_Init_thread(int *argc, char ***argv, int required, int *provided);
_EXTERN_C_ int MPI_Init_thread(int *argc, char ***argv, int required, int *provided) {
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
static void MPI_Init_thread_fortran_wrapper(MPI_Fint *argc, MPI_Fint ***argv, MPI_Fint *required, MPI_Fint *provided,
                                            MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
  _wrap_py_return_val = MPI_Init_thread((int *)argc, (char ***)argv, *required, (int *)provided);
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_INIT_THREAD(MPI_Fint *argc, MPI_Fint ***argv, MPI_Fint *required, MPI_Fint *provided,
                                MPI_Fint *ierr) {
  MPI_Init_thread_fortran_wrapper(argc, argv, required, provided, ierr);
}

_EXTERN_C_ void mpi_init_thread(MPI_Fint *argc, MPI_Fint ***argv, MPI_Fint *required, MPI_Fint *provided,
                                MPI_Fint *ierr) {
  MPI_Init_thread_fortran_wrapper(argc, argv, required, provided, ierr);
}

_EXTERN_C_ void mpi_init_thread_(MPI_Fint *argc, MPI_Fint ***argv, MPI_Fint *required, MPI_Fint *provided,
                                 MPI_Fint *ierr) {
  MPI_Init_thread_fortran_wrapper(argc, argv, required, provided, ierr);
}

_EXTERN_C_ void mpi_init_thread__(MPI_Fint *argc, MPI_Fint ***argv, MPI_Fint *required, MPI_Fint *provided,
                                  MPI_Fint *ierr) {
  MPI_Init_thread_fortran_wrapper(argc, argv, required, provided, ierr);
}

/* ================= End Wrappers for MPI_Init_thread ================= */


// P2P communication
/* ================== C Wrappers for MPI_Send ================== */
_EXTERN_C_ int PMPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
_EXTERN_C_ int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call P2P communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Send(buf, count, datatype, dest, tag, comm);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

    int source_list[1];
    int dest_list[1];
    int tag_list[1];

    source_list[0] = mpi_rank;
    dest_list[0] = dest;
    tag_list[0] = tag;
#ifdef DEBUG
    printf("%s\n", "MPI_Send");
#endif
    TRACE_P2P('s', 1, source_list, dest_list, tag_list, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Send =============== */
static void MPI_Send_fortran_wrapper(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                                     MPI_Fint *comm, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Send((const void *)buf, *count, (MPI_Datatype)(*datatype), *dest, *tag, (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Send((const void *)buf, *count, MPI_Type_f2c(*datatype), *dest, *tag, MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_SEND(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                         MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Send_fortran_wrapper(buf, count, datatype, dest, tag, comm, ierr);
}

_EXTERN_C_ void mpi_send(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                         MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Send_fortran_wrapper(buf, count, datatype, dest, tag, comm, ierr);
}

_EXTERN_C_ void mpi_send_(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                          MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Send_fortran_wrapper(buf, count, datatype, dest, tag, comm, ierr);
}

_EXTERN_C_ void mpi_send__(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                           MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Send_fortran_wrapper(buf, count, datatype, dest, tag, comm, ierr);
}

/* ================= End Wrappers for MPI_Send ================= */

/* ================== C Wrappers for MPI_Isend ================== */
_EXTERN_C_ int PMPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm,
                          MPI_Request *request);
_EXTERN_C_ int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm,
                         MPI_Request *request) {
  int _wrap_py_return_val = 0;
  {
    // First call P2P communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Isend(buf, count, datatype, dest, tag, comm, request);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

    int source_list[1] = {-1};
    int dest_list[1] = {-1};
    int tag_list[1] = {-1};

    source_list[0] = mpi_rank;
    dest_list[0] = dest;
    tag_list[0] = tag;
    // if (mpi_rank == 0){
    // printf("isend record %x %d %d\n",request, mpi_rank , tag);}
    request_converter[RequestConverter(request)] = pair<int, int>(mpi_rank, tag);
#ifdef DEBUG
    printf("%s\n", "MPI_Isend");
#endif
    TRACE_P2P('S', 1, source_list, dest_list, tag_list, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Isend =============== */
static void MPI_Isend_fortran_wrapper(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                                      MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Isend((const void *)buf, *count, (MPI_Datatype)(*datatype), *dest, *tag, (MPI_Comm)(*comm),
                                  (MPI_Request *)request);
#else  /* MPI-2 safe call */
  MPI_Request temp_request;
  temp_request = MPI_Request_f2c(*request);
  _wrap_py_return_val =
      MPI_Isend((const void *)buf, *count, MPI_Type_f2c(*datatype), *dest, *tag, MPI_Comm_f2c(*comm), &temp_request);
  *request = MPI_Request_c2f(temp_request);
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_ISEND(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                          MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  MPI_Isend_fortran_wrapper(buf, count, datatype, dest, tag, comm, request, ierr);
}

_EXTERN_C_ void mpi_isend(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                          MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  MPI_Isend_fortran_wrapper(buf, count, datatype, dest, tag, comm, request, ierr);
}

_EXTERN_C_ void mpi_isend_(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                           MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  MPI_Isend_fortran_wrapper(buf, count, datatype, dest, tag, comm, request, ierr);
}

_EXTERN_C_ void mpi_isend__(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *dest, MPI_Fint *tag,
                            MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  MPI_Isend_fortran_wrapper(buf, count, datatype, dest, tag, comm, request, ierr);
}

/* ================= End Wrappers for MPI_Isend ================= */

/* ================== C Wrappers for MPI_Recv ================== */
_EXTERN_C_ int PMPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm,
                         MPI_Status *status);
_EXTERN_C_ int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm,
                        MPI_Status *status) {
  int _wrap_py_return_val = 0;
  {
    // First call P2P communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Recv(buf, count, datatype, source, tag, comm, status);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

    int source_list[1];
    int dest_list[1];
    int tag_list[1];

    source_list[0] = source;
    dest_list[0] = mpi_rank;
    tag_list[0] = tag;
#ifdef DEBUG
    printf("%s\n", "MPI_Recv");
#endif
    TRACE_P2P('r', 1, source_list, dest_list, tag_list, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Recv =============== */
static void MPI_Recv_fortran_wrapper(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source,
                                     MPI_Fint *tag, MPI_Fint *comm, MPI_Fint *status, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val =
      MPI_Recv((void *)buf, *count, (MPI_Datatype)(*datatype), *source, *tag, (MPI_Comm)(*comm), (MPI_Status *)status);
#else  /* MPI-2 safe call */
  MPI_Status temp_status;
  MPI_Status_f2c(status, &temp_status);
  _wrap_py_return_val =
      MPI_Recv((void *)buf, *count, MPI_Type_f2c(*datatype), *source, *tag, MPI_Comm_f2c(*comm), &temp_status);
  MPI_Status_c2f(&temp_status, status);
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_RECV(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source, MPI_Fint *tag,
                         MPI_Fint *comm, MPI_Fint *status, MPI_Fint *ierr) {
  MPI_Recv_fortran_wrapper(buf, count, datatype, source, tag, comm, status, ierr);
}

_EXTERN_C_ void mpi_recv(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source, MPI_Fint *tag,
                         MPI_Fint *comm, MPI_Fint *status, MPI_Fint *ierr) {
  MPI_Recv_fortran_wrapper(buf, count, datatype, source, tag, comm, status, ierr);
}

_EXTERN_C_ void mpi_recv_(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source, MPI_Fint *tag,
                          MPI_Fint *comm, MPI_Fint *status, MPI_Fint *ierr) {
  MPI_Recv_fortran_wrapper(buf, count, datatype, source, tag, comm, status, ierr);
}

_EXTERN_C_ void mpi_recv__(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source, MPI_Fint *tag,
                           MPI_Fint *comm, MPI_Fint *status, MPI_Fint *ierr) {
  MPI_Recv_fortran_wrapper(buf, count, datatype, source, tag, comm, status, ierr);
}

/* ================= End Wrappers for MPI_Recv ================= */

/* ================== C Wrappers for MPI_Irecv ================== */
_EXTERN_C_ int PMPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm,
                          MPI_Request *request);
_EXTERN_C_ int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm,
                         MPI_Request *request) {
  int _wrap_py_return_val = 0;
  {
    // First call P2P communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Irecv(buf, count, datatype, source, tag, comm, request);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

    int source_list[1] = {-1};
    int dest_list[1] = {-1};
    int tag_list[1] = {-1};

    source_list[0] = source;
    dest_list[0] = mpi_rank;
    tag_list[0] = tag;
#ifdef DEBUG
    if (mpi_rank == 0) {
      printf("irecv record %x %d %d\n", request, source, tag);
    }
#endif
    request_converter[RequestConverter(request)] = pair<int, int>(source, tag);
#ifdef DEBUG
    printf("%s\n", "MPI_Irecv");
#endif
    TRACE_P2P('R', 1, source_list, dest_list, tag_list, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Irecv =============== */
static void MPI_Irecv_fortran_wrapper(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source,
                                      MPI_Fint *tag, MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Irecv((void *)buf, *count, (MPI_Datatype)(*datatype), *source, *tag, (MPI_Comm)(*comm),
                                  (MPI_Request *)request);
#else  /* MPI-2 safe call */
  MPI_Request temp_request;
  temp_request = MPI_Request_f2c(*request);
  _wrap_py_return_val =
      MPI_Irecv((void *)buf, *count, MPI_Type_f2c(*datatype), *source, *tag, MPI_Comm_f2c(*comm), &temp_request);
  *request = MPI_Request_c2f(temp_request);
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_IRECV(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source, MPI_Fint *tag,
                          MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  MPI_Irecv_fortran_wrapper(buf, count, datatype, source, tag, comm, request, ierr);
}

_EXTERN_C_ void mpi_irecv(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source, MPI_Fint *tag,
                          MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  MPI_Irecv_fortran_wrapper(buf, count, datatype, source, tag, comm, request, ierr);
}

_EXTERN_C_ void mpi_irecv_(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source, MPI_Fint *tag,
                           MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  MPI_Irecv_fortran_wrapper(buf, count, datatype, source, tag, comm, request, ierr);
}

_EXTERN_C_ void mpi_irecv__(MPI_Fint *buf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *source, MPI_Fint *tag,
                            MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr) {
  MPI_Irecv_fortran_wrapper(buf, count, datatype, source, tag, comm, request, ierr);
}

/* ================= End Wrappers for MPI_Irecv ================= */

/* ================== C Wrappers for MPI_Wait ================== */
_EXTERN_C_ int PMPI_Wait(MPI_Request *request, MPI_Status *status);
_EXTERN_C_ int MPI_Wait(MPI_Request *request, MPI_Status *status) {
  int _wrap_py_return_val = 0;
  {
    int source_list[1] = {-1};
    int dest_list[1] = {-1};
    int tag_list[1] = {-1};

    dest_list[0] = mpi_rank;
    bool valid_flag = false;

    map<RequestConverter, pair<int, int>>::iterator iter;
    iter = request_converter.find(RequestConverter(request));
    if (iter != request_converter.end()) {
      pair<int, int> p = request_converter[RequestConverter(request)];
      // if (p.first >= 0 && p.second >= 0){
      source_list[0] = p.first;
      tag_list[0] = p.second;
      // printf("%d %d\n", p.first, p.second);
      valid_flag = true;
      //}
      request_converter.erase(RequestConverter(request));
    }

    auto st = chrono::system_clock::now();
    int ret_val = _wrap_py_return_val = PMPI_Wait(request, status);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

    if (valid_flag == false) {
      source_list[0] = status->MPI_SOURCE;
      tag_list[0] = status->MPI_TAG;
#ifdef DEBUG
// printf("%d %d\n", status->MPI_SOURCE, status->MPI_TAG);
#endif
    }

#ifdef DEBUG
    printf("%s\n", "MPI_Wait");
#endif
    TRACE_P2P('w', 1, source_list, dest_list, tag_list, time);

    return ret_val;
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Wait =============== */
static void MPI_Wait_fortran_wrapper(MPI_Fint *request, MPI_Fint *status, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Wait((MPI_Request *)request, (MPI_Status *)status);
#else  /* MPI-2 safe call */
  MPI_Status temp_status;
  MPI_Request temp_request;
  temp_request = MPI_Request_f2c(*request);
  MPI_Status_f2c(status, &temp_status);
  _wrap_py_return_val = MPI_Wait(&temp_request, &temp_status);
  *request = MPI_Request_c2f(temp_request);
  MPI_Status_c2f(&temp_status, status);
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_WAIT(MPI_Fint *request, MPI_Fint *status, MPI_Fint *ierr) {
  MPI_Wait_fortran_wrapper(request, status, ierr);
}

_EXTERN_C_ void mpi_wait(MPI_Fint *request, MPI_Fint *status, MPI_Fint *ierr) {
  MPI_Wait_fortran_wrapper(request, status, ierr);
}

_EXTERN_C_ void mpi_wait_(MPI_Fint *request, MPI_Fint *status, MPI_Fint *ierr) {
  MPI_Wait_fortran_wrapper(request, status, ierr);
}

_EXTERN_C_ void mpi_wait__(MPI_Fint *request, MPI_Fint *status, MPI_Fint *ierr) {
  MPI_Wait_fortran_wrapper(request, status, ierr);
}

/* ================= End Wrappers for MPI_Wait ================= */

/* ================== C Wrappers for MPI_Waitall ================== */
_EXTERN_C_ int PMPI_Waitall(int count, MPI_Request array_of_requests[], MPI_Status *array_of_statuses);
_EXTERN_C_ int MPI_Waitall(int count, MPI_Request array_of_requests[], MPI_Status *array_of_statuses) {
  int _wrap_py_return_val = 0;
  {
    int *source_list = (int *)malloc(count * sizeof(int));
    int *dest_list = (int *)malloc(count * sizeof(int));
    int *tag_list = (int *)malloc(count * sizeof(int));

    char *valid_flag = (char *)malloc(count * sizeof(char));
    memset(valid_flag, 0, count * sizeof(char));

    for (int i = 0; i < count; i++) {
      dest_list[i] = mpi_rank;

      map<RequestConverter, pair<int, int>>::iterator iter;
      iter = request_converter.find(RequestConverter(&array_of_requests[i]));
      if (iter != request_converter.end()) {
        pair<int, int> p = request_converter[RequestConverter(&array_of_requests[i])];

        // if (p.first >= 0 && p.second >= 0){
        source_list[i] = p.first;
        tag_list[i] = p.second;
#ifdef DEBUG
        if (mpi_rank == 0) {
          printf("convert wait %d %d\n", p.first, p.second);
        }
#endif
        valid_flag[i] = 1;

        //}
        request_converter.erase(RequestConverter(&array_of_requests[i]));
      }
    }

    auto st = chrono::system_clock::now();
    int ret_val = _wrap_py_return_val = PMPI_Waitall(count, array_of_requests, array_of_statuses);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

    for (int i = 0; i < count; i++) {
      if (valid_flag[i] == 0) {
        source_list[i] = array_of_statuses[i].MPI_SOURCE;
        tag_list[i] = array_of_statuses[i].MPI_TAG;
// printf("status wait %d %d\n", array_of_statuses[i].MPI_SOURCE, array_of_statuses[i].MPI_TAG);
#ifdef DEBUG
        if (mpi_rank == 0) {
          printf("status wait %d %d\n", array_of_statuses[i].MPI_SOURCE, array_of_statuses[i].MPI_TAG);
        }
#endif
      }
    }

    TRACE_P2P('w', count, source_list, dest_list, tag_list, time);

#ifdef DEBUG
    printf("%s\n", "MPI_Waitall");
#endif

    free(source_list);
    free(dest_list);
    free(tag_list);

    return ret_val;
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Waitall =============== */
static void MPI_Waitall_fortran_wrapper(MPI_Fint *count, MPI_Fint array_of_requests[], MPI_Fint *array_of_statuses,
                                        MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Waitall(*count, (MPI_Request *)array_of_requests, (MPI_Status *)array_of_statuses);
#else  /* MPI-2 safe call */
  int i;
  MPI_Request *temp_array_of_requests;
  MPI_Status *temp_array_of_statuses;
  temp_array_of_requests = (MPI_Request *)malloc(sizeof(MPI_Request) * *count);
  for (i = 0; i < *count; i++) temp_array_of_requests[i] = MPI_Request_f2c(array_of_requests[i]);
  temp_array_of_statuses = (MPI_Status *)malloc(sizeof(MPI_Status) * *count);
  for (i = 0; i < *count; i++) MPI_Status_f2c(&array_of_statuses[i], &temp_array_of_statuses[i]);
  _wrap_py_return_val = MPI_Waitall(*count, temp_array_of_requests, temp_array_of_statuses);
  for (i = 0; i < *count; i++) array_of_requests[i] = MPI_Request_c2f(temp_array_of_requests[i]);
  free(temp_array_of_requests);
  for (i = 0; i < *count; i++) MPI_Status_c2f(&temp_array_of_statuses[i], &array_of_statuses[i]);
  free(temp_array_of_statuses);
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_WAITALL(MPI_Fint *count, MPI_Fint array_of_requests[], MPI_Fint *array_of_statuses,
                            MPI_Fint *ierr) {
  MPI_Waitall_fortran_wrapper(count, array_of_requests, array_of_statuses, ierr);
}

_EXTERN_C_ void mpi_waitall(MPI_Fint *count, MPI_Fint array_of_requests[], MPI_Fint *array_of_statuses,
                            MPI_Fint *ierr) {
  MPI_Waitall_fortran_wrapper(count, array_of_requests, array_of_statuses, ierr);
}

_EXTERN_C_ void mpi_waitall_(MPI_Fint *count, MPI_Fint array_of_requests[], MPI_Fint *array_of_statuses,
                             MPI_Fint *ierr) {
  MPI_Waitall_fortran_wrapper(count, array_of_requests, array_of_statuses, ierr);
}

_EXTERN_C_ void mpi_waitall__(MPI_Fint *count, MPI_Fint array_of_requests[], MPI_Fint *array_of_statuses,
                              MPI_Fint *ierr) {
  MPI_Waitall_fortran_wrapper(count, array_of_requests, array_of_statuses, ierr);
}

/* ================= End Wrappers for MPI_Waitall ================= */

// collective communication
/* ================== C Wrappers for MPI_Reduce ================== */
_EXTERN_C_ int PMPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root,
                           MPI_Comm comm);
_EXTERN_C_ int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root,
                          MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call collective communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

#ifdef DEBUG
    printf("%s\n", "MPI_Reduce");
#endif
    TRACE_COLL(comm, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Reduce =============== */
static void MPI_Reduce_fortran_wrapper(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype,
                                       MPI_Fint *op, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Reduce((const void *)sendbuf, (void *)recvbuf, *count, (MPI_Datatype)(*datatype),
                                   (MPI_Op)(*op), *root, (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Reduce((const void *)sendbuf, (void *)recvbuf, *count, MPI_Type_f2c(*datatype),
                                   MPI_Op_f2c(*op), *root, MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_REDUCE(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                           MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Reduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, root, comm, ierr);
}

_EXTERN_C_ void mpi_reduce(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                           MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Reduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, root, comm, ierr);
}

_EXTERN_C_ void mpi_reduce_(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                            MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Reduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, root, comm, ierr);
}

_EXTERN_C_ void mpi_reduce__(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                             MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Reduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, root, comm, ierr);
}

/* ================= End Wrappers for MPI_Reduce ================= */

/* ================== C Wrappers for MPI_Alltoall ================== */
_EXTERN_C_ int PMPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount,
                             MPI_Datatype recvtype, MPI_Comm comm);
_EXTERN_C_ int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount,
                            MPI_Datatype recvtype, MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call collective communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

#ifdef DEBUG
    printf("%s\n", "MPI_Alltoall");
#endif
    TRACE_COLL(comm, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Alltoall =============== */
static void MPI_Alltoall_fortran_wrapper(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                                         MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Alltoall((const void *)sendbuf, *sendcount, (MPI_Datatype)(*sendtype), (void *)recvbuf,
                                     *recvcount, (MPI_Datatype)(*recvtype), (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Alltoall((const void *)sendbuf, *sendcount, MPI_Type_f2c(*sendtype), (void *)recvbuf,
                                     *recvcount, MPI_Type_f2c(*recvtype), MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_ALLTOALL(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                             MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Alltoall_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr);
}

_EXTERN_C_ void mpi_alltoall(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                             MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Alltoall_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr);
}

_EXTERN_C_ void mpi_alltoall_(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                              MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Alltoall_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr);
}

_EXTERN_C_ void mpi_alltoall__(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                               MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Alltoall_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr);
}

/* ================= End Wrappers for MPI_Alltoall ================= */

/* ================== C Wrappers for MPI_Allreduce ================== */
_EXTERN_C_ int PMPI_Allreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op,
                              MPI_Comm comm);
_EXTERN_C_ int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op,
                             MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call collective communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

#ifdef DEBUG
    printf("%s\n", "MPI_Allreduce");
#endif
    TRACE_COLL(comm, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Allreduce =============== */
static void MPI_Allreduce_fortran_wrapper(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype,
                                          MPI_Fint *op, MPI_Fint *comm, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Allreduce((const void *)sendbuf, (void *)recvbuf, *count, (MPI_Datatype)(*datatype),
                                      (MPI_Op)(*op), (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Allreduce((const void *)sendbuf, (void *)recvbuf, *count, MPI_Type_f2c(*datatype),
                                      MPI_Op_f2c(*op), MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_ALLREDUCE(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                              MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Allreduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, comm, ierr);
}

_EXTERN_C_ void mpi_allreduce(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                              MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Allreduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, comm, ierr);
}

_EXTERN_C_ void mpi_allreduce_(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                               MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Allreduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, comm, ierr);
}

_EXTERN_C_ void mpi_allreduce__(MPI_Fint *sendbuf, MPI_Fint *recvbuf, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *op,
                                MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Allreduce_fortran_wrapper(sendbuf, recvbuf, count, datatype, op, comm, ierr);
}

/* ================= End Wrappers for MPI_Allreduce ================= */

/* ================== C Wrappers for MPI_Bcast ================== */
_EXTERN_C_ int PMPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm);
_EXTERN_C_ int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call collective communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Bcast(buffer, count, datatype, root, comm);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

#ifdef DEBUG
    printf("%s\n", "MPI_Bcast");
#endif
    TRACE_COLL(comm, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Bcast =============== */
static void MPI_Bcast_fortran_wrapper(MPI_Fint *buffer, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *root,
                                      MPI_Fint *comm, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Bcast((void *)buffer, *count, (MPI_Datatype)(*datatype), *root, (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Bcast((void *)buffer, *count, MPI_Type_f2c(*datatype), *root, MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_BCAST(MPI_Fint *buffer, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *root, MPI_Fint *comm,
                          MPI_Fint *ierr) {
  MPI_Bcast_fortran_wrapper(buffer, count, datatype, root, comm, ierr);
}

_EXTERN_C_ void mpi_bcast(MPI_Fint *buffer, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *root, MPI_Fint *comm,
                          MPI_Fint *ierr) {
  MPI_Bcast_fortran_wrapper(buffer, count, datatype, root, comm, ierr);
}

_EXTERN_C_ void mpi_bcast_(MPI_Fint *buffer, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *root, MPI_Fint *comm,
                           MPI_Fint *ierr) {
  MPI_Bcast_fortran_wrapper(buffer, count, datatype, root, comm, ierr);
}

_EXTERN_C_ void mpi_bcast__(MPI_Fint *buffer, MPI_Fint *count, MPI_Fint *datatype, MPI_Fint *root, MPI_Fint *comm,
                            MPI_Fint *ierr) {
  MPI_Bcast_fortran_wrapper(buffer, count, datatype, root, comm, ierr);
}

/* ================= End Wrappers for MPI_Bcast ================= */

/* ================== C Wrappers for MPI_Scatter ================== */
_EXTERN_C_ int PMPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount,
                            MPI_Datatype recvtype, int root, MPI_Comm comm);
_EXTERN_C_ int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount,
                           MPI_Datatype recvtype, int root, MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call collective communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Scatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

#ifdef DEBUG
    printf("%s\n", "MPI_Scatter");
#endif
    TRACE_COLL(comm, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Scatter =============== */
static void MPI_Scatter_fortran_wrapper(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                                        MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm,
                                        MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Scatter((const void *)sendbuf, *sendcount, (MPI_Datatype)(*sendtype), (void *)recvbuf,
                                    *recvcount, (MPI_Datatype)(*recvtype), *root, (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Scatter((const void *)sendbuf, *sendcount, MPI_Type_f2c(*sendtype), (void *)recvbuf,
                                    *recvcount, MPI_Type_f2c(*recvtype), *root, MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_SCATTER(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                            MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Scatter_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, ierr);
}

_EXTERN_C_ void mpi_scatter(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                            MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Scatter_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, ierr);
}

_EXTERN_C_ void mpi_scatter_(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                             MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Scatter_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, ierr);
}

_EXTERN_C_ void mpi_scatter__(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                              MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Scatter_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, ierr);
}

/* ================= End Wrappers for MPI_Scatter ================= */

/* ================== C Wrappers for MPI_Gather ================== */
_EXTERN_C_ int PMPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount,
                           MPI_Datatype recvtype, int root, MPI_Comm comm);
_EXTERN_C_ int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount,
                          MPI_Datatype recvtype, int root, MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call collective communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Gather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

#ifdef DEBUG
    printf("%s\n", "MPI_Gather");
#endif
    TRACE_COLL(comm, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Gather =============== */
static void MPI_Gather_fortran_wrapper(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                                       MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm,
                                       MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Gather((const void *)sendbuf, *sendcount, (MPI_Datatype)(*sendtype), (void *)recvbuf,
                                   *recvcount, (MPI_Datatype)(*recvtype), *root, (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Gather((const void *)sendbuf, *sendcount, MPI_Type_f2c(*sendtype), (void *)recvbuf,
                                   *recvcount, MPI_Type_f2c(*recvtype), *root, MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_GATHER(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                           MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Gather_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, ierr);
}

_EXTERN_C_ void mpi_gather(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                           MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Gather_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, ierr);
}

_EXTERN_C_ void mpi_gather_(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                            MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Gather_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, ierr);
}

_EXTERN_C_ void mpi_gather__(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                             MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Gather_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, ierr);
}

/* ================= End Wrappers for MPI_Gather ================= */

/* ================== C Wrappers for MPI_Allgather ================== */
_EXTERN_C_ int PMPI_Allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount,
                              MPI_Datatype recvtype, MPI_Comm comm);
_EXTERN_C_ int MPI_Allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount,
                             MPI_Datatype recvtype, MPI_Comm comm) {
  int _wrap_py_return_val = 0;
  {
    // First call collective communication
    auto st = chrono::system_clock::now();
    _wrap_py_return_val = PMPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
    auto ed = chrono::system_clock::now();
    double time = chrono::duration_cast<chrono::microseconds>(ed - st).count();

#ifdef DEBUG
    printf("%s\n", "MPI_Allgather");
#endif
    TRACE_COLL(comm, time);
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Allgather =============== */
static void MPI_Allgather_fortran_wrapper(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                                          MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
#if (!defined(MPICH_HAS_C2F) && defined(MPICH_NAME) && (MPICH_NAME == 1)) /* MPICH test */
  _wrap_py_return_val = MPI_Allgather((const void *)sendbuf, *sendcount, (MPI_Datatype)(*sendtype), (void *)recvbuf,
                                      *recvcount, (MPI_Datatype)(*recvtype), (MPI_Comm)(*comm));
#else  /* MPI-2 safe call */
  _wrap_py_return_val = MPI_Allgather((const void *)sendbuf, *sendcount, MPI_Type_f2c(*sendtype), (void *)recvbuf,
                                      *recvcount, MPI_Type_f2c(*recvtype), MPI_Comm_f2c(*comm));
#endif /* MPICH test */
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_ALLGATHER(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                              MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Allgather_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr);
}

_EXTERN_C_ void mpi_allgather(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                              MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Allgather_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr);
}

_EXTERN_C_ void mpi_allgather_(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                               MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Allgather_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr);
}

_EXTERN_C_ void mpi_allgather__(MPI_Fint *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, MPI_Fint *recvbuf,
                                MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr) {
  MPI_Allgather_fortran_wrapper(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr);
}

/* ================= End Wrappers for MPI_Allgather ================= */

/* ================== C Wrappers for MPI_Finalize ================== */
_EXTERN_C_ int PMPI_Finalize();
_EXTERN_C_ int MPI_Finalize() {
  int _wrap_py_return_val = 0;
  {
    mpi_finalize_flag = true;
    writeCollMpiInfoLog();
    writeP2PMpiInfoLog();
    writeTraceLog();
#ifdef DEBUG
    printf("%s\n", "MPI_Finalize");
#endif
    // Finally call MPI_Finalize
    _wrap_py_return_val = PMPI_Finalize();
  }
  return _wrap_py_return_val;
}

/* =============== Fortran Wrappers for MPI_Finalize =============== */
static void MPI_Finalize_fortran_wrapper(MPI_Fint *ierr) {
  int _wrap_py_return_val = 0;
  _wrap_py_return_val = MPI_Finalize();
  *ierr = _wrap_py_return_val;
}

_EXTERN_C_ void MPI_FINALIZE(MPI_Fint *ierr) { MPI_Finalize_fortran_wrapper(ierr); }

_EXTERN_C_ void mpi_finalize(MPI_Fint *ierr) { MPI_Finalize_fortran_wrapper(ierr); }

_EXTERN_C_ void mpi_finalize_(MPI_Fint *ierr) { MPI_Finalize_fortran_wrapper(ierr); }

_EXTERN_C_ void mpi_finalize__(MPI_Fint *ierr) { MPI_Finalize_fortran_wrapper(ierr); }

/* ================= End Wrappers for MPI_Finalize ================= */
