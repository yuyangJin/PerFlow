cmake_minimum_required(VERSION 3.16)

project(baguatool CXX C)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  message(FATAL_ERROR "Do not support non-Linux now!")
endif()

# TODO: remove it in the future
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Set build type to DEBUG" FORCE)
message(AUTHOR_WARNING "Overwrite build type to DEBUG for development")

# # output path
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# third party
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/igraph)
target_compile_options(igraph PRIVATE -fPIC)

find_package(PAPI REQUIRED)
find_package(Dyninst REQUIRED)
find_package(Boost REQUIRED)

if (Dyninst_FOUND) 
  message(STATUS "Found Dyninst: " ${DYNINST_INCLUDE_DIR})
endif()

#option(ENABLE_TEST "Enable test" ON)
option(ENABLE_EXAMPLE "Enable example" ON)
option(ENABLE_SANITIZE "Enable sanitize" OFF)

# explicitly list all source file
add_library(
  baguatool
  SHARED
  
  include/common/common.h
  include/common/tprintf.h
  include/common/utils.cpp
  src/core/graph.cpp
  src/core/pg.cpp
  src/core/pag.cpp
  src/core/pcg.cpp
  src/core/cfg.cpp
  src/core/perf_data.cpp
  src/core/graph_perf_data.cpp
  src/core/bagua_type.cpp

  #src/graph_perf/preprocessing/preprocess.cpp
  src/collector/static/dyninst/static_analysis.cpp
  src/collector/dynamic/papi/sampler.cpp
  src/collector/dynamic/shared_obj_analysis.cpp

  # src/hybrid_analysis/graph_perf.cpp
)




set_target_properties(
  baguatool

  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)



set_property(TARGET baguatool PROPERTY CXX_STANDARD 17)
set_property(TARGET baguatool PROPERTY CXX_STANDARD_REQUIRED ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.0.0")
    message(FATAL_ERROR "Insufficient GCC version (${CMAKE_CXX_COMPILER_VERSION}), need > 7.0.0")
  endif()
else()
  message(FATAL_ERROR "Do not support cxx compiler for ${CMAKE_CXX_COMPILER_ID}, choose GNU please")
endif()

target_compile_options(
  baguatool PRIVATE
  -Wall -Wno-missing-braces
  #-fno-rtti # We cannot open it because dyninst use `typeid` in header
  -fno-omit-frame-pointer -fstack-protector-all
  -rdynamic
)


# FIXME: how to use sanitize for subproject?
if (ENABLE_SANITIZE) 
  target_compile_options(
    baguatool PUBLIC
    $<$<CONFIG:DEBUG>:-fsanitize=address -g>
  )
endif()

target_include_directories(baguatool PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(baguatool PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(baguatool PUBLIC ${PROJECT_SOURCE_DIR}/third_party/dbg)
target_include_directories(baguatool PUBLIC ${PROJECT_SOURCE_DIR}/third_party/json)
#target_include_directories(baguatool PUBLIC ${PROJECT_SOURCE_DIR}/third_party/toml)
#target_include_directories(baguatool PUBLIC ${PROJECT_SOURCE_DIR}/third_party/pmpi_wrapper)
target_include_directories(baguatool PRIVATE igraph)
target_include_directories(baguatool PRIVATE ${DYNINST_INCLUDE_DIR})
target_include_directories(baguatool PRIVATE ${PAPI_INCLUDE_DIRS})
target_include_directories(baguatool PRIVATE ${Boost_INCLUDE_DIRS})

# if(OpenMP_CXX_FOUND)
#     target_link_libraries(baguatool PUBLIC OpenMP::OpenMP_CXX)
# endif()

target_link_libraries(baguatool PRIVATE igraph)
target_link_libraries(
  baguatool
  PRIVATE
  # TODO: we do not need all these
  dyninstAPI parseAPI instructionAPI symtabAPI symLite dynDwarf dynElf common unwind)
target_link_libraries(baguatool PRIVATE ${PAPI_LIBRARIES})
target_link_libraries(baguatool PRIVATE ${Boost_LIBRARIES})






add_subdirectory(${PROJECT_SOURCE_DIR}/builtin)

if (ENABLE_TEST)
  # List what we want to test
  #add_subdirectory(${PROJECT_SOURCE_DIR}/test/igraph_test)
endif()



install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION  ${CMAKE_INSTALL_PREFIX}/include)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/third_party/dbg/ DESTINATION  ${CMAKE_INSTALL_PREFIX}/include)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/third_party/json/ DESTINATION  ${CMAKE_INSTALL_PREFIX}/include)



install(TARGETS baguatool DESTINATION lib)
# install(TARGETS binary_analyzer DESTINATION bin)
# install(TARGETS sequential_sampler DESTINATION lib)
# install(TARGETS pthread_sampler DESTINATION lib)
# install(TARGETS mpi_sampler DESTINATION lib)
# install(TARGETS omp_sampler DESTINATION lib)
