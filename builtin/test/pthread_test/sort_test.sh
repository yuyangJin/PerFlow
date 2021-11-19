#!/bin/bash
GPERF_DIR=/mnt/home/jinyuyang/MY_PROJECT/BaguaTool/build/example/project/graph_perf
# you can replace pthread_test with pthread_test_1
BIN=pthread_test_1

# Run as:
# sh ./sort_test.sh 

# Third phase, sort test
$GPERF_DIR/tools/sort_test $BIN SAMPLE.TXT
python3 $GPERF_DIR/tools/draw_pag.py static_pag.gml static_pag
