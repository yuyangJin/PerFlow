# PerFlow
Domain-specific framework for performance analysis of parallel programs

## For PPoPP22 AE review
This file is only for PPoPP22 AE review. We seek two badges: Artifacts Available badge and Artifacts Evaluated badges.

### Environment setup

There are two ways to set up PerFlow enviroments for AE reviewers.

1. Follow the steps in INSTALL.md to build PerFlow.

2. Directly access to our machines with preinstalled PerFlow and environments.

```bash
ssh -p 3330 ppopp22_42_perflow@166.111.68.163
cd PerFlow
source PerFlow.bashrc
```

### Artifacts Evaluated badges

We think the functionality of PerFlow can be concluded as XX parts. We give details for validation of each part.


#### A. Validation for Program Abstraction Graphs (Section 3) 
Here we use the CG benchmark in NPB to analysis program behavior with **hybrid ststic-dynamic analysis** and show **Program Abstraction Graph** (PAG) with visualized graph format (pdf).

``` bash
cd example/AE/PAG
python3 ./pag.py # python3 
```
We can see there are several files being generated.
The `cg.B.8.pcg`, `cg.B.8.pag.map`, and `cg.B.8.pag/*` files are results of static analysis, while the `SAMPLE+*.TXT`, `MPI*.TXT` files, etc., are the results of dynamic analysis.
Through data embedding, `tdpag.gml` amd `ppag.gml` are generated representing the top-down view and the parallel view of PAG, seperately.
Besides, we draw the PAG in `tdpag.pdf` amd `ppag.pdf`.
Please check these files.

#### B. Validation for Performance Analysis Passes (Section 4.3)
PerFlow provides low level APIs for building peformance analysis passes.
Here we gives an example showing building a critical path pass with low level APIs.

#### C. Validation for PerFlowGraph (Section 4.1 & 4.2)
PerFlow provides high level APIs for building PerFlowGraphs as user-defined performance analytical tasks.


#### D. Validation for Performance Analysis Models (Section 4.4)
PerFlow provides builtin performance analysis models for developers to directly use them.


#### E. Validation for Case Studies
