#ifndef COMMON_H_
#define COMMON_H_

#define DBG_MACRO_NO_WARNING
#include "dbg.h"

#ifndef DBG_MACRO_DISABLE
#define ERR_EXIT(...) \
  do {                \
    dbg(__VA_ARGS__); \
    exit(1);          \
  } while (false)
#else
// ATTENTION: ERR_EXIT here only support one parameter!!
#define ERR_EXIT(_str_) \
  do {                  \
    perror(_str_);      \
    exit(1);            \
  } while (false)
#endif

#define UNREACHABLE() ERR_EXIT("control flow should never reach here")
#define UNIMPLEMENTED() ERR_EXIT("unimplemented here")

template <typename T>
inline void FREE_CONTAINER(T& p_container) {
  T empty;
  using std::swap;
  swap(p_container, empty);
}

#endif
