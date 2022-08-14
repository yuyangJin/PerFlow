#ifndef MPI_INIT_H_
#define MPI_INIT_H_

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifdef __cplusplus
extern "C" {
#endif

extern int mpi_rank;
extern char *addr_threshold;

#ifdef __cplusplus
}
#endif

#endif  // MPI_INIT_H_
