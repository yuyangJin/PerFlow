import sys
import os
proj_dir = os.environ['BAGUA_DIR']
sys.path.append(proj_dir + r"/python")
import json
import perflow as pf
from pag import * 

pflow = pf.PerFlow()

# Run the binary and return program abstraction graphs
tdpag, ppag = pflow.run(binary = "./cg.B.8", cmd = "srun -n 8 -p V132 ./cg.B.8", nprocs = 8)

# Directly use a builtin model
pflow.mpi_profiler_model(tdpag = tdpag, ppag = ppag)