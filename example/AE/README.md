# For PPoPP22 AE review

This file is written for PPoPP22 AE review. We seek two badges: **Artifacts Available badge** and **Artifacts Evaluated badges**.

Github repo: https://github.com/yuyangJin/PerFlow

## Artifacts Available badge

We publish PerFlow at:

```
https://doi.org/10.5281/zenodo.5731301
```

## Artifacts Evaluated badges

We think the functionality of PerFlow can be concluded as four parts. We give detailed instructions for the validation of each part.

(We only show some **stable** builtin passes and models for AE review. Other unstable passes and models are in a private repo. We think that these examples are enough to validate the framework design and the functionality of PerFlow.)

### Environment setup

There are two ways to set up PerFlow environments for AE reviewers:

1. Follow the steps in INSTALL.md to build PerFlow's environments.

2. Directly access to our machines with preinstalled PerFlow and environments.
   
   The step-by-step instructions (not available now).

### A. Validation for Program Abstraction Graphs (Section 3)

PerFlow abstracts performance behaviors of programs as **Program Abstraction Graphs**  (PAG).

Here we run the CG (CLASS B and 8 processes) in NPB benchmarks (**MPI** programs) to validate how PerFlow abstracts program behavior with **hybrid static-dynamic analysis** and show **Program Abstraction Graph** (PAG) with visualized graph format  (`pag_validation.py`).

#### Instructions:

The step-by-step instructions:

```bash
cd build/example/AE/pag_validation
python3 ./pag_validation.py # python3
```

#### Results:

We can see several files are generated.

**Hybrid static-dynamic analysis**: The `cg.B.8.pcg`, `cg.B.8.pag.map`, and `cg.B.8.pag/*` files are results of *static analysis*, while the `SAMPLE+*.TXT`, `MPI*.TXT` files, etc., are the results of *dynamic analysis*.

**Program Abstraction Graph (PAG)**: Through *data embedding*, `cg.B.8.tdpag.gml` amd `cg.B.8.ppag.gml` are generated representing the top-down view and the parallel view of PAG, separately. Besides, we draw the PAG in `cg.B.8.tdpag.pdf` and `cg.B.8.ppag.pdf`.

Please check these files.

### B. Validation for PerFlowGraph (Section 4.1 & 4.2)

PerFlow provides high-level APIs and builtin passes to build PerFlowGraphs for performing user-defined performance analytical tasks.

We build a PerFlowGraph as an **MPI profiler** (`perflowgraph_validation.py`) with a filter pass, a hotspot detection pass, and a report pass.

Here we run CG (CLASS B and 8 processes) in NPB benchmarks (**MPI** programs) for validation.

#### Code:

The code is shown as follows.

```python
# perflowgraph_validation.py
#############################
#        MPI Profiler       #
#############################

import perflow as pf
from pag import * 

pflow = pf.PerFlow()

# Run the binary and return program abstraction graphs
tdpag, ppag = pflow.run(binary = "./cg.B.8", 
                            cmd = "srun -n 8 -p V132 ./cg.B.8", nprocs = 8)

# Build a PerFlowGraph
## a filter pass
V_comm = pflow.filter(tdpag.vs, name = "mpi_")
## a hotspot detection pass
V_hot = pflow.hotspot_detection(V_comm)
## a report pass
attrs_list = ["name", "CYCAVGPERCENT", "saddr"]  # name -> name; 
                                                                              # CYCAVGPERCENT -> proportion of total time; 
                                                              # saddr -> start address (debug info)
pflow.report(V = V_hot, attrs = attrs_list)
```

#### The step-by-step instructions:

```bash
cd build/example/AE/perflowgraph_validation
python3 ./perflowgraph_validation.py
```

#### Results:

**PerFlowGraph**: The mpi profiler will directly output hot MPI function calls and show the corresponding attributes defined in the report pass. The **expected output** is shown as follow.

```tex
name    CYCAVGPERCENT   saddr
mpi_finalize_   0.000132        4218031.0
mpi_wait_       0.000330        4199730.0
mpi_send_       0.000490        4200456.0
mpi_send_       0.000283        4200862.0
mpi_wait_       0.000311        4201508.0
mpi_irecv_      0.000519        4200310.0
mpi_send_       0.007111        4200456.0
mpi_wait_       0.000104        4200497.0
mpi_irecv_      0.000123        4200781.0
mpi_send_       0.003810        4200862.0
mpi_wait_       0.004008        4201508.0
mpi_wait_       0.000462        4202270.0
mpi_send_       0.000481        4203146.0
mpi_wait_       0.000387        4204129.0
mpi_send_       0.000330        4203555.0
```

Please check the output.

### C. Validation for Performance Analysis Passes (Section 4.3)

PerFlow provides low-level APIs for building user-defined performance analysis passes.

Here we give an example showing building a **critical path pass** with low-level APIs. We use a **max flow search** algorithm to identify critical paths (`pass_validation.py`).

We run a multi-threaded micro-benchmark (a **Pthreads** program) to show the results of the critical path pass.

#### Instructions:

The step-by-step instructions:

```bash
cd build/example/AE/pass_validation
python3 ./pass_validation.py # python3
```

#### Results:

There are several files being generated.

**PAG**: The `pthread.tdpag.pdf` and `pthread.ppag.pdf` files show the top-down view and the parallel view of PAG. 

**Critical path pass**: The critical path pass is performed on the `pthread.ppag` (parallel view of PAG). The `critical_path.pdf` file shows the result of the critical path pass. In this pdf, the identified critical path edges are marked as bold orange arrows.

Please check these files.

### D. Validation for Performance Analysis Models (Section 4.4)

PerFlow provides builtin performance analysis models for developers to directly use them.

We take an **MPI Profiler model** (same as B) to validate that PerFlow provides builtin analysis models.

We still run CG (CLASS B and 8 processes) in NPB benchmarks (**MPI** programs).

#### Code:

The code is shown as follow:

```python
import perflow as pf
from pag import * 

pflow = pf.PerFlow()

# Run the binary and return program abstraction graphs
tdpag, ppag = pflow.run(binary = "./cg.B.8", 
                            cmd = "srun -n 8 -p V132 ./cg.B.8", nprocs = 8)

# Directly use a builtin model
pflow.mpi_profiler_model(tdpag = tdpag, ppag = ppag)
```

#### Instructions:

The step-by-step instructions:

```bash
cd build/example/AE/model_validation
python3 ./model_validation.py # python3
```

#### Results:

**Output** The expect results is the same as B.

```tex
name    CYCAVGPERCENT   saddr
mpi_finalize_   0.000133        4218031.0
mpi_wait_       0.000114        4199730.0
mpi_send_       0.000691        4200456.0
mpi_send_       0.000521        4200862.0
mpi_wait_       0.000493        4201508.0
mpi_irecv_      0.000644        4200310.0
mpi_send_       0.005977        4200456.0
mpi_send_       0.003183        4200862.0
mpi_wait_       0.004064        4201508.0
mpi_wait_       0.000606        4202270.0
mpi_send_       0.000644        4203146.0
mpi_wait_       0.000379        4204129.0
mpi_send_       0.000341        4203555.0
```
