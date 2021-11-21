import sys
sys.path.append(r"/home/jinyuyang/workspace/MY_PROJECT/PerFlow/python")
import os
import json
import perflow as pf

pflow = pf.PerFlow()

# Run the binary and return a program abstraction graph
pag = pflow.run(binary = "./a.out", cmd = "./a.out")

# Build a PerFlowGraph
#V_comm = pflow.filter(pag.V, name = "MPI_*")
#V_hot = pflow.hotspot_detection(V_comm)
#attrs = ["name", "comm-info", "debug-info", "time"] 
#pflow.report(V_comm, V_hot, attrs)

