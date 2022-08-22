#include "baguatool.h"
#include "dbg.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unordered_map>

#define MODULE_INITED 1
#define RESOLVE_SYMBOL_VERSIONED 1
#define RESOLVE_SYMBOL_UNVERSIONED 2
#define PTHREAD_VERSION "GLIBC_2.3.2"

#define NUM_EVENTS 1

#define MAX_CALL_PATH_DEPTH 100

#define gettid() syscall(__NR_gettid)

static int (*original_pthread_create)(pthread_t *thread,
                                      const pthread_attr_t *attr,
                                      void *(*start_routine)(void *),
                                      void *arg) = NULL;
static int (*original_pthread_join)(pthread_t thread, void **value_ptr) = NULL;

static int (*original_pthread_mutex_init)(
    pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) = NULL;
static int (*original_pthread_mutex_lock)(pthread_mutex_t *mutex) = NULL;
static int (*original_pthread_mutex_unlock)(pthread_mutex_t *mutex) = NULL;

std::unique_ptr<baguatool::collector::Sampler> sampler = nullptr;
std::unique_ptr<baguatool::core::PerfData> perf_data = nullptr;

// struct pairhash {
//  public:
//   template <typename T, typename U>
//   std::size_t operator()(const std::pair<T, U> &x) const {
//     return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
//   }
// };

struct call_path_t {
  baguatool::type::addr_t call_path[MAX_CALL_PATH_DEPTH] = {0};
  int call_path_len = 0;
};

std::unordered_map<u_int64_t, std::pair<u_int64_t, call_path_t *>>
    *mutex_to_tid_and_callpath;

std::unordered_map<long, int> *tid_to_thread_gid;

std::unordered_map<pthread_t, int> pthread_t_to_create_thread_id;

static int CYC_SAMPLE_COUNT = 100; // 10ms
static int module_init = 0;

static __thread int thread_gid = -1;
static int main_thread_gid = -1;
static int thread_global_id;

int new_thread_gid() {
  thread_gid = __sync_fetch_and_add(&thread_global_id, 1);
  // LOG_INFO("GET thread_gid = %d\n", thread_gid);
  return thread_gid;
}
void close_thread_gid() {
  thread_gid = __sync_sub_and_fetch(&thread_global_id, 1);
  // LOG_INFO("GET thread_gid = %d\n", thread_gid);
  return;
}

void print_thread_id(pthread_t id) {
  size_t i;
  for (i = sizeof(i); i; --i)
    printf("%02x", *(((unsigned char *)&id) + i - 1));
}

void print_call_path(baguatool::type::addr_t *call_path, int cal_path_len) {
  for (int i = 0; i < cal_path_len; i++) {
    printf("%lx - ", call_path[i]);
  }
  printf("\n");
}

void RecordCallPath(int y) {
  baguatool::type::addr_t call_path[MAX_CALL_PATH_DEPTH] = {0};
  int call_path_len = sampler->GetBacktrace(call_path, MAX_CALL_PATH_DEPTH);
  perf_data->RecordVertexData(call_path, call_path_len, 0, thread_gid, 1);
}

static void *resolve_symbol(const char *symbol_name, int config) {
  void *result;
  if (config == RESOLVE_SYMBOL_VERSIONED) {
    result = dlvsym(RTLD_NEXT, symbol_name, PTHREAD_VERSION);
    if (result == NULL) {
      LOG_ERROR("Unable to resolve symbol %s@%s\n", symbol_name,
                PTHREAD_VERSION);
      // exit(1);
    }
  } else if (config == RESOLVE_SYMBOL_UNVERSIONED) {
    result = dlsym(RTLD_NEXT, symbol_name);
    if (result == NULL) {
      LOG_ERROR("Unable to resolve symbol %s\n", symbol_name);
      // exit(1);
    }
  }
  return result;
}

static void init_mock() __attribute__((constructor));
static void fini_mock() __attribute__((destructor));

// User-defined what to do at constructor
static void init_mock() {
  if (module_init == MODULE_INITED)
    return;
  module_init = MODULE_INITED;

  sampler = std::make_unique<baguatool::collector::Sampler>();
  perf_data = std::make_unique<baguatool::core::PerfData>();

  original_pthread_create = (decltype(original_pthread_create))resolve_symbol(
      "pthread_create", RESOLVE_SYMBOL_UNVERSIONED);
  original_pthread_join = (decltype(original_pthread_join))resolve_symbol(
      "pthread_join", RESOLVE_SYMBOL_UNVERSIONED);
  original_pthread_mutex_init =
      (decltype(original_pthread_mutex_init))resolve_symbol(
          "pthread_mutex_init", RESOLVE_SYMBOL_UNVERSIONED);
  original_pthread_mutex_lock =
      (decltype(original_pthread_mutex_lock))resolve_symbol(
          "pthread_mutex_lock", RESOLVE_SYMBOL_UNVERSIONED);
  original_pthread_mutex_unlock =
      (decltype(original_pthread_mutex_unlock))resolve_symbol(
          "pthread_mutex_unlock", RESOLVE_SYMBOL_UNVERSIONED);

  thread_global_id = 0;
  main_thread_gid = new_thread_gid();

  tid_to_thread_gid = new std::unordered_map<long, int>();
  mutex_to_tid_and_callpath =
      new std::unordered_map<u_int64_t, std::pair<u_int64_t, call_path_t *>>();
  (*tid_to_thread_gid)[gettid()] = thread_gid;

  // sampler setup for main thread
  sampler->SetSamplingFreq(CYC_SAMPLE_COUNT);
  sampler->Setup();

  void (*RecordCallPathPointer)(int) = &(RecordCallPath);
  sampler->SetOverflow(RecordCallPathPointer);

  sampler->Start();
}

// User-defined what to do at destructor
static void fini_mock() {
  sampler->Stop();

  // sampler->RecordLdLib();

  perf_data->Dump("SAMPLE.TXT");

  // check memory leak
  // std::unordered_map<long, int>().swap(*tid_to_thread_gid);
  // delete (tid_to_thread_gid);
}

/** struct for pthread instrumentation start_routine */
struct start_routine_wrapper_arg {
  void *(*start_routine)(void *);
  void *real_arg;
  int create_thread_id;
  // baguatool::type::addr_t call_path[MAX_CALL_PATH_DEPTH] = {0};
  // int call_path_len;
  // pthread_t thread;
};

static void *start_routine_wrapper(void *arg) {
  auto args_ = (start_routine_wrapper_arg *)arg;
  void *(*start_routine)(void *) = args_->start_routine;
  void *real_arg = args_->real_arg;

  int create_thread_id = new_thread_gid();
  LOG_INFO("Thread Start, thread_gid = %d\n", create_thread_id);

  // dbg(thread_gid);
  // perf_data->RecordEdgeData(args_->call_path, args_->call_path_len,
  // (baguatool::type::addr_t*) nullptr, 0, 0, 0, main_thread_gid,
  // create_thread_id, -1); dbg(args_->thread, create_thread_id);
  // pthread_t_to_create_thread_id[args_->thread] = create_thread_id;
  args_->create_thread_id = create_thread_id;
  sampler->AddThread();
  sampler->SetOverflow(&RecordCallPath);
  sampler->Start();

  // dbg(start_routine);

  void *ret = (start_routine)(real_arg); // acutally launch new thread

  sampler->Stop();
  sampler->UnsetOverflow();
  sampler->RemoveThread();
  // close_thread_gid();
  LOG_INFO("Thread Finish, thread_gid = %d\n", thread_gid);

  return ret;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *real_arg) {
  /** If module are not initialized, init it at first. */
  if (module_init != MODULE_INITED) {
    init_mock();
  }

  /** ------------------------- */
  sampler->Stop();
  struct start_routine_wrapper_arg *arg =
      (struct start_routine_wrapper_arg *)malloc(
          sizeof(struct start_routine_wrapper_arg));
  arg->start_routine = start_routine;
  arg->real_arg = real_arg;
  arg->create_thread_id = 0; // init
  // arg->call_path_len = sampler->GetBacktrace(arg->call_path,
  // MAX_CALL_PATH_DEPTH); arg->thread = (*thread); dbg(arg->thread);
  sampler->Start();

  /** execute real pthread_create */
  int ret =
      (*original_pthread_create)(thread, attr, start_routine_wrapper, arg);
  /** ------------------------- */

  sampler->Stop();
  /** get call path / call stack / calling context */
  baguatool::type::addr_t call_path[MAX_CALL_PATH_DEPTH] = {0};
  int call_path_len = sampler->GetBacktrace(call_path, MAX_CALL_PATH_DEPTH);
  baguatool::type::addr_t *out_call_path = nullptr;
  /** recording */
  dbg(arg->create_thread_id);
  perf_data->RecordEdgeData(call_path, call_path_len, out_call_path, 0, 0, 0,
                            thread_gid, arg->create_thread_id, -1);
  // dbg(*thread);
  pthread_t_to_create_thread_id[*thread] = arg->create_thread_id;
  sampler->Start();

  free(arg);
  return ret;
}

/** pthread_join wrapper */
int pthread_join(pthread_t thread, void **value_ptr) {
  /** If module are not initialized, init it at first. */
  if (module_init != MODULE_INITED) {
    init_mock();
  }
  /** timer starts */
  auto t1 = std::chrono::high_resolution_clock::now();

  /** ------------------------- */
  /** execute real pthread_join */
  int ret = (*original_pthread_join)(thread, value_ptr);
  /** ------------------------- */

  /** timer stops */
  auto t2 = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> fp_ms = t2 - t1;
  baguatool::type::perf_data_t time = fp_ms.count() / 1000.0 * CYC_SAMPLE_COUNT;
  /** get call path / call stack / calling context */
  baguatool::type::addr_t out_call_path[MAX_CALL_PATH_DEPTH] = {0};
  int out_call_path_len =
      sampler->GetBacktrace(out_call_path, MAX_CALL_PATH_DEPTH);
  int create_thread_id = 0;
  if (pthread_t_to_create_thread_id.find(thread) !=
      pthread_t_to_create_thread_id.end()) {
    create_thread_id = pthread_t_to_create_thread_id[thread];
  }
  /** recording */
  perf_data->RecordEdgeData((baguatool::type::addr_t *)nullptr, 0,
                            out_call_path, out_call_path_len, 0, 0,
                            create_thread_id, thread_gid, time);

  return ret;
}

int pthread_mutex_init(pthread_mutex_t *mutex,
                       const pthread_mutexattr_t *attr) {
  /** If module are not initialized, init it at first. */
  if (module_init != MODULE_INITED) {
    init_mock();
  }

  /** ------------------------- */
  /** execute real pthread_mutex_init */
  int ret = (*original_pthread_mutex_init)(mutex, attr);
  /** ------------------------- */

  return ret;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
  /** If module are not initialized, init it at first. */
  if (module_init != MODULE_INITED) {
    init_mock();
  }

  long tid = gettid();

  if (tid_to_thread_gid->find(tid) == tid_to_thread_gid->end()) {
    (*tid_to_thread_gid)[tid] = thread_gid;
    // dbg(tid, thread_gid);
  }

  int ret;
  if (mutex->__data.__lock > 0) {
    void *ret_addr = __builtin_return_address(0);

    if (ret_addr && ret_addr > (void *)0x40000 &&
        ret_addr < (void *)0x40000000) { /** pthread_mutex_lock is invoked
                                            directly in the program */

      // printf("%p, ", ret_addr);
      // dbg(ret_addr);
      // printf("thread_gid = %d, lock, mutex = %lx\n", thread_gid, mutex);
      // dbg(gettid(), mutex->__data.__lock, mutex->__data.__owner);

      /** timer starts */
      auto t1 = std::chrono::high_resolution_clock::now();

      /** ------------------------- */
      /** execute real pthread_mutex_lock */
      ret = (*original_pthread_mutex_lock)(mutex);
      /** ------------------------- */

      /** timer stops */
      auto t2 = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> fp_ms = t2 - t1;
      baguatool::type::perf_data_t time =
          fp_ms.count() / 1000.0 * CYC_SAMPLE_COUNT;
      dbg(time);

      std::pair<u_int64_t, call_path_t *> tid_cp_pair;
      if (mutex_to_tid_and_callpath->find((u_int64_t)mutex) !=
          mutex_to_tid_and_callpath->end()) {
        tid_cp_pair = (*mutex_to_tid_and_callpath)[(u_int64_t)mutex];
      }

      baguatool::type::addr_t call_path[MAX_CALL_PATH_DEPTH] = {0};
      int call_path_len = sampler->GetBacktrace(call_path, MAX_CALL_PATH_DEPTH);
      // print_call_path(call_path, call_path_len);
      // printf("lock by \n");

      int src_tid = tid_cp_pair.first;

      // printf("record lock by , mutex = %lx, %ld ", mutex, src_tid);

      call_path_t *cp = tid_cp_pair.second;
      // print_call_path(cp->call_path, cp->call_path_len);

      int src_thread_id = 0;

      if (tid_to_thread_gid->find(src_tid) != tid_to_thread_gid->end()) {
        src_thread_id = (*tid_to_thread_gid)[src_tid];
      }

      perf_data->RecordEdgeData(cp->call_path, cp->call_path_len, call_path,
                                call_path_len, 0, 0, src_thread_id, thread_gid,
                                time);
    }
  } else { /** pthread_mutex_lock is not invoked directly in the program */
    /** ------------------------- */
    /** execute real pthread_mutex_lock */
    ret = (*original_pthread_mutex_lock)(mutex);
    /** ------------------------- */
  }

  return ret;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  /** If module are not initialized, init it at first. */
  if (module_init != MODULE_INITED) {
    init_mock();
  }

  /** record callpath before executing real pthread_mutex_unlock
   * to make sure pthread_mutex_lock in other thread can get locking thread */
  void *ret_addr = __builtin_return_address(0);

  if (ret_addr && ret_addr > (void *)0x40000 &&
      ret_addr < (void *)0x40000000) { /** pthread_mutex_lock is invoked
                                          directly in the program */
    // printf("%p, ", ret_addr);
    // printf("thread_gid = %d, unlock, mutex = %lx\n", thread_gid, mutex);
    // dbg(gettid());

    call_path_t *cp = new (struct call_path_t)();
    cp->call_path_len =
        sampler->GetBacktrace(cp->call_path, MAX_CALL_PATH_DEPTH);
    // print_call_path(cp->call_path, cp->call_path_len);
    (*mutex_to_tid_and_callpath)[(u_int64_t)mutex] =
        std::make_pair((u_int64_t)gettid(), cp);
    // printf("record unlock, mutex = %lx, %ld, %p\n", mutex,
    // (u_int64_t)gettid(), ret_addr);
  }

  /** ------------------------- */
  /** execute real pthread_mutex_unlock */
  int ret = (*original_pthread_mutex_unlock)(mutex);
  /** ------------------------- */

  return ret;
}
