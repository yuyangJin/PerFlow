#ifndef SAMPLER_H_
#define SAMPLER_H_

//#define _GNU_SOURCE
#define UNW_LOCAL_ONLY

#include "baguatool.h"
#include "common/tprintf.h"
#include <assert.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <libunwind.h>
#include <malloc.h>
#include <papi.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define TRY(func, flag)                                                        \
  {                                                                            \
    int retval = func;                                                         \
    if (retval != flag)                                                        \
      LOG_ERROR("%s, ErrCode: %s\n", #func, PAPI_strerror(retval));            \
  }

#define RESOLVE_SYMBOL_VERSIONED 1
#define RESOLVE_SYMBOL_UNVERSIONED 2
#define PTHREAD_VERSION "GLIBC_2.3.2"

#define DEFAULT_CYC_SAMPLE_COUNT (1000)     // 10ms
#define DEFAULT_INS_SAMPLE_COUNT (20000000) // 10ms
#define DEFAULT_CM_SAMPLE_COUNT (100000)    // 10ms

#ifndef MAX_STACK_DEPTH
#define MAX_STACK_DEPTH 100
#endif

#ifndef NUM_EVENTS
#define NUM_EVENTS 1
#endif

#ifndef __cplusplus

#define bool _Bool
#define true 1
#define false 0

#endif

// struct for pthread instrumrntation pthread_create

namespace baguatool::collector {

// typedef unsigned long long int addr_t;

struct LongLongVec {
  long_long overflow_vector;
};

class SamplerImpl {
private:
  int mpiRank = -1;

  // thread_local static int EventSet;
  // void (*func_at_overflow)(int);
  int cyc_sample_count;

public:
  SamplerImpl() {
    // EventSet = PAPI_NULL;
    // func_at_overflow = nullptr;
    cyc_sample_count = 0;
  };
  ~SamplerImpl(){};

  void SetSamplingFreq(int freq);
  void Setup();
  void AddThread();
  void RemoveThread();
  void UnsetOverflow();
  void SetOverflow(void (*FUNC_AT_OVERFLOW)(int));
  void Start();
  void Stop();
  int GetOverflowEvent(LongLongVec *overflow_vector);
  int GetBacktrace(type::addr_t *call_path, int max_call_path_depth);
  int GetBacktrace(type::addr_t *call_path, int max_call_path_depth,
                   int start_depth);
};

int my_backtrace(unw_word_t *buffer, int max_depth);

// static void* resolve_symbol(const char* symbol_name, int config);

static void papi_handler(int EventSet, void *address, long_long overflow_vector,
                         void *context);

} // namespace baguatool::collector
#endif // SAMPLER_H_