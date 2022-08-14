import sys
import os
proj_dir = os.environ['BAGUA_DIR']
sys.path.append(proj_dir + r"/python")
import json
import perflow as pf
from pag import * 

pflow = pf.PerFlow()

# Run the binary and return a program abstraction graph
tdpag, ppag = pflow.run(binary = "cg.B.x", cmd = "srun -n 64 ./cg.B.x", nprocs = 64)

# # draw pags and save as PDF files
# pflow.draw(tdpag, save_pdf = './cg.B.8.tdpag')
# pflow.draw(ppag, save_pdf = './cg.B.8.ppag')

# Perform imbalance analysis
pflow.communication_pattern_analysis_model(64)