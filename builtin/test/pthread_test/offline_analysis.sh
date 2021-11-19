#!/bin/bash
GPERF_DIR=/mnt/home/jinyuyang/MY_PROJECT/BaguaTool/build/example/project/graph_perf
# you can replace pthread_test with pthread_test_1
BIN=pthread_test_1

# Run as:
# sh ./offline_analysis.sh 

# Third phase, offline analysis
# (1) Generate static program abstraction graph 
$GPERF_DIR/tools/static_pag_generation $BIN SAMPLE.TXT
python3 $GPERF_DIR/tools/draw_pag.py static_pag.gml static_pag

# (2) Generate program abstraction graph 
$GPERF_DIR/tools/pag_generation $BIN SAMPLE.TXT
python3 $GPERF_DIR/tools/draw_pag.py pag.gml pag

# (3) Generate multi-threaded program abstraction graph 
$GPERF_DIR/tools/pthread_mpag_generation_example $BIN SAMPLE.TXT
python3 $GPERF_DIR/tools/draw_pag.py multi_thread_pag.gml multi_thread_pag

# (4) Critical path on the multi-threaded program abstraction graph
python3 $GPERF_DIR/tools/critical_path.py multi_thread_pag.gml