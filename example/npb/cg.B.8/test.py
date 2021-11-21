import sys
import os
proj_dir = os.environ['BAGUA_DIR']
sys.path.append(proj_dir + r"/python")
import json
import perflow as pf

pflow = pf.PerFlow()

# Run the binary and return a program abstraction graph
tdpag, ppag = pflow.run(binary = "./cg.B.8", cmd = "srun -n 8 ./cg.B.8", nprocs = 8)

# Build a PerFlowGraph
#V_comm = pflow.filter(pag.V, name = "MPI_*")
#V_hot = pflow.hotspot_detection(V_comm)
#attrs = ["name", "comm-info", "debug-info", "time"] 
#pflow.report(V_comm, V_hot, attrs)

# 
pflow.draw(tdpag, save_pdf = './cg.B.8.tdpag')
pflow.draw(ppag, save_pdf = './cg.B.8.ppag')