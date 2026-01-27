# Copyright 2024 PerFlow Authors
# Licensed under the MIT License

# Source files
set(SAMPLER_SOURCES
  mpi_sampler.cpp
  call_stack.cpp
  data_collection.cpp
)

# Only build if PAPI is found
if(PAPI_FOUND)
  # Create sampler library
  add_library(perflow_sampler SHARED ${SAMPLER_SOURCES})

  # Include directories
  target_include_directories(perflow_sampler PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${PAPI_INCLUDE_DIR}
    ${MPI_CXX_INCLUDE_PATH}
  )

  # Link libraries
  target_link_libraries(perflow_sampler
    ${PAPI_LIBRARY}
    ${MPI_CXX_LIBRARIES}
    Threads::Threads
    dl
  )

  # Set compile features
  target_compile_features(perflow_sampler PRIVATE cxx_std_14)
  target_compile_options(perflow_sampler PRIVATE -Wall -Wextra -fPIC)

  # Add MPI compile flags
  if(MPI_CXX_COMPILE_FLAGS)
    target_compile_options(perflow_sampler PRIVATE ${MPI_CXX_COMPILE_FLAGS})
  endif()

  # Installation
  install(TARGETS perflow_sampler
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
  )
endif()
