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
## a filter pass
V_comm = pflow.filter(tdpag.vs, name = "mpi_")
## a hotspot detection pass
V_hot = pflow.hotspot_detection(V_comm)
## a report pass
attrs_list = ["name", "CYCAVGPERCENT", "saddr"] 
pflow.report(V = V_hot, attrs = attrs_list)
