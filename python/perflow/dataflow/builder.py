# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Workflow Builder for PerFlow Dataflow Framework

This module provides a fluent API for building analysis workflows,
making it easy to compose common analysis patterns.
"""

from typing import Any, Callable, Dict, List, Optional, Tuple, Union

from .graph import DataflowGraph, DataflowNode
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


class WorkflowBuilder:
    """
    Fluent builder for creating analysis workflows.
    
    Provides a convenient API for building dataflow graphs with
    method chaining and sensible defaults.
    
    Example:
        >>> workflow = (
        ...     WorkflowBuilder("MyAnalysis")
        ...     .load_data([('rank_0.pflw', 0), ('rank_1.pflw', 1)])
        ...     .find_hotspots(top_n=10)
        ...     .analyze_balance()
        ...     .build()
        ... )
        >>> results = workflow.execute()
    """
    
    def __init__(self, name: str = "Workflow"):
        """
        Initialize the workflow builder.
        
        Args:
            name: Name for the workflow
        """
        self._name = name
        self._graph = DataflowGraph(name=name)
        self._current_node: Optional[DataflowNode] = None
        self._load_node: Optional[LoadDataNode] = None
        self._nodes: Dict[str, DataflowNode] = {}
    
    def load_data(
        self,
        sample_files: List[Tuple[str, int]],
        libmap_files: Optional[List[Tuple[str, int]]] = None,
        mode: str = 'ContextFree',
        count_mode: str = 'Both',
        concurrency_model: str = 'Serial',
        num_threads: int = 0,
        time_per_sample: float = 1000.0
    ) -> 'WorkflowBuilder':
        """
        Add a data loading node.
        
        Args:
            sample_files: List of (filepath, process_id) tuples
            libmap_files: Optional list of (filepath, process_id) tuples for library maps
            mode: Tree build mode ('ContextFree' or 'ContextAware')
            count_mode: Sample count mode ('Exclusive', 'Inclusive', or 'Both')
            concurrency_model: Concurrency model ('Serial', 'FineGrainedLock', 
                             'ThreadLocalMerge', or 'LockFree')
            num_threads: Number of threads for parallel operations (0 = auto-detect)
            time_per_sample: Estimated time per sample in microseconds
        
        Returns:
            Self for method chaining
        """
        node = LoadDataNode(
            sample_files=sample_files,
            libmap_files=libmap_files,
            mode=mode,
            count_mode=count_mode,
            concurrency_model=concurrency_model,
            num_threads=num_threads,
            time_per_sample=time_per_sample,
            name="LoadData"
        )
        
        self._graph.add_node(node)
        self._load_node = node
        self._current_node = node
        self._nodes['load'] = node
        
        return self
    
    def find_hotspots(
        self,
        top_n: int = 10,
        mode: str = 'exclusive',
        name: str = "HotspotAnalysis"
    ) -> 'WorkflowBuilder':
        """
        Add a hotspot analysis node.
        
        Args:
            top_n: Number of top hotspots to find
            mode: Analysis mode ('exclusive' or 'inclusive')
            name: Node name
        
        Returns:
            Self for method chaining
        """
        node = HotspotAnalysisNode(top_n=top_n, mode=mode, name=name)
        self._graph.add_node(node)
        
        # Connect to data source
        if self._load_node:
            self._graph.connect(self._load_node, node, 'tree')
        elif self._current_node and 'tree' in self._current_node.outputs:
            self._graph.connect(self._current_node, node, 'tree')
        
        self._current_node = node
        self._nodes['hotspots'] = node
        
        return self
    
    def analyze_balance(self, name: str = "BalanceAnalysis") -> 'WorkflowBuilder':
        """
        Add a balance analysis node.
        
        Args:
            name: Node name
        
        Returns:
            Self for method chaining
        """
        node = BalanceAnalysisNode(name=name)
        self._graph.add_node(node)
        
        # Connect to data source
        if self._load_node:
            self._graph.connect(self._load_node, node, 'tree')
        elif self._current_node and 'tree' in self._current_node.outputs:
            self._graph.connect(self._current_node, node, 'tree')
        
        self._current_node = node
        self._nodes['balance'] = node
        
        return self
    
    def filter_nodes(
        self,
        min_samples: Optional[int] = None,
        max_samples: Optional[int] = None,
        min_self_samples: Optional[int] = None,
        function_pattern: Optional[str] = None,
        library_name: Optional[str] = None,
        predicate: Optional[Callable[[Any], bool]] = None,
        name: str = "Filter"
    ) -> 'WorkflowBuilder':
        """
        Add a filter node.
        
        Args:
            min_samples: Minimum total samples threshold
            max_samples: Maximum total samples threshold  
            min_self_samples: Minimum self samples threshold
            function_pattern: Function name pattern to match
            library_name: Library name to filter by
            predicate: Custom predicate function
            name: Node name
        
        Returns:
            Self for method chaining
        """
        node = FilterNode(
            min_samples=min_samples,
            max_samples=max_samples,
            min_self_samples=min_self_samples,
            function_pattern=function_pattern,
            library_name=library_name,
            predicate=predicate,
            name=name
        )
        self._graph.add_node(node)
        
        # Connect to data source
        if self._load_node:
            self._graph.connect(self._load_node, node, 'tree')
        
        self._current_node = node
        self._nodes['filter'] = node
        
        return self
    
    def traverse(
        self,
        visitor: Callable[[Any], Any],
        order: str = 'preorder',
        stop_condition: Optional[Callable[[Any], bool]] = None,
        name: str = "Traverse"
    ) -> 'WorkflowBuilder':
        """
        Add a tree traversal node.
        
        Args:
            visitor: Function to apply to each node
            order: Traversal order ('preorder', 'postorder', 'levelorder')
            stop_condition: Optional function to stop traversal
            name: Node name
        
        Returns:
            Self for method chaining
        """
        node = TreeTraversalNode(
            visitor=visitor,
            order=order,
            stop_condition=stop_condition,
            name=name
        )
        self._graph.add_node(node)
        
        # Connect to data source
        if self._load_node:
            self._graph.connect(self._load_node, node, 'tree')
        
        self._current_node = node
        self._nodes['traverse'] = node
        
        return self
    
    def transform(
        self,
        transform_fn: Callable[[Dict[str, Any]], Dict[str, Any]],
        input_ports: Optional[Dict[str, str]] = None,
        output_ports: Optional[Dict[str, str]] = None,
        name: str = "Transform"
    ) -> 'WorkflowBuilder':
        """
        Add a transform node.
        
        Args:
            transform_fn: Function to transform inputs to outputs
            input_ports: Input port specifications
            output_ports: Output port specifications
            name: Node name
        
        Returns:
            Self for method chaining
        """
        node = TransformNode(
            transform=transform_fn,
            input_ports=input_ports,
            output_ports=output_ports,
            name=name
        )
        self._graph.add_node(node)
        self._current_node = node
        self._nodes['transform'] = node
        
        return self
    
    def custom(
        self,
        execute_fn: Callable[[Dict[str, Any]], Dict[str, Any]],
        input_ports: Optional[Dict[str, str]] = None,
        output_ports: Optional[Dict[str, str]] = None,
        name: str = "Custom"
    ) -> 'WorkflowBuilder':
        """
        Add a custom node.
        
        Args:
            execute_fn: Function to execute
            input_ports: Input port specifications
            output_ports: Output port specifications
            name: Node name
        
        Returns:
            Self for method chaining
        """
        node = CustomNode(
            execute_fn=execute_fn,
            input_ports=input_ports,
            output_ports=output_ports,
            name=name
        )
        self._graph.add_node(node)
        self._current_node = node
        self._nodes[name.lower()] = node
        
        return self
    
    def add_node(self, node: DataflowNode) -> 'WorkflowBuilder':
        """
        Add an arbitrary node to the workflow.
        
        Args:
            node: The node to add
        
        Returns:
            Self for method chaining
        """
        self._graph.add_node(node)
        self._current_node = node
        self._nodes[node.name.lower()] = node
        
        return self
    
    def connect(
        self,
        source_name: str,
        target_name: str,
        port: str,
        source_port: Optional[str] = None,
        target_port: Optional[str] = None
    ) -> 'WorkflowBuilder':
        """
        Connect two nodes by name.
        
        Args:
            source_name: Name of source node (lowercase)
            target_name: Name of target node (lowercase)
            port: Port name (or source_port/target_port for different names)
            source_port: Explicit source port name
            target_port: Explicit target port name
        
        Returns:
            Self for method chaining
        """
        source = self._nodes.get(source_name.lower())
        target = self._nodes.get(target_name.lower())
        
        if not source:
            raise ValueError(f"No node named '{source_name}' in workflow")
        if not target:
            raise ValueError(f"No node named '{target_name}' in workflow")
        
        self._graph.connect(source, target, port, source_port, target_port)
        return self
    
    def get_node(self, name: str) -> Optional[DataflowNode]:
        """Get a node by name."""
        return self._nodes.get(name.lower())
    
    def build(self) -> DataflowGraph:
        """
        Build and return the dataflow graph.
        
        Returns:
            The constructed DataflowGraph
        """
        return self._graph
    
    def execute(
        self,
        executor: Optional[GraphExecutor] = None,
        parallel: bool = False,
        caching: bool = False,
        **kwargs
    ) -> Dict[str, Dict[str, Any]]:
        """
        Build and execute the workflow.
        
        Args:
            executor: Optional custom executor
            parallel: If True, use parallel executor
            caching: If True, use caching executor
            **kwargs: Additional arguments for executor
        
        Returns:
            Dictionary mapping node IDs to their outputs
        """
        if executor is None:
            if caching:
                executor = CachingExecutor()
            elif parallel:
                executor = ParallelExecutor()
            else:
                executor = GraphExecutor()
        
        return self._graph.execute(executor=executor, **kwargs)


def create_hotspot_workflow(
    sample_files: List[Tuple[str, int]],
    libmap_files: Optional[List[Tuple[str, int]]] = None,
    top_n: int = 10
) -> DataflowGraph:
    """
    Create a simple hotspot analysis workflow.
    
    Args:
        sample_files: List of (filepath, process_id) tuples
        libmap_files: Optional library map files
        top_n: Number of hotspots to find
    
    Returns:
        Configured DataflowGraph
    """
    return (
        WorkflowBuilder("HotspotAnalysis")
        .load_data(sample_files, libmap_files)
        .find_hotspots(top_n=top_n)
        .build()
    )


def create_balance_workflow(
    sample_files: List[Tuple[str, int]],
    libmap_files: Optional[List[Tuple[str, int]]] = None
) -> DataflowGraph:
    """
    Create a workload balance analysis workflow.
    
    Args:
        sample_files: List of (filepath, process_id) tuples
        libmap_files: Optional library map files
    
    Returns:
        Configured DataflowGraph
    """
    return (
        WorkflowBuilder("BalanceAnalysis")
        .load_data(sample_files, libmap_files)
        .analyze_balance()
        .build()
    )


def create_full_analysis_workflow(
    sample_files: List[Tuple[str, int]],
    libmap_files: Optional[List[Tuple[str, int]]] = None,
    top_n: int = 10
) -> DataflowGraph:
    """
    Create a comprehensive analysis workflow with hotspots and balance.
    
    Args:
        sample_files: List of (filepath, process_id) tuples
        libmap_files: Optional library map files
        top_n: Number of hotspots to find
    
    Returns:
        Configured DataflowGraph
    """
    return (
        WorkflowBuilder("FullAnalysis")
        .load_data(sample_files, libmap_files)
        .find_hotspots(top_n=top_n, name="ExclusiveHotspots")
        .find_hotspots(top_n=top_n, mode='inclusive', name="InclusiveHotspots")
        .analyze_balance()
        .build()
    )
