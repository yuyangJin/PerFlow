import sys
import os
proj_dir = os.environ['BAGUA_DIR']
sys.path.append(proj_dir + r"/python")
import json
import perflow as pf
from pag import * 

pflow = pf.PerFlow()

# Run the binary and return a program abstraction graph
tdpag, ppag = pflow.run(binary = "cg.B.x", cmd = "mpirun -np 8 ./cg.B.x", nprocs = 8)

# # draw pags and save as PDF files
# pflow.draw(tdpag, save_pdf = './cg.B.8.tdpag')
# pflow.draw(ppag, save_pdf = './cg.B.8.ppag')

# Perform imbalance analysis
pflow.imbalance_analysis(tdpag.vs, nprocs = 8)