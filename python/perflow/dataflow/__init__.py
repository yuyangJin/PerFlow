# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
PerFlow Dataflow-based Analysis Framework

This module provides a dataflow-based programming framework for performance analysis.
It allows users to compose analysis workflows as directed acyclic graphs (DAGs) where:
- Nodes represent analysis subtasks
- Edges represent performance analysis result transfers between subtasks

Key Features:
- Dataflow graph abstraction for flexible composition of analysis tasks
- Optimization mechanisms: graph fusion, parallel execution, lazy evaluation, caching
- User-friendly interface for defining analysis workflows

Example usage:
    >>> from perflow.dataflow import DataflowGraph, LoadDataNode, HotspotAnalysisNode
    >>> 
    >>> # Create a dataflow graph
    >>> graph = DataflowGraph()
    >>> 
    >>> # Add nodes
    >>> load_node = LoadDataNode(sample_files=[('rank_0.pflw', 0), ('rank_1.pflw', 1)])
    >>> hotspot_node = HotspotAnalysisNode(top_n=10)
    >>> 
    >>> # Connect nodes
    >>> graph.add_node(load_node)
    >>> graph.add_node(hotspot_node)
    >>> graph.connect(load_node, hotspot_node, 'tree')
    >>> 
    >>> # Execute the graph
    >>> results = graph.execute()
    >>> hotspots = results[hotspot_node]['hotspots']
"""

from .graph import DataflowGraph, DataflowNode, DataflowEdge
from .nodes import (
    LoadDataNode,
    HotspotAnalysisNode,
    BalanceAnalysisNode,
    FilterNode,
    TreeTraversalNode,
    TransformNode,
    MergeNode,
    CustomNode,
)
from .executor import GraphExecutor, ParallelExecutor, CachingExecutor
from .builder import WorkflowBuilder

__all__ = [
    # Core graph classes
    'DataflowGraph',
    'DataflowNode',
    'DataflowEdge',
    
    # Pre-built analysis nodes
    'LoadDataNode',
    'HotspotAnalysisNode',
    'BalanceAnalysisNode',
    'FilterNode',
    'TreeTraversalNode',
    'TransformNode',
    'MergeNode',
    'CustomNode',
    
    # Executors
    'GraphExecutor',
    'ParallelExecutor',
    'CachingExecutor',
    
    # Builder
    'WorkflowBuilder',
]
