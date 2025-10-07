Communication Structure Tree (CST) - Implementation Guide
## Overview

The Communication Structure Tree (CST) is a **pruned version** of a Program Structure 
Graph (PSG) that focuses on MPI communication operations and their calling context. 

## Key Characteristics

1. **Generated from PSG**: CST is built by pruning a PSG, not created independently
2. **MPI nodes as leaves**: All MPI communication operations appear as leaf nodes
3. **Preserves structure**: The call, loop, and branch structure from PSG is maintained
4. **Automatic pruning**: Non-MPI branches are automatically removed

## Relationship to PSG

```
PSG (Full Program Structure)
├── main
    ├── init_loop (no MPI)          ← PRUNED
    │   └── initialize_data         ← PRUNED
    ├── compute_loop (has MPI)      ← KEPT
    │   └── inner_loop              ← KEPT
    │       ├── MPI_Send            ← KEPT (MPI leaf)
    │       └── MPI_Recv            ← KEPT (MPI leaf)
    └── final_loop (no MPI)         ← PRUNED
        └── cleanup_data            ← PRUNED

CST (Pruned for Communication)
├── main
    └── compute_loop
        └── inner_loop
            ├── MPI_Send [MPI]
            └── MPI_Recv [MPI]
```

## Implementation Details

### Building CST from PSG

```python
from perflow.perf_data_struct.static import ProgramStructureGraph
from perflow.perf_data_struct.static.comm_structure_tree import CommStructureTree, CommType

# 1. Create PSG
psg = ProgramStructureGraph()
func = psg.addFunctionNode(1, "main", "main.c", 10)
loop = psg.addLoopNode(2, "compute_loop", parent_id=1, loop_type="for")
mpi_call = psg.addCallSite(3, caller_id=2, callee_name="MPI_Send")

# 2. Identify MPI nodes
mpi_node_ids = {3}  # Node ID of MPI_Send
comm_type_map = {3: CommType.POINT_TO_POINT}

# 3. Build CST by pruning PSG
cst = CommStructureTree()
cst.buildFromPSG(psg, mpi_node_ids, comm_type_map)

# 4. Analyze communication structure
print(f"MPI nodes: {len(cst.getMPINodes())}")
print(f"Max nesting depth: {cst.getMaxNestingDepth()}")
patterns = cst.getCommunicationPattern()
```

### Pruning Algorithm

The CST is built using the following algorithm:

1. **Identify MPI nodes**: Mark all nodes in PSG that represent MPI operations
2. **Find ancestors**: For each MPI node, find all ancestors (parent, grandparent, etc.)
3. **Include relevant nodes**: Copy only MPI nodes and their ancestors to CST
4. **Preserve edges**: Maintain parent-child relationships between included nodes
5. **Result**: A tree where all paths lead to MPI leaf nodes

### Supported Node Types

CST preserves the following node types from PSG:
- **FUNCTION**: Function definitions (e.g., `main`, `compute_kernel`)
- **LOOP**: Loop constructs (e.g., `for`, `while` loops)
- **CALL_SITE**: Function calls (including MPI calls)
- **BRANCH**: Conditional branches (e.g., `if` statements)
- **BASIC_BLOCK**: Basic blocks in control flow

### MPI Communication Types

CST supports the following MPI communication types:

- **POINT_TO_POINT**: MPI_Send, MPI_Recv, MPI_Isend, MPI_Irecv
- **BROADCAST**: MPI_Bcast
- **REDUCE**: MPI_Reduce
- **ALLREDUCE**: MPI_Allreduce
- **SCATTER**: MPI_Scatter
- **GATHER**: MPI_Gather
- **ALLTOALL**: MPI_Alltoall
- **BARRIER**: MPI_Barrier
- **ONE_SIDED**: MPI_Put, MPI_Get, MPI_Accumulate

## Use Cases

### 1. Communication Pattern Analysis

```python
# Get communication patterns
patterns = cst.getCommunicationPattern()
print(f"Point-to-point: {patterns['p2p']}")
print(f"Collective: {patterns['collective']}")
```

### 2. Hotspot Detection

```python
# Find MPI operations by type
sends = cst.getMPINodesByType(CommType.POINT_TO_POINT)
collectives = cst.getMPINodesByType(CommType.BROADCAST)

print(f"P2P operations: {len(sends)}")
print(f"Broadcast operations: {len(collectives)}")
```

### 3. Nesting Depth Analysis

```python
# Analyze calling depth
max_depth = cst.getMaxNestingDepth()
print(f"Maximum calling depth to MPI: {max_depth}")
```

### 4. Communication Structure Visualization

The CST provides a clear view of where MPI operations occur in the program:

```python
for node in cst.getNodes():
    is_mpi = node.getAttribute("is_mpi")
    indent = "  " * get_node_depth(node)
    marker = "[MPI]" if is_mpi else ""
    print(f"{indent}{node.getName()} {marker}")
```

## Advantages of CST

1. **Focused view**: Shows only communication-relevant code structure
2. **Performance analysis**: Identifies communication bottlenecks
3. **Optimization guidance**: Highlights nested communication patterns
4. **Reduced complexity**: Removes non-communication code for clarity
5. **Context preservation**: Maintains calling context for MPI operations

## Comparison: CST vs PSG

| Aspect | PSG | CST |
|--------|-----|-----|
| Scope | Full program structure | Communication-focused |
| Nodes | All program elements | Only MPI and ancestors |
| Purpose | Complete static analysis | Communication analysis |
| Size | Larger (full program) | Smaller (pruned) |
| Leaf nodes | Various (statements, etc.) | MPI operations only |

## Performance Considerations

- **Memory**: CST uses less memory than PSG (pruned structure)
- **Analysis speed**: Faster to analyze communication patterns
- **Build time**: O(N + E) where N = PSG nodes, E = PSG edges
- **Query time**: O(1) for node lookups, O(M) for MPI node queries (M = MPI nodes)

## Example Output

Running the example `example_cst_from_psg.py`:

```
=== Program Structure Graph (PSG) ===
Total nodes: 9
Total edges: 8
Functions: 1
Loops: 4

=== Communication Structure Tree (CST) ===
Total nodes: 5
Total edges: 4
MPI nodes: 2
Point-to-point: 2
Collective: 0
Max nesting depth: 3

Pruned nodes: 4
  [2] init_loop (type: loop)
  [3] initialize_data (type: call_site)
  [8] final_loop (type: loop)
  [9] cleanup_data (type: call_site)
```

## Summary

The Communication Structure Tree is a powerful abstraction for analyzing MPI 
communication in parallel programs. By pruning the PSG to focus on communication 
operations, it provides a clear and focused view of where and how communication 
occurs in the program's control flow structure.
