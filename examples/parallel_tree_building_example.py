#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Example: Parallel Tree Building with Different Concurrency Models

This example demonstrates how to use PerFlow's parallel tree building
capabilities with different concurrency models in Python.

Three concurrency models are available:
1. Serial - Single-threaded, no parallelism (default)
2. FineGrainedLock - Per-node locks for parallel subtree insertion
3. ThreadLocalMerge - Thread-local trees merged after construction
4. LockFree - Atomic operations for counters, locks for structural changes

Usage:
    python3 parallel_tree_building_example.py <data_directory> [num_processes]

Example:
    python3 parallel_tree_building_example.py ./sample_output 4
"""

import sys
import os
import glob
import time

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
        for rank in range(num_processes):
            sample_path = os.path.join(data_dir, f"perflow_mpi_rank_{rank}.pflw")
            libmap_path = os.path.join(data_dir, f"perflow_mpi_rank_{rank}.libmap")
            
            if os.path.exists(sample_path):
                sample_files.append((sample_path, rank))
            if os.path.exists(libmap_path):
                libmap_files.append((libmap_path, rank))
    else:
        pattern = os.path.join(data_dir, "perflow_mpi_rank_*.pflw")
        for sample_path in sorted(glob.glob(pattern)):
            basename = os.path.basename(sample_path)
            try:
                rank = int(basename.replace("perflow_mpi_rank_", "").replace(".pflw", ""))
                sample_files.append((sample_path, rank))
                
                libmap_path = sample_path.replace(".pflw", ".libmap")
                if os.path.exists(libmap_path):
                    libmap_files.append((libmap_path, rank))
            except ValueError:
                continue
    
    return sample_files, libmap_files


def example_serial_tree_building(sample_files, libmap_files):
    """
    Example 1: Serial Tree Building (Default)
    
    Uses the default serial mode with no parallelism.
    """
    print("\n" + "=" * 70)
    print("Example 1: Serial Tree Building")
    print("=" * 70)
    
    # Create TreeBuilder with Serial concurrency model
    builder = perflow.TreeBuilder(
        perflow.TreeBuildMode.ContextFree,
        perflow.SampleCountMode.Both,
        perflow.ConcurrencyModel.Serial
    )
    
    # Load library maps if available
    if libmap_files:
        builder.load_library_maps(libmap_files)
    
    # Build tree from sample files
    start_time = time.time()
    success_count = builder.build_from_files(sample_files)
    elapsed = time.time() - start_time
    
    tree = builder.tree
    
    print(f"  Concurrency Model: Serial")
    print(f"  Files Loaded: {success_count}")
    print(f"  Total Samples: {tree.total_samples}")
    print(f"  Node Count: {tree.node_count}")
    print(f"  Time: {elapsed*1000:.1f} ms")
    
    return tree


def example_fine_grained_lock_building(sample_files, libmap_files):
    """
    Example 2: Fine-Grained Lock Tree Building
    
    Uses per-node locks for parallel subtree insertion.
    Good for deep trees with many distinct paths.
    """
    print("\n" + "=" * 70)
    print("Example 2: Fine-Grained Lock Tree Building")
    print("=" * 70)
    
    # Create TreeBuilder with FineGrainedLock concurrency model
    builder = perflow.TreeBuilder(
        perflow.TreeBuildMode.ContextFree,
        perflow.SampleCountMode.Both,
        perflow.ConcurrencyModel.FineGrainedLock
    )
    
    # Configure thread count (0 = auto-detect)
    builder.set_num_threads(4)
    
    # Load library maps if available
    if libmap_files:
        builder.load_library_maps(libmap_files)
    
    # Build tree from sample files in parallel
    start_time = time.time()
    success_count = builder.build_from_files_parallel(sample_files)
    elapsed = time.time() - start_time
    
    tree = builder.tree
    
    print(f"  Concurrency Model: FineGrainedLock")
    print(f"  Threads: {builder.num_threads}")
    print(f"  Files Loaded: {success_count}")
    print(f"  Total Samples: {tree.total_samples}")
    print(f"  Node Count: {tree.node_count}")
    print(f"  Time: {elapsed*1000:.1f} ms")
    
    return tree


def example_thread_local_merge_building(sample_files, libmap_files):
    """
    Example 3: Thread-Local Merge Tree Building
    
    Each thread builds a separate tree, which are merged after construction.
    Best for processing many independent files.
    """
    print("\n" + "=" * 70)
    print("Example 3: Thread-Local Merge Tree Building")
    print("=" * 70)
    
    # Create TreeBuilder with ThreadLocalMerge concurrency model
    builder = perflow.TreeBuilder(
        perflow.TreeBuildMode.ContextFree,
        perflow.SampleCountMode.Both,
        perflow.ConcurrencyModel.ThreadLocalMerge
    )
    
    # Configure thread count
    builder.set_num_threads(4)
    
    # Load library maps if available
    if libmap_files:
        builder.load_library_maps(libmap_files)
    
    # Build tree from sample files in parallel
    start_time = time.time()
    success_count = builder.build_from_files_parallel(sample_files)
    elapsed = time.time() - start_time
    
    tree = builder.tree
    
    print(f"  Concurrency Model: ThreadLocalMerge")
    print(f"  Threads: {builder.num_threads}")
    print(f"  Files Loaded: {success_count}")
    print(f"  Total Samples: {tree.total_samples}")
    print(f"  Node Count: {tree.node_count}")
    print(f"  Time: {elapsed*1000:.1f} ms")
    
    return tree


def example_lock_free_building(sample_files, libmap_files):
    """
    Example 4: Lock-Free Tree Building
    
    Uses atomic operations for counters, locks only for structural changes.
    Good when many threads update the same nodes.
    """
    print("\n" + "=" * 70)
    print("Example 4: Lock-Free Tree Building")
    print("=" * 70)
    
    # Create TreeBuilder with LockFree concurrency model
    builder = perflow.TreeBuilder(
        perflow.TreeBuildMode.ContextFree,
        perflow.SampleCountMode.Both,
        perflow.ConcurrencyModel.LockFree
    )
    
    # Configure thread count
    builder.set_num_threads(4)
    
    # Load library maps if available
    if libmap_files:
        builder.load_library_maps(libmap_files)
    
    # Build tree from sample files in parallel
    start_time = time.time()
    success_count = builder.build_from_files_parallel(sample_files)
    elapsed = time.time() - start_time
    
    tree = builder.tree
    
    print(f"  Concurrency Model: LockFree")
    print(f"  Threads: {builder.num_threads}")
    print(f"  Files Loaded: {success_count}")
    print(f"  Total Samples: {tree.total_samples}")
    print(f"  Node Count: {tree.node_count}")
    print(f"  Time: {elapsed*1000:.1f} ms")
    
    return tree


def example_dataflow_with_concurrency(sample_files, libmap_files):
    """
    Example 5: Using Dataflow Framework with Concurrency
    
    Demonstrates using the high-level dataflow API with concurrency configuration.
    """
    print("\n" + "=" * 70)
    print("Example 5: Dataflow Framework with Concurrency")
    print("=" * 70)
    
    from perflow.dataflow import WorkflowBuilder
    
    # Build workflow with parallel processing
    start_time = time.time()
    results = (
        WorkflowBuilder("ParallelAnalysis")
        .load_data(
            sample_files,
            libmap_files,
            concurrency_model='FineGrainedLock',
            num_threads=4
        )
        .find_hotspots(top_n=5, mode='exclusive')
        .analyze_balance()
        .execute()
    )
    elapsed = time.time() - start_time
    
    print(f"  Concurrency Model: FineGrainedLock (via Dataflow)")
    print(f"  Execution Time: {elapsed*1000:.1f} ms")
    
    # Display results
    for node_id, output in results.items():
        if 'hotspots' in output:
            print(f"\n  Top 5 Hotspots:")
            for i, h in enumerate(output['hotspots'][:5], 1):
                func = h.function_name[:30] if len(h.function_name) > 30 else h.function_name
                print(f"    {i}. {func}: {h.self_percentage:.1f}%")
        
        if 'balance' in output:
            balance = output['balance']
            print(f"\n  Workload Balance:")
            print(f"    Imbalance factor: {balance.imbalance_factor:.2f}")
    
    return results


def benchmark_concurrency_models(sample_files, libmap_files):
    """
    Benchmark all concurrency models.
    
    Compares the performance of different concurrency models.
    """
    print("\n" + "=" * 70)
    print("Benchmark: Concurrency Models Comparison")
    print("=" * 70)
    
    models = [
        ('Serial', perflow.ConcurrencyModel.Serial),
        ('FineGrainedLock', perflow.ConcurrencyModel.FineGrainedLock),
        ('ThreadLocalMerge', perflow.ConcurrencyModel.ThreadLocalMerge),
        ('LockFree', perflow.ConcurrencyModel.LockFree),
    ]
    
    print(f"\n{'Model':<20} {'Time (ms)':<15} {'Samples':<15} {'Nodes':<10}")
    print("-" * 60)
    
    results = []
    for name, model in models:
        builder = perflow.TreeBuilder(
            perflow.TreeBuildMode.ContextFree,
            perflow.SampleCountMode.Both,
            model
        )
        
        if model != perflow.ConcurrencyModel.Serial:
            builder.set_num_threads(4)
        
        if libmap_files:
            builder.load_library_maps(libmap_files)
        
        start_time = time.time()
        if model == perflow.ConcurrencyModel.Serial:
            builder.build_from_files(sample_files)
        else:
            builder.build_from_files_parallel(sample_files)
        elapsed = time.time() - start_time
        
        tree = builder.tree
        
        print(f"{name:<20} {elapsed*1000:<15.1f} {tree.total_samples:<15} {tree.node_count:<10}")
        results.append((name, elapsed, tree.total_samples, tree.node_count))
    
    print("-" * 60)
    return results


def main():
    """Main function running all examples."""
    print("PerFlow Parallel Tree Building Example (Python)")
    print("=" * 70)
    print(f"Version: {perflow.version()}")
    
    # Parse command line arguments
    if len(sys.argv) < 2:
        print("\nUsage: python3 parallel_tree_building_example.py <data_directory> [num_processes]")
        print()
        print("Arguments:")
        print("  data_directory  Directory containing perflow_mpi_rank_*.pflw files")
        print("  num_processes   Optional: Number of MPI processes (auto-detect if omitted)")
        print()
        print("Available Concurrency Models:")
        print("  - Serial: Single-threaded, no parallelism (default)")
        print("  - FineGrainedLock: Per-node locks for parallel subtree insertion")
        print("  - ThreadLocalMerge: Thread-local trees merged after construction")
        print("  - LockFree: Atomic operations for counters, locks for structural changes")
        return 1
    
    data_dir = sys.argv[1]
    num_processes = int(sys.argv[2]) if len(sys.argv) > 2 else None
    
    if not os.path.isdir(data_dir):
        print(f"Error: Directory not found: {data_dir}")
        return 1
    
    # Find sample files
    sample_files, libmap_files = find_sample_files(data_dir, num_processes)
    
    if not sample_files:
        print(f"Error: No sample files found in {data_dir}")
        print(f"Expected files like: perflow_mpi_rank_0.pflw")
        return 1
    
    print(f"\nData directory: {data_dir}")
    print(f"Sample files found: {len(sample_files)}")
    print(f"Library map files found: {len(libmap_files)}")
    
    try:
        # Run examples
        example_serial_tree_building(sample_files, libmap_files)
        example_fine_grained_lock_building(sample_files, libmap_files)
        example_thread_local_merge_building(sample_files, libmap_files)
        example_lock_free_building(sample_files, libmap_files)
        example_dataflow_with_concurrency(sample_files, libmap_files)
        benchmark_concurrency_models(sample_files, libmap_files)
        
        print("\n" + "=" * 70)
        print("Summary: Concurrency Model Selection Guide")
        print("=" * 70)
        print("  - Serial: Best for small datasets or debugging")
        print("  - FineGrainedLock: Good for deep trees with many distinct paths")
        print("  - ThreadLocalMerge: Best for processing many independent files")
        print("  - LockFree: Good when many threads update the same nodes")
        print("=" * 70)
        
        return 0
    
    except ImportError as e:
        print(f"\nError: PerFlow module not available: {e}")
        print("Please build the Python bindings first.")
        return 1
    except Exception as e:
        print(f"\nError during analysis: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
