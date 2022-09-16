find_package(MPI REQUIRED)
find_package(OpenMP REQUIRED)

# graph perf hybrid collectors
add_executable(binary_analyzer src/hybrid_collector/static/binary_analyzer.cpp)
add_library(sequential_sampler SHARED src/hybrid_collector/dynamic/sequential_sampler.cpp)
add_library(mpi_sampler SHARED src/hybrid_collector/dynamic/mpi_sampler.cpp src/hybrid_collector/dynamic/mpi_tracer.cpp) 
#src/hybrid_collector/dynamic/mpi_init.cpp)
add_library(pthread_sampler SHARED src/hybrid_collector/dynamic/pthread_sampler.cpp)
add_library(omp_sampler SHARED src/hybrid_collector/dynamic/omp_sampler.cpp)
add_library(mpi_omp_sampler SHARED src/hybrid_collector/dynamic/mpi_omp_sampler.cpp src/hybrid_collector/dynamic/mpi_tracer.cpp)
add_library(mpi_omp_profiler SHARED src/hybrid_collector/dynamic/mpi_omp_sampler.cpp)
# add_library(mpi_tracer SHARED src/hybrid_collector/dynamic/mpi_tracer.cpp)
add_executable(comm_dep_analysis src/hybrid_collector/dynamic/comm_dep_analysis.cpp)
add_executable(comm_dep_approxi_analysis src/hybrid_collector/dynamic/comm_dep_approxi_analysis.cpp)

# graph perf offline analysis library
add_library(
    graph_perf
    SHARED

    src/offline_analysis/graph_perf.cpp
)

target_include_directories(graph_perf PUBLIC src/offline_analysis)

target_link_libraries(graph_perf PRIVATE baguatool)
target_link_libraries(binary_analyzer PRIVATE baguatool)
target_link_libraries(sequential_sampler PRIVATE baguatool)
target_link_libraries(mpi_sampler PUBLIC baguatool MPI::MPI_CXX)
target_link_libraries(pthread_sampler PRIVATE baguatool)
# target_link_libraries(mpi_tracer PRIVATE baguatool unwind MPI::MPI_CXX)


# if(OpenMP_C_FOUND)
#     target_link_libraries(sampler_omp PUBLIC unwind ${PAPI_LIBRARIES} OpenMP::OpenMP_C)
#     # target_link_libraries(omp_sampler PUBLIC baguatool OpenMP::OpenMP_C)
# endif()
if(OpenMP_CXX_FOUND)
    target_link_libraries(omp_sampler PUBLIC baguatool OpenMP::OpenMP_CXX)
    target_link_libraries(mpi_omp_sampler PUBLIC baguatool MPI::MPI_CXX OpenMP::OpenMP_CXX)
    target_link_libraries(mpi_omp_profiler PUBLIC baguatool MPI::MPI_CXX OpenMP::OpenMP_CXX)
endif()

option(ENABLE_TOOL "Enable tool" ON)
option(ENABLE_TOOL_TEST "Enable tool test" ON)

if (ENABLE_TOOL)
  add_subdirectory(tools)
endif()

if (ENABLE_TOOL_TEST)
  add_subdirectory(test)
endif()