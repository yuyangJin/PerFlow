#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Simple Example: Loading sample data files and performing hotspot analysis

This example demonstrates:
1. Loading sample data files (.pflw) from a directory
2. Optionally loading library map files (.libmap) for symbol resolution
3. Performing hotspot analysis to find performance bottlenecks

Usage:
    python3 simple_file_analysis.py <data_directory> [num_processes]

Example:
    python3 simple_file_analysis.py ./sample_output 4

File naming convention:
    - Sample files: perflow_mpi_rank_<N>.pflw
    - Library maps: perflow_mpi_rank_<N>.libmap
"""

import sys
import os
import glob

# Add the python directory to path if running from source
script_dir = os.path.dirname(os.path.abspath(__file__))
python_dir = os.path.join(os.path.dirname(script_dir), 'python')
if os.path.exists(python_dir):
    sys.path.insert(0, python_dir)

import perflow


def find_sample_files(data_dir, num_processes=None):
    """
    Find sample data files in the specified directory.
    
    Args:
        data_dir: Directory containing sample files
        num_processes: Optional number of processes (auto-detect if None)
    
    Returns:
        Tuple of (sample_files, libmap_files) where each is a list of (path, process_id) tuples
    """
    sample_files = []
    libmap_files = []
    
    if num_processes is not None:
        # Use specified number of processes
        for rank in range(num_processes):
            sample_path = os.path.join(data_dir, f"perflow_mpi_rank_{rank}.pflw")
            libmap_path = os.path.join(data_dir, f"perflow_mpi_rank_{rank}.libmap")
            
            if os.path.exists(sample_path):
                sample_files.append((sample_path, rank))
            if os.path.exists(libmap_path):
                libmap_files.append((libmap_path, rank))
    else:
        # Auto-detect by scanning directory
        pattern = os.path.join(data_dir, "perflow_mpi_rank_*.pflw")
        for sample_path in sorted(glob.glob(pattern)):
            # Extract rank from filename
            basename = os.path.basename(sample_path)
            try:
                rank = int(basename.replace("perflow_mpi_rank_", "").replace(".pflw", ""))
                sample_files.append((sample_path, rank))
                
                # Check for corresponding libmap
                libmap_path = sample_path.replace(".pflw", ".libmap")
                if os.path.exists(libmap_path):
                    libmap_files.append((libmap_path, rank))
            except ValueError:
                continue
    
    return sample_files, libmap_files


def analyze_from_files(data_dir, num_processes=None, top_n=10):
    """
    Load sample data from files and perform hotspot analysis.
    
    Args:
        data_dir: Directory containing sample data files
        num_processes: Number of MPI processes (auto-detect if None)
        top_n: Number of top hotspots to display
    
    Returns:
        The performance tree object
    """
    print(f"PerFlow File-based Hotspot Analysis")
    print("=" * 50)
    print(f"Version: {perflow.version()}")
    print(f"Data directory: {data_dir}")
    print()
    
    # Find sample files
    sample_files, libmap_files = find_sample_files(data_dir, num_processes)
    
    if not sample_files:
        print(f"Error: No sample files found in {data_dir}")
        print(f"Expected files like: perflow_mpi_rank_0.pflw")
        return None
    
    print(f"Found {len(sample_files)} sample file(s)")
    print(f"Found {len(libmap_files)} library map file(s)")
    print()
    
    # Create tree builder
    builder = perflow.TreeBuilder(
        perflow.TreeBuildMode.ContextFree,
        perflow.SampleCountMode.Both
    )
    
    # Load library maps if available (for symbol resolution)
    if libmap_files:
        print("Loading library maps...")
        loaded = builder.load_library_maps(libmap_files)
        print(f"  Loaded {loaded} library map(s)")
    
    # Build tree from sample files
    print("Loading sample data...")
    for path, rank in sample_files:
        print(f"  Loading rank {rank}: {os.path.basename(path)}")
    
    success_count = builder.build_from_files(sample_files)
    print(f"  Successfully loaded {success_count} file(s)")
    print()
    
    # Get the tree
    tree = builder.tree
    
    if tree.total_samples == 0:
        print("Warning: No samples loaded. Check file formats.")
        return tree
    
    # Display tree statistics
    print("Performance Tree Statistics:")
    print("-" * 40)
    print(f"  Total samples:    {tree.total_samples}")
    print(f"  Node count:       {tree.node_count}")
    print(f"  Max depth:        {tree.max_depth}")
    print(f"  Process count:    {tree.process_count}")
    print()
    
    # Perform hotspot analysis (exclusive/self time)
    print(f"Top {top_n} Hotspots (Exclusive/Self Time):")
    print("-" * 70)
    print(f"{'Rank':<5} {'Function':<35} {'Samples':<12} {'%':<8}")
    print("-" * 70)
    
    hotspots = perflow.HotspotAnalyzer.find_hotspots(tree, top_n)
    
    # Debug: Check if hotspots have valid function names
    if hotspots and not hotspots[0].function_name:
        print("DEBUG: Function names appear to be empty.")
        print("DEBUG: Checking tree nodes for function name data...")
        # Check first few nodes
        all_nodes = tree.all_nodes[:5] if hasattr(tree, 'all_nodes') else []
        for node in all_nodes:
            print(f"  Node: function='{node.function_name}', library='{node.library_name}', samples={node.total_samples}")
    
    for i, h in enumerate(hotspots, 1):
        func = h.function_name[:33] if len(h.function_name) > 33 else h.function_name
        print(f"{i:<5} {func:<35} {h.self_samples:<12} {h.self_percentage:.2f}%")
    print("-" * 70)
    print()
    
    # Perform hotspot analysis (inclusive/total time)
    print(f"Top {top_n} Hotspots (Inclusive/Total Time):")
    print("-" * 70)
    print(f"{'Rank':<5} {'Function':<35} {'Samples':<12} {'%':<8}")
    print("-" * 70)
    
    total_hotspots = perflow.HotspotAnalyzer.find_total_hotspots(tree, top_n)
    for i, h in enumerate(total_hotspots, 1):
        func = h.function_name[:33] if len(h.function_name) > 33 else h.function_name
        print(f"{i:<5} {func:<35} {h.total_samples:<12} {h.percentage:.2f}%")
    print("-" * 70)
    print()
    
    # Workload balance analysis
    if tree.process_count > 1:
        perflow.print_balance(tree)
    
    return tree


def main():
    # Parse command line arguments
    if len(sys.argv) < 2:
        print("Usage: python3 simple_file_analysis.py <data_directory> [num_processes]")
        print()
        print("Arguments:")
        print("  data_directory  Directory containing perflow_mpi_rank_*.pflw files")
        print("  num_processes   Optional: Number of MPI processes (auto-detect if omitted)")
        print()
        print("Example:")
        print("  python3 simple_file_analysis.py ./sample_output")
        print("  python3 simple_file_analysis.py ./sample_output 4")
        return 1
    
    data_dir = sys.argv[1]
    num_processes = int(sys.argv[2]) if len(sys.argv) > 2 else None
    
    if not os.path.isdir(data_dir):
        print(f"Error: Directory not found: {data_dir}")
        return 1
    
    tree = analyze_from_files(data_dir, num_processes)
    
    if tree and tree.total_samples > 0:
        print("Analysis completed successfully!")
        return 0
    else:
        return 1


if __name__ == "__main__":
    sys.exit(main())
