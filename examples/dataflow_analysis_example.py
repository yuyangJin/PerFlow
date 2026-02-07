#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Example: Dataflow-based Performance Analysis

This example demonstrates how to use the PerFlow dataflow framework
to compose and execute performance analysis workflows.

The dataflow framework provides:
1. A graph abstraction where nodes are analysis tasks and edges are data flows
2. Optimization mechanisms: parallel execution, caching, lazy evaluation
3. User-friendly fluent API for building workflows

Usage:
    python3 dataflow_analysis_example.py <data_directory> [num_processes]

Example:
    python3 dataflow_analysis_example.py ./sample_output 4
"""

import sys
import os
import glob

# Add the python directory to path if running from source
# script_dir = os.path.dirname(os.path.abspath(__file__))
# python_dir = os.path.join(os.path.dirname(script_dir), 'python')
# if os.path.exists(python_dir):
#     sys.path.insert(0, python_dir)


def find_sample_files(data_dir, num_processes=None):
    """Find sample data files in the specified directory."""
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


def example_basic_workflow(sample_files, libmap_files):
    """
    Example 1: Basic workflow using WorkflowBuilder
    
    This demonstrates the fluent API for building simple analysis workflows.
    """
    print("\n" + "=" * 70)
    print("Example 1: Basic Workflow with WorkflowBuilder")
    print("=" * 70)
    
    from perflow.dataflow import WorkflowBuilder
    
    # Build and execute a simple workflow
    results = (
        WorkflowBuilder("BasicAnalysis")
        .load_data(sample_files, libmap_files)
        .find_hotspots(top_n=5)
        .analyze_balance()
        .execute()
    )
    
    # Access results
    print("\nWorkflow Results:")
    print("-" * 40)
    
    # Find hotspots results (need to find the right node)
    for node_id, output in results.items():
        if 'hotspots' in output:
            print(f"\nTop 5 Hotspots:")
            for i, h in enumerate(output['hotspots'][:5], 1):
                print(f"  {i}. {h.function_name}: {h.self_percentage:.1f}%")
        
        if 'balance' in output:
            balance = output['balance']
            print(f"\nWorkload Balance:")
            print(f"  Imbalance factor: {balance.imbalance_factor:.2f}")
            print(f"  Most loaded: process {balance.most_loaded_process}")
            print(f"  Least loaded: process {balance.least_loaded_process}")
    
    return results


def example_manual_graph_construction(sample_files, libmap_files):
    """
    Example 2: Manual graph construction
    
    This demonstrates building a dataflow graph manually with full control.
    """
    print("\n" + "=" * 70)
    print("Example 2: Manual Graph Construction")
    print("=" * 70)
    
    from perflow.dataflow import (
        DataflowGraph,
        LoadDataNode,
        HotspotAnalysisNode,
        BalanceAnalysisNode,
        FilterNode,
    )
    
    # Create nodes
    load_node = LoadDataNode(
        sample_files=sample_files,
        libmap_files=libmap_files,
        mode='ContextFree',
        name="LoadData"
    )
    
    exclusive_hotspots = HotspotAnalysisNode(
        top_n=10,
        mode='exclusive',
        name="ExclusiveHotspots"
    )
    
    inclusive_hotspots = HotspotAnalysisNode(
        top_n=10,
        mode='inclusive',
        name="InclusiveHotspots"
    )
    
    balance_node = BalanceAnalysisNode(name="Balance")
    
    filter_node = FilterNode(
        min_samples=10,
        name="FilterByMinSamples"
    )
    
    # Create graph and add nodes
    graph = DataflowGraph(name="ComprehensiveAnalysis")
    graph.add_node(load_node)
    graph.add_node(exclusive_hotspots)
    graph.add_node(inclusive_hotspots)
    graph.add_node(balance_node)
    graph.add_node(filter_node)
    
    # Connect nodes - all analysis nodes depend on load_node
    graph.connect(load_node, exclusive_hotspots, 'tree')
    graph.connect(load_node, inclusive_hotspots, 'tree')
    graph.connect(load_node, balance_node, 'tree')
    graph.connect(load_node, filter_node, 'tree')
    
    # Validate and visualize the graph
    graph.validate()
    print(f"\nGraph: {graph}")
    print(f"Nodes: {len(graph.nodes)}")
    print(f"Edges: {len(graph.edges)}")
    
    # Get parallel execution groups
    groups = graph.get_parallel_groups()
    print(f"\nParallel Execution Groups:")
    for i, group in enumerate(groups):
        node_names = [n.name for n in group]
        print(f"  Group {i}: {', '.join(node_names)}")
    
    # Execute
    results = graph.execute()
    
    print(f"\nExecution complete. Results from {len(results)} nodes.")
    
    return results


def example_parallel_execution(sample_files, libmap_files):
    """
    Example 3: Parallel execution
    
    This demonstrates using the parallel executor for improved performance.
    """
    print("\n" + "=" * 70)
    print("Example 3: Parallel Execution")
    print("=" * 70)
    
    from perflow.dataflow import (
        WorkflowBuilder,
        ParallelExecutor,
    )
    import time
    
    # Build a workflow with multiple independent analysis tasks
    workflow = (
        WorkflowBuilder("ParallelAnalysis")
        .load_data(sample_files, libmap_files)
        .find_hotspots(top_n=10, mode='exclusive', name="ExclusiveHotspots")
        .find_hotspots(top_n=10, mode='inclusive', name="InclusiveHotspots")
        .analyze_balance()
        .filter_nodes(min_samples=5)
        .build()
    )
    
    # Execute with parallel executor
    executor = ParallelExecutor(max_workers=4)
    
    def progress_callback(completed, total, current):
        print(f"  Progress: {completed}/{total} - {current}")
    
    executor.set_progress_callback(progress_callback)
    
    print("\nExecuting with ParallelExecutor:")
    start_time = time.time()
    results = workflow.execute(executor=executor)
    elapsed = time.time() - start_time
    
    print(f"\nExecution completed in {elapsed:.3f} seconds")
    
    # Print execution times per node
    exec_times = executor.get_execution_times()
    print("\nPer-node execution times:")
    for node_id, exec_time in exec_times.items():
        print(f"  {node_id}: {exec_time:.3f}s")
    
    return results


def example_caching_execution(sample_files, libmap_files):
    """
    Example 4: Caching execution with lazy evaluation
    
    This demonstrates using the caching executor for repeated analyses.
    """
    print("\n" + "=" * 70)
    print("Example 4: Caching Execution")
    print("=" * 70)
    
    from perflow.dataflow import (
        WorkflowBuilder,
        CachingExecutor,
    )
    import time
    
    # Create a caching executor
    executor = CachingExecutor(max_cache_size=50)
    
    # Build a workflow
    builder = WorkflowBuilder("CachedAnalysis")
    workflow = (
        builder
        .load_data(sample_files, libmap_files)
        .find_hotspots(top_n=10)
        .analyze_balance()
        .build()
    )
    
    # First execution - computes everything
    print("\nFirst execution (cold cache):")
    start_time = time.time()
    results1 = workflow.execute(executor=executor)
    elapsed1 = time.time() - start_time
    print(f"  Completed in {elapsed1:.3f} seconds")
    
    stats1 = executor.get_cache_stats()
    print(f"  Cache stats: hits={stats1['cache_hits']}, misses={stats1['cache_misses']}")
    
    # Reset graph for re-execution
    workflow.reset()
    
    # Second execution - uses cache
    print("\nSecond execution (warm cache):")
    start_time = time.time()
    results2 = workflow.execute(executor=executor)
    elapsed2 = time.time() - start_time
    print(f"  Completed in {elapsed2:.3f} seconds")
    
    stats2 = executor.get_cache_stats()
    print(f"  Cache stats: hits={stats2['cache_hits']}, misses={stats2['cache_misses']}")
    print(f"  Hit rate: {stats2['hit_rate']:.1%}")
    
    if elapsed1 > 0:
        speedup = elapsed1 / max(elapsed2, 0.001)
        print(f"\n  Speedup from caching: {speedup:.1f}x")
    
    return results2


def example_custom_nodes(sample_files, libmap_files):
    """
    Example 5: Custom analysis nodes
    
    This demonstrates creating custom analysis nodes and transformations.
    """
    print("\n" + "=" * 70)
    print("Example 5: Custom Analysis Nodes")
    print("=" * 70)
    
    from perflow.dataflow import (
        WorkflowBuilder,
        CustomNode,
        TransformNode,
    )
    
    # Create a custom node that computes library-level statistics
    def compute_library_stats(inputs):
        tree = inputs['tree']
        
        # Aggregate samples by library
        library_samples = {}
        
        def visitor(node):
            lib = node.library_name
            if lib not in library_samples:
                library_samples[lib] = 0
            library_samples[lib] += node.self_samples
            return True
        
        tree.traverse_preorder(visitor)
        
        # Sort by samples
        sorted_libs = sorted(library_samples.items(), key=lambda x: x[1], reverse=True)
        
        return {
            'library_stats': sorted_libs,
            'library_count': len(library_samples)
        }
    
    library_stats_node = CustomNode(
        execute_fn=compute_library_stats,
        input_ports={'tree': 'PerformanceTree'},
        output_ports={'library_stats': 'list', 'library_count': 'int'},
        name="LibraryStats"
    )
    
    # Create a transform node to format hotspot report
    def format_report(inputs):
        report_lines = []
        if 'hotspots' in inputs:
            for i, h in enumerate(inputs['hotspots'][:10], 1):
                line = f"{i:2d}. {h.function_name:<40} {h.self_percentage:6.2f}%"
                report_lines.append(line)
        
        return {'report': '\n'.join(report_lines)}
    
    # Build workflow with custom nodes
    builder = WorkflowBuilder("CustomAnalysis")
    workflow = (
        builder
        .load_data(sample_files, libmap_files)
        .find_hotspots(top_n=10)
        .add_node(library_stats_node)
    )
    
    # Connect library stats to the load node
    workflow.connect('load', 'librarystats', 'tree')
    
    # Execute
    results = workflow.execute()
    
    # Print library statistics
    for node_id, output in results.items():
        if 'library_stats' in output:
            print(f"\nLibrary-level Statistics:")
            print(f"  Total libraries: {output['library_count']}")
            print(f"\n  Top Libraries by Self Samples:")
            for lib, samples in output['library_stats'][:5]:
                print(f"    {lib}: {samples} samples")
    
    return results


def example_tree_traversal(sample_files, libmap_files):
    """
    Example 6: Tree traversal and filtering
    
    This demonstrates low-level tree traversal and filtering operations.
    """
    print("\n" + "=" * 70)
    print("Example 6: Tree Traversal and Filtering")
    print("=" * 70)
    
    from perflow.dataflow import (
        WorkflowBuilder,
        TreeTraversalNode,
    )
    
    # Create a traversal node that collects call paths
    def collect_path(node):
        if node.self_samples > 0:
            return {
                'path': ' -> '.join(node.get_path()),
                'samples': node.self_samples
            }
        return None
    
    traversal_node = TreeTraversalNode(
        visitor=collect_path,
        order='preorder',
        name="CollectCallPaths"
    )
    
    # Build workflow
    builder = WorkflowBuilder("TraversalAnalysis")
    workflow = (
        builder
        .load_data(sample_files, libmap_files)
        .add_node(traversal_node)
        .filter_nodes(min_self_samples=1, name="HotNodes")
    )
    
    workflow.connect('load', 'collectcallpaths', 'tree')
    
    # Execute
    results = workflow.execute()
    
    # Print results
    for node_id, output in results.items():
        if 'results' in output and output['results']:
            print(f"\nCall Paths with Samples (first 5):")
            sorted_paths = sorted(output['results'], key=lambda x: x['samples'], reverse=True)
            for path_info in sorted_paths[:5]:
                print(f"  [{path_info['samples']:6d}] {path_info['path'][:60]}...")
        
        if 'nodes' in output:
            print(f"\nFiltered nodes with samples: {output['count']}")
    
    return results


def main():
    """Main function running all examples."""
    # Parse command line arguments
    if len(sys.argv) < 2:
        print("Usage: python3 dataflow_analysis_example.py <data_directory> [num_processes]")
        print()
        print("Arguments:")
        print("  data_directory  Directory containing perflow_mpi_rank_*.pflw files")
        print("  num_processes   Optional: Number of MPI processes (auto-detect if omitted)")
        print()
        print("This example demonstrates the PerFlow dataflow-based analysis framework.")
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
        return 1
    
    print("=" * 70)
    print("PerFlow Dataflow-based Analysis Examples")
    print("=" * 70)
    print(f"Data directory: {data_dir}")
    print(f"Sample files: {len(sample_files)}")
    print(f"Library maps: {len(libmap_files)}")
    
    try:
        # Run examples
        example_basic_workflow(sample_files, libmap_files)
        example_manual_graph_construction(sample_files, libmap_files)
        example_parallel_execution(sample_files, libmap_files)
        example_caching_execution(sample_files, libmap_files)
        example_custom_nodes(sample_files, libmap_files)
        example_tree_traversal(sample_files, libmap_files)
        
        print("\n" + "=" * 70)
        print("All examples completed successfully!")
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
