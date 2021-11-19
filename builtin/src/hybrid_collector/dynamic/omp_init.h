#ifndef OMP_INIT_H_
#define OMP_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif

void GOMP_parallel(void (*fn)(void *), void *data, unsigned num_threads, unsigned int flags);

#ifdef __cplusplus
}
#endif

#endif  // OMP_INIT_H_
