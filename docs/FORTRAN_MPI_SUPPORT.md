# Fortran MPI Support in PerFlow

## Overview

PerFlow MPI sampler now fully supports Fortran MPI applications in addition to C/C++ applications. This enables performance profiling of Fortran MPI programs using the same LD_PRELOAD mechanism.

## Supported Fortran Functions

### MPI_Init Variants
All Fortran name mangling conventions are supported:
- `MPI_INIT` (all uppercase)
- `mpi_init` (all lowercase)  
- `mpi_init_` (lowercase with single trailing underscore)
- `mpi_init__` (lowercase with double trailing underscore)

### MPI_Init_thread Variants
All Fortran name mangling conventions are supported:
- `MPI_INIT_THREAD`
- `mpi_init_thread`
- `mpi_init_thread_`
- `mpi_init_thread__`

## Compiler Compatibility

The implementation automatically detects which name mangling convention is used by your Fortran compiler:

- **gfortran**: Typically uses `mpi_init_` (single underscore)
- **ifort**: May use `mpi_init__` (double underscore)
- **Other compilers**: Support for uppercase or plain lowercase variants

No configuration is needed - the sampler automatically handles all variants.

## Usage

### Building Fortran Test

```bash
cd build
cmake ..
make mpi_sampler_fortran_test
```

### Running with Sampler

```bash
# Using LD_PRELOAD
LD_PRELOAD=./lib/libperflow_mpi_sampler.so \
  PERFLOW_OUTPUT_DIR=/tmp \
  mpirun -n 4 ./your_fortran_program

# Or use the test target
make run_mpi_sampler_fortran_test
```

### Example Fortran Program

```fortran
program my_mpi_app
    use mpi
    implicit none
    
    integer :: ierr, rank, size
    
    ! Initialize MPI - will be intercepted by sampler
    call MPI_Init(ierr)
    
    ! Get rank and size
    call MPI_Comm_rank(MPI_COMM_WORLD, rank, ierr)
    call MPI_Comm_size(MPI_COMM_WORLD, size, ierr)
    
    ! Your computation here
    ! ...
    
    ! Finalize MPI
    call MPI_Finalize(ierr)
end program my_mpi_app
```

## Implementation Details

### How It Works

1. **Function Interposition**: The sampler library exports all Fortran MPI_Init variants
2. **Wrapper Functions**: Each variant calls a common wrapper function with a flag indicating which variant was called
3. **PMPI Delegation**: The wrapper calls the appropriate PMPI function (e.g., `pmpi_init_`) from the real MPI library
4. **Rank Capture**: After MPI initialization, the sampler captures the process rank using `MPI_Comm_rank`

### Weak Symbols

For shared library builds, weak symbols are used to avoid linking errors when a particular variant is not needed:

```c
#ifdef PIC
#pragma weak pmpi_init
#pragma weak PMPI_INIT
#pragma weak pmpi_init_
#pragma weak pmpi_init__
#endif
```

This allows the sampler to work with any Fortran compiler without requiring all PMPI variants to be present.

## Mixed-Language Applications

The sampler supports applications that mix C/C++ and Fortran code:
- Both `MPI_Init` (C) and `mpi_init_` (Fortran) are intercepted
- Rank identification works consistently regardless of which language initializes MPI
- No special configuration or build flags needed

## Troubleshooting

### "Couldn't find fortran pmpi_init function" Error

This error occurs when building as a shared library (.so) but the PMPI functions are not available. Solutions:
1. Link against the MPI library as a shared library (most common)
2. Build as a static library instead
3. Ensure MPI is properly installed with Fortran support

### Rank Not Captured

If you see "Warning: MPI rank was not captured", it means:
1. MPI_Init was not called before program exit
2. MPI_Init was called but initialization failed
3. The sampler was loaded after MPI initialization (use LD_PRELOAD)

Check that:
- Your program calls MPI_Init
- MPI_Init succeeds (check return value)
- LD_PRELOAD is set before launching your program

## Testing

The repository includes a comprehensive Fortran test program:

```bash
# Build the test
cd build
make mpi_sampler_fortran_test

# Run without sampler (verify program works)
mpirun -n 4 ./tests/mpi_sampler_fortran_test

# Run with sampler
LD_PRELOAD=./lib/libperflow_mpi_sampler.so \
  PERFLOW_OUTPUT_DIR=/tmp \
  mpirun -n 4 ./tests/mpi_sampler_fortran_test

# Check output
ls -la /tmp/perflow_mpi_rank_*.pflw
```

Expected output should include:
```
[MPI Sampler] Captured MPI rank: 0 (Fortran static init)
[MPI Sampler] Captured MPI rank: 1 (Fortran static init)
...
```

## Performance

There is no performance overhead for Fortran applications compared to C/C++ applications:
- Same sampling mechanism (PAPI hardware counters)
- Same data structures
- Fortran wrapper functions are minimal (just parameter conversion)

The only difference is the initial MPI_Init interception, which happens once per process.
