configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pthread_test/Makefile ${CMAKE_CURRENT_BINARY_DIR}/pthread_test/Makefile COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pthread_test/pthread_test.cpp ${CMAKE_CURRENT_BINARY_DIR}/pthread_test/pthread_test.cpp COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pthread_test/pthread_test_1.cpp ${CMAKE_CURRENT_BINARY_DIR}/pthread_test/pthread_test_1.cpp COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pthread_test/hybrid_collect.sh ${CMAKE_CURRENT_BINARY_DIR}/pthread_test/hybrid_collect.sh COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pthread_test/offline_analysis.sh ${CMAKE_CURRENT_BINARY_DIR}/pthread_test/offline_analysis.sh COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pthread_test/README ${CMAKE_CURRENT_BINARY_DIR}/pthread_test/README COPYONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pthread_test/sort_test.sh ${CMAKE_CURRENT_BINARY_DIR}/pthread_test/sort_test.sh COPYONLY)