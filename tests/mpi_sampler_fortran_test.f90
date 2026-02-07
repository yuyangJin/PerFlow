! Copyright 2024 PerFlow Authors
! Licensed under the Apache License, Version 2.0

! @file mpi_sampler_fortran_test.f90
! @brief Fortran test program for MPI Performance Sampler
!
! This test program performs computational operations to generate
! samples for the MPI sampler to collect using Fortran MPI initialization.

program mpi_sampler_fortran_test
    use mpi
    implicit none
    
    integer :: ierr, rank, size
    integer :: i, j, k, iter
    integer, parameter :: matrix_size = 100
    integer, parameter :: num_iterations = 10
    real(8), dimension(matrix_size, matrix_size) :: A, B, C
    real(8) :: start_time, end_time, total_time
    real(8) :: sum_val
    
    ! Initialize MPI
    call MPI_Init(ierr)
    if (ierr /= MPI_SUCCESS) then
        print *, 'Error initializing MPI'
        stop 1
    end if
    
    ! Get rank and size
    call MPI_Comm_rank(MPI_COMM_WORLD, rank, ierr)
    call MPI_Comm_size(MPI_COMM_WORLD, size, ierr)
    
    if (rank == 0) then
        print *, '[Fortran Test] MPI Sampler Test Started (total processes: ', size, ')'
        print *, '[Fortran Test] Testing matrix multiplication (', matrix_size, 'x', matrix_size, ')...'
    end if
    
    ! Give the sampler time to initialize
    call MPI_Barrier(MPI_COMM_WORLD, ierr)
    
    ! Start timing
    start_time = MPI_Wtime()
    
    ! Initialize matrices
    do i = 1, matrix_size
        do j = 1, matrix_size
            A(i,j) = real(rank * 1000 + i * matrix_size + j, 8) / 1000000.0
            B(i,j) = real(rank * 1000 + i + j, 8) / 1000000.0
        end do
    end do
    
    ! Perform matrix multiplication multiple times
    do iter = 1, num_iterations
        ! Matrix multiplication: C = A * B
        do i = 1, matrix_size
            do j = 1, matrix_size
                C(i,j) = 0.0
                do k = 1, matrix_size
                    C(i,j) = C(i,j) + A(i,k) * B(k,j)
                end do
            end do
        end do
    end do
    
    ! Calculate checksum
    sum_val = 0.0
    do i = 1, matrix_size
        do j = 1, matrix_size
            sum_val = sum_val + C(i,j)
        end do
    end do
    
    end_time = MPI_Wtime()
    total_time = end_time - start_time
    
    ! Print results only from rank 0
    if (rank == 0) then
        print *, '[Fortran Test] Matrix multiplication complete in ', total_time, ' seconds'
        print *, '[Fortran Test] Checksum: ', sum_val
    end if
    
    ! Test some MPI collective operations
    call test_mpi_collectives(rank, size)
    
    call MPI_Barrier(MPI_COMM_WORLD, ierr)
    
    if (rank == 0) then
        print *, '[Fortran Test] All tests completed successfully!'
        print *, '[Fortran Test] Check output files: /tmp/perflow_mpi_rank_*.pflw'
    end if
    
    ! Finalize MPI
    call MPI_Finalize(ierr)
    
contains

    subroutine test_mpi_collectives(rank, size)
        implicit none
        integer, intent(in) :: rank, size
        integer :: ierr, i, iter
        real(8), dimension(100) :: send_data, recv_data
        real(8) :: local_value, global_sum
        real(8) :: start_time, end_time
        
        start_time = MPI_Wtime()
        
        ! Initialize data
        do i = 1, 100
            send_data(i) = real(rank * 100 + i, 8)
        end do
        
        ! Test MPI_Bcast
        do iter = 1, num_iterations
            call MPI_Bcast(send_data, 100, MPI_DOUBLE_PRECISION, 0, MPI_COMM_WORLD, ierr)
        end do
        
        ! Test MPI_Reduce
        do iter = 1, num_iterations
            call MPI_Reduce(send_data, recv_data, 100, MPI_DOUBLE_PRECISION, &
                           MPI_SUM, 0, MPI_COMM_WORLD, ierr)
        end do
        
        ! Test MPI_Allreduce
        local_value = real(rank + 1, 8)
        do iter = 1, num_iterations
            call MPI_Allreduce(local_value, global_sum, 1, MPI_DOUBLE_PRECISION, &
                              MPI_SUM, MPI_COMM_WORLD, ierr)
        end do
        
        call MPI_Barrier(MPI_COMM_WORLD, ierr)
        end_time = MPI_Wtime()
        
        if (rank == 0) then
            print *, '[Fortran Test] MPI collectives complete in ', end_time - start_time, ' seconds'
            print *, '[Fortran Test] Global sum: ', global_sum
        end if
        
    end subroutine test_mpi_collectives

end program mpi_sampler_fortran_test
