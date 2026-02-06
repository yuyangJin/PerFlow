# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
PerFlow - Programmable and Fast Performance Analysis for Parallel Programs

This module provides Python bindings for the PerFlow C++ performance analysis library.

Example usage:
    >>> import perflow
    >>> builder = perflow.TreeBuilder()
    >>> builder.build_from_file("samples.pflw", process_id=0)
    >>> tree = builder.tree
    >>> hotspots = perflow.HotspotAnalyzer.find_hotspots(tree, top_n=10)
    >>> for h in hotspots:
    ...     print(f"{h.function_name}: {h.self_percentage:.1f}%")
"""

from ._perflow import (
    # Enumerations
    TreeBuildMode,
    SampleCountMode,
    
    # Core classes
    ResolvedFrame,
    TreeNode,
    PerformanceTree,
    TreeBuilder,
    OffsetConverter,
    
    # Analysis results
    BalanceAnalysisResult,
    HotspotInfo,
    
    # Analyzers
    BalanceAnalyzer,
    HotspotAnalyzer,
    
    # Parallel processing
    ParallelFileReader,
    FileReadResult,
    
    # Utility functions
    version,
)

__version__ = version()
__all__ = [
    # Enums
    'TreeBuildMode',
    'SampleCountMode',
    
    # Core classes
    'ResolvedFrame',
    'TreeNode',
    'PerformanceTree',
    'TreeBuilder',
    'OffsetConverter',
    
    # Analysis results
    'BalanceAnalysisResult',
    'HotspotInfo',
    
    # Analyzers
    'BalanceAnalyzer',
    'HotspotAnalyzer',
    
    # Parallel processing
    'ParallelFileReader',
    'FileReadResult',
    
    # Functions
    'version',
]


def analyze_samples(sample_files, libmap_files=None, top_n=10, mode=TreeBuildMode.ContextFree):
    """
    Convenience function to analyze performance sample files.
    
    Args:
        sample_files: List of (filepath, process_id) tuples for sample data files
        libmap_files: Optional list of (filepath, process_id) tuples for library maps
        top_n: Number of top hotspots to return
        mode: Tree build mode (ContextFree or ContextAware)
    
    Returns:
        dict with keys:
            - 'tree': The PerformanceTree object
            - 'hotspots': List of HotspotInfo objects
            - 'balance': BalanceAnalysisResult object
    
    Example:
        >>> results = perflow.analyze_samples([
        ...     ('rank_0.pflw', 0),
        ...     ('rank_1.pflw', 1),
        ... ])
        >>> print(f"Top hotspot: {results['hotspots'][0].function_name}")
    """
    builder = TreeBuilder(mode)
    
    if libmap_files:
        builder.load_library_maps(libmap_files)
    
    builder.build_from_files(sample_files)
    tree = builder.tree
    
    return {
        'tree': tree,
        'hotspots': HotspotAnalyzer.find_hotspots(tree, top_n),
        'balance': BalanceAnalyzer.analyze(tree),
    }


def print_hotspots(tree, top_n=10, show_inclusive=False):
    """
    Print hotspot analysis results in a formatted table.
    
    Args:
        tree: PerformanceTree to analyze
        top_n: Number of top hotspots to show
        show_inclusive: If True, show inclusive (total) time; otherwise show exclusive (self) time
    """
    if show_inclusive:
        hotspots = HotspotAnalyzer.find_total_hotspots(tree, top_n)
        print(f"\nTop {len(hotspots)} Hotspots (Inclusive/Total Time):")
        print("-" * 70)
        print(f"{'Rank':<5} {'Function':<40} {'Samples':<12} {'%':<8}")
        print("-" * 70)
        for i, h in enumerate(hotspots, 1):
            func = h.function_name[:38] if len(h.function_name) > 38 else h.function_name
            print(f"{i:<5} {func:<40} {h.total_samples:<12} {h.percentage:.1f}%")
    else:
        hotspots = HotspotAnalyzer.find_hotspots(tree, top_n)
        print(f"\nTop {len(hotspots)} Hotspots (Exclusive/Self Time):")
        print("-" * 70)
        print(f"{'Rank':<5} {'Function':<40} {'Samples':<12} {'%':<8}")
        print("-" * 70)
        for i, h in enumerate(hotspots, 1):
            func = h.function_name[:38] if len(h.function_name) > 38 else h.function_name
            print(f"{i:<5} {func:<40} {h.self_samples:<12} {h.self_percentage:.1f}%")
    print("-" * 70)


def print_balance(tree):
    """
    Print workload balance analysis results.
    
    Args:
        tree: PerformanceTree to analyze
    """
    balance = BalanceAnalyzer.analyze(tree)
    
    print("\nWorkload Balance Analysis:")
    print("-" * 40)
    print(f"  Processes:        {len(balance.process_samples)}")
    print(f"  Mean samples:     {balance.mean_samples:.1f}")
    print(f"  Std deviation:    {balance.std_dev_samples:.1f}")
    print(f"  Min samples:      {balance.min_samples:.0f} (process {balance.least_loaded_process})")
    print(f"  Max samples:      {balance.max_samples:.0f} (process {balance.most_loaded_process})")
    print(f"  Imbalance factor: {balance.imbalance_factor:.2f}")
    print("-" * 40)


def traverse_tree(tree, visitor, order='preorder'):
    """
    Traverse the performance tree with a visitor function.
    
    Args:
        tree: PerformanceTree to traverse
        visitor: Function that takes a TreeNode and returns True to continue, False to stop
        order: Traversal order - 'preorder', 'postorder', or 'levelorder'
    
    Example:
        >>> def print_node(node):
        ...     print(f"{node.function_name}: {node.total_samples}")
        ...     return True
        >>> perflow.traverse_tree(tree, print_node)
    """
    if order == 'preorder':
        tree.traverse_preorder(visitor)
    elif order == 'postorder':
        tree.traverse_postorder(visitor)
    elif order == 'levelorder':
        tree.traverse_levelorder(visitor)
    else:
        raise ValueError(f"Unknown traversal order: {order}")
