"""
Example demonstrating how to build a Communication Structure Tree (CST) from a 
Program Structure Graph (PSG).

The CST is a pruned version of the PSG that only keeps nodes leading to MPI 
communication operations, preserving the call, loop, and branch structure.
"""

from perflow.perf_data_struct.static import ProgramStructureGraph, NodeType
from perflow.perf_data_struct.static.comm_structure_tree import CommStructureTree, CommType


def create_example_psg():
    """
    Create an example Program Structure Graph.
    
    The PSG represents a parallel program with the following structure:
    - main function
      - initialization loop (no MPI)
      - computation loop
        - inner loop with MPI_Send/Recv
      - finalization loop (no MPI)
    
    Returns:
        Tuple of (psg, mpi_node_ids, comm_type_map)
    """
    psg = ProgramStructureGraph()
    
    # Main function
    main = psg.addFunctionNode(1, "main", "main.c", line_number=10)
    psg.setEntryPoint(1)
    
    # Initialization loop - no MPI, will be pruned
    init_loop = psg.addLoopNode(2, "init_loop", parent_id=1, loop_type="for")
    init_compute = psg.addCallSite(3, caller_id=2, callee_name="initialize_data")
    
    # Computation loop with MPI - will be kept
    comp_loop = psg.addLoopNode(4, "comp_loop", parent_id=1, loop_type="for")
    inner_loop = psg.addLoopNode(5, "inner_loop", parent_id=4, loop_type="for")
    
    # MPI communication calls
    mpi_send = psg.addCallSite(6, caller_id=5, callee_name="MPI_Send")
    mpi_recv = psg.addCallSite(7, caller_id=5, callee_name="MPI_Recv")
    
    # Finalization loop - no MPI, will be pruned
    final_loop = psg.addLoopNode(8, "final_loop", parent_id=1, loop_type="for")
    final_compute = psg.addCallSite(9, caller_id=8, callee_name="cleanup_data")
    
    # Identify MPI nodes
    mpi_node_ids = {6, 7}
    comm_type_map = {
        6: CommType.POINT_TO_POINT,
        7: CommType.POINT_TO_POINT
    }
    
    return psg, mpi_node_ids, comm_type_map


def print_psg_structure(psg):
    """Print the structure of a Program Structure Graph."""
    print("\n=== Program Structure Graph (PSG) ===")
    print(f"Total nodes: {psg.getNodeCount()}")
    print(f"Total edges: {psg.getEdgeCount()}")
    print(f"Functions: {psg.getFunctionCount()}")
    print(f"Loops: {psg.getLoopCount()}")
    
    print("\nNodes:")
    for node in psg.getNodes():
        print(f"  [{node.getId()}] {node.getName()} (type: {node.getType()})")


def print_cst_structure(cst):
    """Print the structure of a Communication Structure Tree."""
    print("\n=== Communication Structure Tree (CST) ===")
    print(f"Total nodes: {cst.getNodeCount()}")
    print(f"Total edges: {cst.getEdgeCount()}")
    print(f"MPI nodes: {len(cst.getMPINodes())}")
    print(f"Point-to-point: {cst.getPointToPointCount()}")
    print(f"Collective: {cst.getCollectiveCount()}")
    print(f"Max nesting depth: {cst.getMaxNestingDepth()}")
    
    print("\nNodes (after pruning):")
    for node in cst.getNodes():
        is_mpi = node.getAttribute("is_mpi")
        mpi_marker = " [MPI]" if is_mpi else ""
        print(f"  [{node.getId()}] {node.getName()} (type: {node.getType()}){mpi_marker}")
    
    print("\nCommunication patterns:")
    patterns = cst.getCommunicationPattern()
    print(f"  Point-to-point: {patterns['p2p']}")
    print(f"  Collective: {patterns['collective']}")


def demonstrate_pruning():
    """Demonstrate how CST prunes non-MPI branches from PSG."""
    print("="*70)
    print("Demonstrating CST Pruning from PSG")
    print("="*70)
    
    # Create PSG
    psg, mpi_node_ids, comm_type_map = create_example_psg()
    
    # Print PSG structure
    print_psg_structure(psg)
    
    # Build CST from PSG
    cst = CommStructureTree()
    cst.buildFromPSG(psg, mpi_node_ids, comm_type_map)
    
    # Print CST structure
    print_cst_structure(cst)
    
    # Analyze what was pruned
    print("\n=== Pruning Analysis ===")
    psg_node_ids = {node.getId() for node in psg.getNodes()}
    cst_node_ids = {node.getId() for node in cst.getNodes()}
    pruned_node_ids = psg_node_ids - cst_node_ids
    
    print(f"Nodes in PSG: {len(psg_node_ids)}")
    print(f"Nodes in CST: {len(cst_node_ids)}")
    print(f"Pruned nodes: {len(pruned_node_ids)}")
    
    if pruned_node_ids:
        print("\nPruned node details (no MPI descendants):")
        for node_id in sorted(pruned_node_ids):
            node = psg.getNode(node_id)
            if node:
                print(f"  [{node_id}] {node.getName()} (type: {node.getType()})")
    
    print("\n" + "="*70)
    print("Key Observations:")
    print("="*70)
    print("1. CST preserves the hierarchical structure (main -> comp_loop -> inner_loop)")
    print("2. Branches without MPI descendants are pruned (init_loop, final_loop)")
    print("3. MPI calls (MPI_Send, MPI_Recv) become leaf nodes in the CST")
    print("4. Non-MPI calls in pruned branches (initialize_data, cleanup_data) are removed")
    print("="*70)


def demonstrate_complex_structure():
    """Demonstrate CST with multiple branches and nested MPI calls."""
    print("\n\n")
    print("="*70)
    print("Demonstrating Complex Structure with Multiple Branches")
    print("="*70)
    
    psg = ProgramStructureGraph()
    
    # Create a more complex structure
    main = psg.addFunctionNode(1, "main", "main.c", 10)
    
    # Branch 1: MPI initialization
    init_func = psg.addCallSite(2, caller_id=1, callee_name="MPI_Init")
    
    # Branch 2: Loop with collective
    loop1 = psg.addLoopNode(3, "data_exchange_loop", parent_id=1, loop_type="for")
    bcast = psg.addCallSite(4, caller_id=3, callee_name="MPI_Bcast")
    
    # Branch 3: Nested loops with point-to-point
    loop2 = psg.addLoopNode(5, "compute_loop", parent_id=1, loop_type="for")
    loop3 = psg.addLoopNode(6, "inner_compute", parent_id=5, loop_type="for")
    send = psg.addCallSite(7, caller_id=6, callee_name="MPI_Send")
    recv = psg.addCallSite(8, caller_id=6, callee_name="MPI_Recv")
    
    # Branch 4: No MPI (will be pruned)
    loop4 = psg.addLoopNode(9, "local_compute", parent_id=1, loop_type="for")
    compute = psg.addCallSite(10, caller_id=9, callee_name="local_work")
    
    # Branch 5: MPI finalization
    finalize = psg.addCallSite(11, caller_id=1, callee_name="MPI_Finalize")
    
    # Define MPI nodes
    mpi_node_ids = {2, 4, 7, 8, 11}
    comm_type_map = {
        2: CommType.BARRIER,  # MPI_Init is like a barrier
        4: CommType.BROADCAST,
        7: CommType.POINT_TO_POINT,
        8: CommType.POINT_TO_POINT,
        11: CommType.BARRIER  # MPI_Finalize is like a barrier
    }
    
    # Build CST
    cst = CommStructureTree()
    cst.buildFromPSG(psg, mpi_node_ids, comm_type_map)
    
    # Display results
    print_psg_structure(psg)
    print_cst_structure(cst)
    
    # Analyze MPI distribution
    print("\n=== MPI Operation Distribution ===")
    mpi_nodes = cst.getMPINodes()
    for node in mpi_nodes:
        comm_type = node.getAttribute("comm_type")
        print(f"  {node.getName()}: {comm_type}")
    
    print("\n" + "="*70)


if __name__ == "__main__":
    # Run demonstrations
    demonstrate_pruning()
    demonstrate_complex_structure()
    
    print("\n" + "="*70)
    print("Example completed successfully!")
    print("="*70)
