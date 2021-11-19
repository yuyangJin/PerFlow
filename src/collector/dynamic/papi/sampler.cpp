#include "sampler.h"
#include "baguatool.h"

namespace baguatool::collector {

Sampler::Sampler() { this->sa = std::make_unique<SamplerImpl>(); }

Sampler::~Sampler() {}

void Sampler::SetSamplingFreq(int freq) { sa->SetSamplingFreq(freq); }
void Sampler::Setup() { sa->Setup(); }
void Sampler::AddThread() { sa->AddThread(); }
void Sampler::RemoveThread() { sa->RemoveThread(); }
void Sampler::UnsetOverflow() { sa->UnsetOverflow(); }
void Sampler::SetOverflow(void (*FUNC_AT_OVERFLOW)(int)) { sa->SetOverflow(FUNC_AT_OVERFLOW); }
void Sampler::Start() { sa->Start(); }
void Sampler::Stop() { sa->Stop(); }
int Sampler::GetOverflowEvent(LongLongVec* overflow_vector) { return sa->GetOverflowEvent(overflow_vector); }
int Sampler::GetBacktrace(type::addr_t* call_path, int max_call_path_depth) {
  return sa->GetBacktrace(call_path, max_call_path_depth);
}
int Sampler::GetBacktrace(type::addr_t* call_path, int max_call_path_depth, int start_depth) {
  return sa->GetBacktrace(call_path, max_call_path_depth, start_depth);
}

static __thread void (*func_at_overflow_1)(int) = nullptr;
static __thread int EventSet;

void SamplerImpl::SetSamplingFreq(int freq) {
  // PAPI setup for main thread
  // char* str = getenv("CYC_SAMPLE_COUNT");
  // CYC_SAMPLE_COUNT = (str ? atoi(str) : DEFAULT_CYC_SAMPLE_COUNT);
  if (freq > 0) {
    this->cyc_sample_count = (3.1 * 1e9) / freq;
  } else {
    this->cyc_sample_count = (3.1 * 1e9) / DEFAULT_CYC_SAMPLE_COUNT;
  }
  // this->cyc_sample_count = (3.1 * 1e9) / (freq ? freq : DEFAULT_CYC_SAMPLE_COUNT);
  LOG_INFO("SET sampling interval to %d cycles\n", this->cyc_sample_count);
}

void SamplerImpl::Setup() {
  TRY(PAPI_library_init(PAPI_VER_CURRENT), PAPI_VER_CURRENT);
  TRY(PAPI_thread_init(pthread_self), PAPI_OK);
}

void SamplerImpl::SetOverflow(void (*FUNC_AT_OVERFLOW)(int)) {
  int Events[NUM_EVENTS];

  EventSet = PAPI_NULL;
  TRY(PAPI_create_eventset(&(EventSet)), PAPI_OK);

  Events[0] = PAPI_TOT_CYC;
  // Events[1] = PAPI_L2_DCM;
  // Events[1] = PAPI_L1_DCM;
  // Events[1] = PAPI_TOT_INS;
  // Events[1] = PAPI_LD_INS;
  // Events[2] = PAPI_SR_INS;
  // Events[3] = PAPI_L1_DCM;
  // Events[3] = PAPI_L3_DCA;

  TRY(PAPI_add_events(EventSet, (int*)Events, NUM_EVENTS), PAPI_OK);

  // PAPI_overflow_handler_t *_papi_overflow_handler = (PAPI_overflow_handler_t*) &(this->papi_handler) (int EventSet,
  // void *address, long_long overflow_vector, void *context );
  // void (*)(int, void*, long long int, void*)
  // PAPI_overflow_handler_t _papi_overflow_handler = void (*)(int, void*, long long int, void*) &(papi_handler);
  PAPI_overflow_handler_t _papi_overflow_handler = (PAPI_overflow_handler_t) & (papi_handler);
  TRY(PAPI_overflow(EventSet, PAPI_TOT_CYC, this->cyc_sample_count, 0, _papi_overflow_handler), PAPI_OK);
  // TRY(PAPI_overflow(EventSet, PAPI_LD_INS, INS_SAMPLE_COUNT, 0, papi_handler), PAPI_OK);
  // TRY(PAPI_overflow(EventSet, PAPI_SR_INS, INS_SAMPLE_COUNT, 0, papi_handler), PAPI_OK);
  // TRY(PAPI_overflow(EventSet, PAPI_L1_DCM, CM_SAMPLE_COUNT, 0, papi_handler), PAPI_OK);
  // TRY(PAPI_overflow(EventSet, PAPI_L3_DCA, CM_SAMPLE_COUNT, 0, papi_handler), PAPI_OK);

  // this->func_at_overflow = FUNC_AT_OVERFLOW;
  func_at_overflow_1 = FUNC_AT_OVERFLOW;
}

void SamplerImpl::AddThread() { TRY(PAPI_register_thread(), PAPI_OK); }

void SamplerImpl::RemoveThread() { TRY(PAPI_unregister_thread(), PAPI_OK); }

void SamplerImpl::UnsetOverflow() {
  int Events[NUM_EVENTS];

  Events[0] = PAPI_TOT_CYC;
  // Events[1] = PAPI_L2_DCM;
  // Events[1] = PAPI_L1_DCM;
  // Events[1] = PAPI_TOT_INS;
  // Events[1] = PAPI_LD_INS;
  // Events[2] = PAPI_SR_INS;
  // Events[3] = PAPI_L1_DCM;
  // Events[3] = PAPI_L3_DCA;

  TRY(PAPI_overflow(EventSet, PAPI_TOT_CYC, 0, 0, papi_handler), PAPI_OK);
  TRY(PAPI_remove_events(EventSet, (int*)Events, NUM_EVENTS), PAPI_OK);
  TRY(PAPI_destroy_eventset(&(EventSet)), PAPI_OK);
}

void SamplerImpl::Start() { TRY(PAPI_start(EventSet), PAPI_OK); }

void SamplerImpl::Stop() { TRY(PAPI_stop(EventSet, NULL), PAPI_OK); }

int SamplerImpl::GetOverflowEvent(LongLongVec* overflow_vector) {
  int Events[NUM_EVENTS], number, x, y;
  number = NUM_EVENTS;

  TRY(PAPI_get_overflow_event_index(EventSet, overflow_vector->overflow_vector, Events, &number), PAPI_OK);

  for (x = 0; x < number; x++) {
    for (y = 0; y < NUM_EVENTS; y++) {
      if (Events[x] == y) {
        break;
      }
    }
  }
  return y;
}

#define MY_BT

int SamplerImpl::GetBacktrace(type::addr_t* call_path, int max_call_path_depth) {
#ifdef MY_BT
  unw_word_t buffer[MAX_STACK_DEPTH] = {0};
#else
  void* buffer[MAX_STACK_DEPTH];
  memset(buffer, 0, sizeof(buffer));
#endif
  unsigned int i, depth = 0;

#ifdef MY_BT
  depth = my_backtrace(buffer, max_call_path_depth);
#else
  depth = unw_backtrace(buffer, max_call_path_depth);
#endif
  unsigned int addr_log_pointer = 0;

#ifdef MY_BT
  for (i = 1; i < depth; ++i) {
    if (buffer[i] != 0) {
      call_path[addr_log_pointer] = (type::addr_t)(buffer[i]) - 2;
      addr_log_pointer++;
    }
  }
#else
  for (i = 1; i < depth; ++i) {
    if ((void*)buffer[i] != NULL) {
      call_path[addr_log_pointer] = (type::addr_t)(buffer[i]) - 2;
      addr_log_pointer++;
    }
  }
#endif

  return addr_log_pointer;
}

int SamplerImpl::GetBacktrace(type::addr_t* call_path, int max_call_path_depth, int start_depth) {
#ifdef MY_BT
  unw_word_t buffer[MAX_STACK_DEPTH] = {0};
#else
  void* buffer[MAX_STACK_DEPTH];
  memset(buffer, 0, sizeof(buffer));
#endif
  unsigned int i, depth = 0;

#ifdef MY_BT
  depth = my_backtrace(buffer, max_call_path_depth);
#else
  depth = unw_backtrace(buffer, max_call_path_depth);
#endif
  unsigned int addr_log_pointer = 0;

#ifdef MY_BT
  for (i = start_depth; i < depth; ++i) {
    if (buffer[i] != 0) {
      call_path[addr_log_pointer] = (type::addr_t)(buffer[i]) - 2;
      addr_log_pointer++;
    }
  }

#else
  for (i = start_depth; i < depth; ++i) {
    if ((void*)buffer[i] != NULL) {
      call_path[addr_log_pointer] = (type::addr_t)(buffer[i]) - 2;
      addr_log_pointer++;
    }
  }

#endif

  return addr_log_pointer;
}

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

void papi_handler(int EventSet, void* address, long_long overflow_vector, void* context) {
  // this->Stop();
  TRY(PAPI_stop(EventSet, NULL), PAPI_OK);

  // int y = this->GetOverflowEvent(overflow_vector);

  int Events[NUM_EVENTS], number, x, y;
  number = NUM_EVENTS;

  TRY(PAPI_get_overflow_event_index(EventSet, overflow_vector, Events, &number), PAPI_OK);

  for (x = 0; x < number; x++) {
    for (y = 0; y < NUM_EVENTS; y++) {
      if (Events[x] == y) {
        break;
      }
    }
  }
  // return y;

  // printf("interrupt\n");
  (*(func_at_overflow_1))(y);

  TRY(PAPI_start(EventSet), PAPI_OK);
  // this->Start();
}
}
