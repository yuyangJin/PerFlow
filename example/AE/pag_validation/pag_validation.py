import sys
import os
proj_dir = os.environ['BAGUA_DIR']
sys.path.append(proj_dir + r"/python")
import json
import perflow as pf
from pag import * 

pflow = pf.PerFlow()

# Run the binary and return a program abstraction graph
tdpag, ppag = pflow.run(binary = "./cg.B.8", cmd = "srun -n 8 -p V132 ./cg.B.8", nprocs = 8)

# Build a PerFlowGraph
#V_comm = pflow.filter(tdpag.vs, type = PagType.MPI_NODE)
#print(V_comm)
V_hot = pflow.hotspot_detection(tdpag.vs)
print([(v["name"], v['CYCAVGPERCENT']) for v in V_hot])
#attrs = ["name", "comm-info", "debug-info", "time"] 
#pflow.report(V_comm, V_hot, attrs)

# 
#pflow.draw(tdpag, save_pdf = './cg.B.8.tdpag')
#pflow.draw(ppag, save_pdf = './cg.B.8.ppag')