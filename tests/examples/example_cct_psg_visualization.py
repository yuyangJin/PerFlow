"""
Example: Visualizing Calling Context Tree (CCT) and Program Structure Graph (PSG)

This example demonstrates how to:
1. Build a Calling Context Tree from profiling data
2. Build a Program Structure Graph
3. Visualize both structures in various formats (text, DOT/GraphViz)
"""

from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData, ProfileInfo
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData
from perflow.perf_data_struct.dynamic.profile.calling_context_tree import CallingContextTree
from perflow.perf_data_struct.static.program_structure_graph import ProgramStructureGraph, NodeType


def create_sample_profile_data():
    """Create sample profiling data with call stacks for demonstration."""
    profile = PerfData()
    
    # Set profile metadata
    profile_info = ProfileInfo(
        pid=1000,
        application_name="example_app"
    )
    profile.setProfileInfo(profile_info)
    
    # Create samples with different call stacks
    # Simulate a program with main -> compute -> matrix_multiply call chain
    
    # Samples from main -> compute -> matrix_multiply
    for i in range(50):
        sample = SampleData(
            timestamp=float(i) * 0.001,
            function_name="matrix_multiply"
        )
        sample.setMetric("cycles", 1000.0)
        sample.setMetric("instructions", 500.0)
        sample.setCallStack(["main", "compute", "matrix_multiply"])
        profile.addSample(sample)
    
    # Samples from main -> compute -> vector_add
    for i in range(30):
        sample = SampleData(
            timestamp=float(i) * 0.001,
            function_name="vector_add"
        )
        sample.setMetric("cycles", 500.0)
        sample.setMetric("instructions", 300.0)
        sample.setCallStack(["main", "compute", "vector_add"])
        profile.addSample(sample)
    
    # Samples from main -> io_read
    for i in range(20):
        sample = SampleData(
            timestamp=float(i) * 0.001,
            function_name="io_read"
        )
        sample.setMetric("cycles", 2000.0)
        sample.setMetric("instructions", 200.0)
        sample.setCallStack(["main", "io_read"])
        profile.addSample(sample)
    
    # Samples from main -> compute -> helper
    for i in range(15):
        sample = SampleData(
            timestamp=float(i) * 0.001,
            function_name="helper"
        )
        sample.setMetric("cycles", 300.0)
        sample.setMetric("instructions", 150.0)
        sample.setCallStack(["main", "compute", "helper"])
        profile.addSample(sample)
    
    return profile


def visualize_cct_text(cct: CallingContextTree):
    """
    Visualize the Calling Context Tree in text format.
    
    Args:
        cct: CallingContextTree to visualize
    """
    print("=" * 80)
    print("CALLING CONTEXT TREE (CCT) - TEXT VISUALIZATION")
    print("=" * 80)
    print()
    
    def print_node(node_id, indent=0):
        """Recursively print nodes with indentation."""
        node = cct.getNode(node_id)
        if not node:
            return
        
        # Get metrics
        sample_count = cct.getNodeSampleCount(node_id)
        metrics = cct.getNodeMetrics(node_id)
        inclusive_metrics = cct.getInclusiveMetrics(node_id)
        
        # Print node info
        indent_str = "  " * indent
        func_name = node.getName()
        
        print(f"{indent_str}├─ {func_name}")
        print(f"{indent_str}│  Samples: {sample_count}")
        
        if metrics:
            print(f"{indent_str}│  Exclusive:")
            for metric, value in sorted(metrics.items()):
                print(f"{indent_str}│    {metric}: {value:,.0f}")
        
        if inclusive_metrics:
            print(f"{indent_str}│  Inclusive:")
            for metric, value in sorted(inclusive_metrics.items()):
                print(f"{indent_str}│    {metric}: {value:,.0f}")
        
        print(f"{indent_str}│")
        
        # Print children
        for child_id in cct.getChildren(node_id):
            print_node(child_id, indent + 1)
    
    # Start from root
    root = cct.getRoot()
    if root:
        print_node(root.getId())
    
    # Print hot paths
    print()
    print("-" * 80)
    print("TOP 5 HOTTEST CALLING PATHS (by cycles)")
    print("-" * 80)
    
    hot_paths = cct.getHotPaths("cycles", top_n=5)
    for i, (context, cycles) in enumerate(hot_paths, 1):
        path_str = " -> ".join(context)
        print(f"{i}. {path_str}")
        print(f"   Cycles: {cycles:,.0f}")
        print()


def visualize_cct_dot(cct: CallingContextTree, output_file: str = "cct_graph.dot"):
    """
    Generate DOT format visualization for Calling Context Tree.
    
    Args:
        cct: CallingContextTree to visualize
        output_file: Output file path for DOT format
    """
    print(f"Generating DOT file: {output_file}")
    
    with open(output_file, 'w') as f:
        f.write("digraph CallingContextTree {\n")
        f.write("  rankdir=TB;\n")
        f.write("  node [shape=box, style=rounded];\n")
        f.write("  \n")
        
        # Write nodes
        for node_id in cct.m_node_metrics.keys():
            node = cct.getNode(node_id)
            if node:
                func_name = node.getName()
                sample_count = cct.getNodeSampleCount(node_id)
                metrics = cct.getNodeMetrics(node_id)
                
                cycles = metrics.get("cycles", 0)
                
                # Create label with metrics
                label = f"{func_name}\\n"
                label += f"Samples: {sample_count}\\n"
                if cycles > 0:
                    label += f"Cycles: {cycles:,.0f}"
                
                # Color nodes by heat (sample count)
                if sample_count > 40:
                    color = "red"
                elif sample_count > 20:
                    color = "orange"
                elif sample_count > 10:
                    color = "yellow"
                else:
                    color = "lightblue"
                
                f.write(f'  node{node_id} [label="{label}", fillcolor={color}, style="rounded,filled"];\n')
        
        f.write("  \n")
        
        # Write edges
        for parent_id, children in cct.m_edges.items():
            for child_id in children:
                f.write(f"  node{parent_id} -> node{child_id};\n")
        
        f.write("}\n")
    
    print(f"DOT file generated. To create an image, run:")
    print(f"  dot -Tpng {output_file} -o cct_graph.png")
    print(f"  # Or use: dot -Tsvg {output_file} -o cct_graph.svg")


def create_sample_psg():
    """Create a sample Program Structure Graph for demonstration."""
    psg = ProgramStructureGraph()
    
    # Add main function
    main_func = psg.addFunctionNode(1, "main", "main.c", line_number=10)
    psg.setEntryPoint(1)
    
    # Add compute function
    compute_func = psg.addFunctionNode(2, "compute", "compute.c", line_number=5)
    
    # Add other functions
    matrix_func = psg.addFunctionNode(3, "matrix_multiply", "matrix.c", line_number=15)
    vector_func = psg.addFunctionNode(4, "vector_add", "vector.c", line_number=8)
    io_func = psg.addFunctionNode(5, "io_read", "io.c", line_number=20)
    helper_func = psg.addFunctionNode(6, "helper", "utils.c", line_number=12)
    
    # Add loops in matrix_multiply
    psg.addLoopNode(10, "outer_loop", parent_id=3, loop_type="for")
    psg.addLoopNode(11, "inner_loop", parent_id=10, loop_type="for")
    
    # Add basic blocks
    psg.addBasicBlock(20, "bb_init", parent_id=2)
    psg.addBasicBlock(21, "bb_compute", parent_id=2)
    psg.addBasicBlock(22, "bb_cleanup", parent_id=2)
    
    # Add call sites
    psg.addCallSite(30, caller_id=1, callee_name="compute", call_type="direct")
    psg.addCallSite(31, caller_id=1, callee_name="io_read", call_type="direct")
    psg.addCallSite(32, caller_id=2, callee_name="matrix_multiply", call_type="direct")
    psg.addCallSite(33, caller_id=2, callee_name="vector_add", call_type="direct")
    psg.addCallSite(34, caller_id=2, callee_name="helper", call_type="direct")
    
    return psg


def visualize_psg_text(psg: ProgramStructureGraph):
    """
    Visualize the Program Structure Graph in text format.
    
    Args:
        psg: ProgramStructureGraph to visualize
    """
    print("=" * 80)
    print("PROGRAM STRUCTURE GRAPH (PSG) - TEXT VISUALIZATION")
    print("=" * 80)
    print()
    
    # Show functions
    print("FUNCTIONS:")
    print("-" * 80)
    functions = psg.getFunctions()
    for func in functions:
        func_id = func.getId()
        func_name = func.getName()
        source_file = func.getAttribute("source_file") or "unknown"
        line_number = func.getAttribute("line_number") or "?"
        
        is_entry = func_id in psg.getEntryPoints()
        entry_mark = " [ENTRY]" if is_entry else ""
        
        print(f"  {func_name} (ID: {func_id}){entry_mark}")
        print(f"    Location: {source_file}:{line_number}")
        
        # Show successors (functions it calls)
        successors = psg.getSuccessors(func_id)
        if successors:
            print(f"    Contains: {len(successors)} child elements")
        print()
    
    # Show loops
    print()
    print("LOOPS:")
    print("-" * 80)
    loops = psg.getLoops()
    for loop in loops:
        loop_id = loop.getId()
        loop_name = loop.getName()
        loop_type = loop.getAttribute("loop_type") or "unknown"
        depth = psg.getLoopNestingDepth(loop_id)
        
        print(f"  {loop_name} (ID: {loop_id})")
        print(f"    Type: {loop_type}")
        print(f"    Nesting depth: {depth}")
        print()
    
    # Show call graph
    print()
    print("CALL GRAPH:")
    print("-" * 80)
    call_graph = psg.getCallGraph()
    for func in functions:
        func_id = func.getId()
        callees = call_graph.getSuccessors(func_id)
        if callees:
            print(f"  {func.getName()} calls:")
            for callee_id in callees:
                callee = call_graph.getNode(callee_id)
                if callee:
                    print(f"    -> {callee.getName()}")
    
    # Statistics
    print()
    print("STATISTICS:")
    print("-" * 80)
    print(f"  Total functions: {psg.getFunctionCount()}")
    print(f"  Total loops: {psg.getLoopCount()}")
    print(f"  Total nodes: {psg.getNodeCount()}")
    print(f"  Total edges: {psg.getEdgeCount()}")


def visualize_psg_dot(psg: ProgramStructureGraph, output_file: str = "psg_graph.dot"):
    """
    Generate DOT format visualization for Program Structure Graph.
    
    Args:
        psg: ProgramStructureGraph to visualize
        output_file: Output file path for DOT format
    """
    print(f"Generating DOT file: {output_file}")
    
    with open(output_file, 'w') as f:
        f.write("digraph ProgramStructureGraph {\n")
        f.write("  rankdir=TB;\n")
        f.write("  compound=true;\n")
        f.write("  \n")
        
        # Define styles for different node types
        f.write("  // Node styles\n")
        f.write('  node [shape=box];\n')
        f.write("  \n")
        
        # Create subgraph for call graph
        f.write("  subgraph cluster_call_graph {\n")
        f.write('    label="Call Graph";\n')
        f.write('    style=filled;\n')
        f.write('    fillcolor=lightgrey;\n')
        f.write("    \n")
        
        # Write function nodes
        functions = psg.getFunctions()
        for func in functions:
            func_id = func.getId()
            func_name = func.getName()
            source_file = func.getAttribute("source_file") or ""
            line_number = func.getAttribute("line_number") or ""
            
            is_entry = func_id in psg.getEntryPoints()
            
            label = func_name
            if source_file:
                label += f"\\n{source_file}"
                if line_number:
                    label += f":{line_number}"
            
            shape = "doubleoctagon" if is_entry else "box"
            color = "green" if is_entry else "lightblue"
            
            f.write(f'    func{func_id} [label="{label}", shape={shape}, fillcolor={color}, style=filled];\n')
        
        f.write("    \n")
        
        # Write call edges
        call_graph = psg.getCallGraph()
        f.write("    // Call edges\n")
        for func in functions:
            func_id = func.getId()
            for callee_id in call_graph.getSuccessors(func_id):
                f.write(f"    func{func_id} -> func{callee_id};\n")
        
        f.write("  }\n")
        f.write("  \n")
        
        # Add loop information as separate nodes
        loops = psg.getLoops()
        if loops:
            f.write("  subgraph cluster_loops {\n")
            f.write('    label="Loops";\n')
            f.write('    style=filled;\n')
            f.write('    fillcolor=lightyellow;\n')
            f.write("    \n")
            
            for loop in loops:
                loop_id = loop.getId()
                loop_name = loop.getName()
                loop_type = loop.getAttribute("loop_type") or "unknown"
                depth = psg.getLoopNestingDepth(loop_id)
                
                label = f"{loop_name}\\nType: {loop_type}\\nDepth: {depth}"
                f.write(f'    loop{loop_id} [label="{label}", shape=ellipse, fillcolor=yellow, style=filled];\n')
            
            f.write("  }\n")
        
        f.write("}\n")
    
    print(f"DOT file generated. To create an image, run:")
    print(f"  dot -Tpng {output_file} -o psg_graph.png")
    print(f"  # Or use: dot -Tsvg {output_file} -o psg_graph.svg")


def main():
    """
    Main function demonstrating CCT and PSG visualization.
    """
    print("\n" + "=" * 80)
    print("PERFLOW VISUALIZATION EXAMPLE: CCT AND PSG")
    print("=" * 80)
    print()
    print("This example demonstrates visualization of:")
    print("1. Calling Context Tree (CCT) - built from profiling data")
    print("2. Program Structure Graph (PSG) - static program structure")
    print()
    
    # Part 1: Calling Context Tree
    print("\n" + "=" * 80)
    print("PART 1: CALLING CONTEXT TREE (CCT)")
    print("=" * 80)
    print()
    
    print("Step 1: Creating sample profile data...")
    profile = create_sample_profile_data()
    print(f"  Created profile with {profile.getSampleCount()} samples")
    print()
    
    print("Step 2: Building Calling Context Tree...")
    cct = CallingContextTree()
    cct.buildFromProfile(profile)
    print(f"  Built CCT with {cct.getNodeCount()} nodes")
    print(f"  Maximum context depth: {cct.getContextDepth()}")
    print()
    
    print("Step 3: Text visualization of CCT:")
    visualize_cct_text(cct)
    print()
    
    print("Step 4: Generating DOT format for CCT...")
    visualize_cct_dot(cct, "cct_example.dot")
    print()
    
    # Part 2: Program Structure Graph
    print("\n" + "=" * 80)
    print("PART 2: PROGRAM STRUCTURE GRAPH (PSG)")
    print("=" * 80)
    print()
    
    print("Step 1: Creating sample Program Structure Graph...")
    psg = create_sample_psg()
    print(f"  Created PSG with {psg.getNodeCount()} nodes")
    print(f"  Functions: {psg.getFunctionCount()}")
    print(f"  Loops: {psg.getLoopCount()}")
    print()
    
    print("Step 2: Text visualization of PSG:")
    visualize_psg_text(psg)
    print()
    
    print("Step 3: Generating DOT format for PSG...")
    visualize_psg_dot(psg, "psg_example.dot")
    print()
    
    # Summary
    print("\n" + "=" * 80)
    print("VISUALIZATION SUMMARY")
    print("=" * 80)
    print()
    print("Generated files:")
    print("  - cct_example.dot: Calling Context Tree in DOT format")
    print("  - psg_example.dot: Program Structure Graph in DOT format")
    print()
    print("To generate images from DOT files, install Graphviz and run:")
    print("  dot -Tpng cct_example.dot -o cct_example.png")
    print("  dot -Tpng psg_example.dot -o psg_example.png")
    print()
    print("Example complete!")


if __name__ == "__main__":
    main()
