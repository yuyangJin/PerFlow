#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Example: Using PerFlow Python bindings for performance analysis

This example demonstrates:
1. Building a performance tree from sample data
2. Analyzing hotspots and workload balance
3. Tree traversal and filtering
4. Programmatic analysis patterns

To run this example, first build the Python bindings:
    cd PerFlow
    mkdir build && cd build
    cmake .. -DPERFLOW_BUILD_PYTHON=ON
    make -j4
    
Then run:
    cd python
    python3 -c "import perflow; print(perflow.version())"  # Test import
    python3 ../examples/python_analysis_example.py
"""

import sys
import os

# Add the python directory to path if running from source
script_dir = os.path.dirname(os.path.abspath(__file__))
python_dir = os.path.join(os.path.dirname(script_dir), 'python')
if os.path.exists(python_dir):
    sys.path.insert(0, python_dir)

import perflow


def create_sample_tree():
    """Create a sample performance tree for demonstration."""
    # Create a tree builder with context-free mode and both sample count modes
    builder = perflow.TreeBuilder(
        perflow.TreeBuildMode.ContextFree,
        perflow.SampleCountMode.Both
    )
    
    tree = builder.tree
    tree.process_count = 4
    
    # Simulate call stacks from a parallel application
    # main -> compute -> kernel (hot path)
    main_frame = perflow.ResolvedFrame()
    main_frame.function_name = "main"
    main_frame.library_name = "app"
    
    compute_frame = perflow.ResolvedFrame()
    compute_frame.function_name = "compute"
    compute_frame.library_name = "app"
    
    kernel_frame = perflow.ResolvedFrame()
    kernel_frame.function_name = "kernel"
    kernel_frame.library_name = "libmath.so"
    
    # main -> io_write (cold path)
    io_frame = perflow.ResolvedFrame()
    io_frame.function_name = "io_write"
    io_frame.library_name = "libc.so.6"
    
    # main -> mpi_barrier
    mpi_frame = perflow.ResolvedFrame()
    mpi_frame.function_name = "MPI_Barrier"
    mpi_frame.library_name = "libmpi.so"
    
    # Insert call stacks for each process with different load distributions
    # Process 0: Heavy compute
    for _ in range(100):
        tree.insert_call_stack([main_frame, compute_frame, kernel_frame], 0, 10, 100.0)
    tree.insert_call_stack([main_frame, io_frame], 0, 5, 50.0)
    tree.insert_call_stack([main_frame, mpi_frame], 0, 20, 200.0)
    
    # Process 1: Balanced
    for _ in range(80):
        tree.insert_call_stack([main_frame, compute_frame, kernel_frame], 1, 10, 100.0)
    tree.insert_call_stack([main_frame, io_frame], 1, 10, 100.0)
    tree.insert_call_stack([main_frame, mpi_frame], 1, 30, 300.0)
    
    # Process 2: More I/O
    for _ in range(60):
        tree.insert_call_stack([main_frame, compute_frame, kernel_frame], 2, 10, 100.0)
    tree.insert_call_stack([main_frame, io_frame], 2, 30, 300.0)
    tree.insert_call_stack([main_frame, mpi_frame], 2, 40, 400.0)
    
    # Process 3: Light load
    for _ in range(40):
        tree.insert_call_stack([main_frame, compute_frame, kernel_frame], 3, 10, 100.0)
    tree.insert_call_stack([main_frame, io_frame], 3, 5, 50.0)
    tree.insert_call_stack([main_frame, mpi_frame], 3, 10, 100.0)
    
    return tree


def analyze_hotspots(tree):
    """Demonstrate hotspot analysis."""
    print("\n" + "=" * 60)
    print("HOTSPOT ANALYSIS")
    print("=" * 60)
    
    # Self-time (exclusive) hotspots - default
    print("\nTop 5 Hotspots by Self Time (Exclusive):")
    print("-" * 50)
    hotspots = perflow.HotspotAnalyzer.find_hotspots(tree, top_n=5)
    for i, h in enumerate(hotspots, 1):
        print(f"  {i}. {h.function_name:<20} {h.self_samples:>8} samples ({h.self_percentage:.1f}%)")
    
    # Total-time (inclusive) hotspots
    print("\nTop 5 Hotspots by Total Time (Inclusive):")
    print("-" * 50)
    total_hotspots = perflow.HotspotAnalyzer.find_total_hotspots(tree, top_n=5)
    for i, h in enumerate(total_hotspots, 1):
        print(f"  {i}. {h.function_name:<20} {h.total_samples:>8} samples ({h.percentage:.1f}%)")


def analyze_balance(tree):
    """Demonstrate balance analysis."""
    print("\n" + "=" * 60)
    print("WORKLOAD BALANCE ANALYSIS")
    print("=" * 60)
    
    balance = perflow.BalanceAnalyzer.analyze(tree)
    
    print(f"\n  Process count:        {len(balance.process_samples)}")
    print(f"  Mean samples:         {balance.mean_samples:.1f}")
    print(f"  Std deviation:        {balance.std_dev_samples:.1f}")
    print(f"  Min samples:          {balance.min_samples:.0f} (process {balance.least_loaded_process})")
    print(f"  Max samples:          {balance.max_samples:.0f} (process {balance.most_loaded_process})")
    print(f"  Imbalance factor:     {balance.imbalance_factor:.2f}")
    
    print("\n  Per-process distribution:")
    for i, samples in enumerate(balance.process_samples):
        bar_len = int(samples / max(balance.process_samples) * 30)
        bar = "█" * bar_len
        print(f"    Process {i}: {bar:<30} {samples:.0f}")


def demonstrate_tree_traversal(tree):
    """Demonstrate tree traversal APIs."""
    print("\n" + "=" * 60)
    print("TREE TRAVERSAL EXAMPLES")
    print("=" * 60)
    
    # Pre-order traversal
    print("\nPre-order traversal (parent before children):")
    visited = []
    def collect_names(node):
        visited.append(node.function_name)
        return True
    tree.traverse_preorder(collect_names)
    print(f"  {' -> '.join(visited)}")
    
    # Level-order traversal
    print("\nLevel-order traversal (breadth-first):")
    levels = {}
    def collect_by_level(node):
        d = node.depth
        if d not in levels:
            levels[d] = []
        levels[d].append(node.function_name)
        return True
    tree.traverse_levelorder(collect_by_level)
    for level, names in sorted(levels.items()):
        print(f"  Level {level}: {', '.join(names)}")
    
    # Early termination example
    print("\nEarly termination (stop after finding 'compute'):")
    def stop_at_compute(node):
        print(f"  Visiting: {node.function_name}")
        return node.function_name != "compute"
    tree.traverse_preorder(stop_at_compute)


def demonstrate_filtering(tree):
    """Demonstrate node filtering APIs."""
    print("\n" + "=" * 60)
    print("NODE FILTERING EXAMPLES")
    print("=" * 60)
    
    # Find by name
    print("\nFind nodes by function name 'kernel':")
    kernel_nodes = tree.find_nodes_by_name("kernel")
    for node in kernel_nodes:
        print(f"  Found: {node.function_name} with {node.total_samples} samples")
    
    # Find by library
    print("\nFind nodes from library 'libmath.so':")
    math_nodes = tree.find_nodes_by_library("libmath.so")
    for node in math_nodes:
        print(f"  Found: {node.function_name} in {node.library_name}")
    
    # Filter by samples
    print("\nFilter nodes with >= 500 total samples:")
    hot_nodes = tree.filter_by_samples(500)
    for node in hot_nodes:
        print(f"  {node.function_name}: {node.total_samples} samples")
    
    # Leaf nodes only
    print("\nLeaf nodes (actual execution points):")
    leaves = tree.leaf_nodes
    for leaf in leaves:
        print(f"  {leaf.function_name}: {leaf.self_samples} self samples")
    
    # Nodes at specific depth
    print("\nNodes at depth 2:")
    depth2 = tree.nodes_at_depth(2)
    for node in depth2:
        print(f"  {node.function_name} (depth={node.depth})")


def demonstrate_node_navigation(tree):
    """Demonstrate node navigation APIs."""
    print("\n" + "=" * 60)
    print("NODE NAVIGATION EXAMPLES")
    print("=" * 60)
    
    # Start from root
    root = tree.root
    print(f"\nRoot node: {root.function_name}")
    print(f"  Children: {[c.function_name for c in root.children]}")
    
    # Navigate to a specific node
    main_node = root.find_child_by_name("main")
    if main_node:
        print(f"\n'main' node:")
        print(f"  Total samples: {main_node.total_samples}")
        print(f"  Children: {[c.function_name for c in main_node.children]}")
        
        # Navigate further
        compute_node = main_node.find_child_by_name("compute")
        if compute_node:
            print(f"\n'compute' node:")
            print(f"  Path from root: {' -> '.join(compute_node.get_path())}")
            print(f"  Is leaf: {compute_node.is_leaf}")
            print(f"  Siblings: {[s.function_name for s in compute_node.siblings()]}")


def demonstrate_programmatic_analysis(tree):
    """Demonstrate custom programmatic analysis patterns."""
    print("\n" + "=" * 60)
    print("PROGRAMMATIC ANALYSIS PATTERNS")
    print("=" * 60)
    
    # Find the critical path (path with most samples)
    print("\nFinding critical path:")
    
    def find_critical_path(node):
        path = [node.function_name]
        current = node
        while current.children:
            # Find child with most samples
            max_child = max(current.children, key=lambda c: c.total_samples)
            path.append(max_child.function_name)
            current = max_child
        return path
    
    critical = find_critical_path(tree.root)
    print(f"  {' -> '.join(critical)}")
    
    # Calculate library breakdown
    print("\nLibrary-level breakdown:")
    lib_samples = {}
    
    def accumulate_library(node):
        lib = node.library_name
        if lib not in lib_samples:
            lib_samples[lib] = 0
        lib_samples[lib] += node.self_samples
        return True
    
    tree.traverse_preorder(accumulate_library)
    total = sum(lib_samples.values())
    for lib, samples in sorted(lib_samples.items(), key=lambda x: -x[1]):
        if samples > 0:
            pct = samples / total * 100 if total > 0 else 0
            print(f"  {lib:<30} {samples:>8} samples ({pct:.1f}%)")
    
    # Calculate per-process time in specific function
    print("\nPer-process time in 'kernel' function:")
    kernel_nodes = tree.find_nodes_by_name("kernel")
    if kernel_nodes:
        kernel = kernel_nodes[0]
        for pid in range(tree.process_count):
            samples = kernel.sampling_count(pid)
            time_us = kernel.execution_time(pid)
            print(f"  Process {pid}: {samples} samples, {time_us:.0f} µs")


def main():
    print("PerFlow Python Bindings - Example Script")
    print("=" * 60)
    print(f"Version: {perflow.version()}")
    
    # Create sample data
    print("\nCreating sample performance tree...")
    tree = create_sample_tree()
    
    print(f"\nTree statistics:")
    print(f"  Total samples:  {tree.total_samples}")
    print(f"  Node count:     {tree.node_count}")
    print(f"  Max depth:      {tree.max_depth}")
    print(f"  Process count:  {tree.process_count}")
    
    # Run demonstrations
    analyze_hotspots(tree)
    analyze_balance(tree)
    demonstrate_tree_traversal(tree)
    demonstrate_filtering(tree)
    demonstrate_node_navigation(tree)
    demonstrate_programmatic_analysis(tree)
    
    # Use convenience functions
    print("\n" + "=" * 60)
    print("USING CONVENIENCE FUNCTIONS")
    print("=" * 60)
    perflow.print_hotspots(tree, top_n=5)
    perflow.print_balance(tree)
    
    print("\n" + "=" * 60)
    print("Example completed successfully!")
    print("=" * 60)


if __name__ == "__main__":
    main()
