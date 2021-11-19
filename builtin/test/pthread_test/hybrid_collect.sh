#!/bin/bash
GPERF_DIR=/mnt/home/jinyuyang/MY_PROJECT/BaguaTool/build/example/project/graph_perf
# you can replace pthread_test with pthread_test_1
BIN=pthread_test_1

# Run as:
# sh ./hybrid_collect.sh 

# First phase, static analysis on binary, you should find your path to binary_analyzer
$GPERF_DIR/binary_analyzer $BIN

# Second phase, dynamic analysis on binary, you should find your path to libpthread_sampler.so
LD_PRELOAD=$GPERF_DIR/libpthread_sampler.so ./$BIN 1000000000
