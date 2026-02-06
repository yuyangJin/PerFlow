# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Pre-built Analysis Nodes for PerFlow Dataflow Framework

This module provides ready-to-use analysis subtask nodes:
- LoadDataNode: Load performance sample data files
- HotspotAnalysisNode: Find performance hotspots
- BalanceAnalysisNode: Analyze workload balance
- FilterNode: Filter tree nodes by criteria
- TreeTraversalNode: Traverse and transform tree data
- TransformNode: Apply custom transformations
- MergeNode: Merge results from multiple nodes
- CustomNode: Create custom analysis nodes
"""

from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from .graph import DataflowNode


class LoadDataNode(DataflowNode):
    """
    Node for loading performance sample data files.
    
    This node loads .pflw sample files and optionally .libmap library map files
    to build a PerformanceTree.
    
    Inputs: None (source node)
    
    Outputs:
        - tree: PerformanceTree object
        - builder: TreeBuilder object
    
    Example:
        >>> node = LoadDataNode(
        ...     sample_files=[('rank_0.pflw', 0), ('rank_1.pflw', 1)],
        ...     libmap_files=[('rank_0.libmap', 0), ('rank_1.libmap', 1)],
        ...     mode='ContextFree'
        ... )
    """
    
    def __init__(
        self,
        sample_files: Optional[List[Tuple[str, int]]] = None,
        libmap_files: Optional[List[Tuple[str, int]]] = None,
        mode: str = 'ContextFree',
        count_mode: str = 'Both',
        time_per_sample: float = 1000.0,
        name: str = "LoadData"
    ):
        """
        Initialize LoadDataNode.
        
        Args:
            sample_files: List of (filepath, process_id) tuples for sample data
            libmap_files: Optional list of (filepath, process_id) tuples for library maps
            mode: Tree build mode ('ContextFree' or 'ContextAware')
            count_mode: Sample count mode ('Exclusive', 'Inclusive', or 'Both')
            time_per_sample: Estimated time per sample in microseconds
            name: Node name
        """
        super().__init__(
            name=name,
            inputs={},
            outputs={'tree': 'PerformanceTree', 'builder': 'TreeBuilder'}
        )
        self._sample_files = sample_files or []
        self._libmap_files = libmap_files or []
        self._mode = mode
        self._count_mode = count_mode
        self._time_per_sample = time_per_sample
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Load sample data and build the performance tree."""
        # Import here to avoid circular imports
        try:
            from .. import TreeBuilder, TreeBuildMode, SampleCountMode
        except ImportError:
            raise RuntimeError(
                "PerFlow native module not available. "
                "Please build the Python bindings first."
            )
        
        # Map string modes to enum values
        mode_map = {
            'ContextFree': TreeBuildMode.ContextFree,
            'ContextAware': TreeBuildMode.ContextAware,
        }
        count_mode_map = {
            'Exclusive': SampleCountMode.Exclusive,
            'Inclusive': SampleCountMode.Inclusive,
            'Both': SampleCountMode.Both,
        }
        
        build_mode = mode_map.get(self._mode, TreeBuildMode.ContextFree)
        sample_count_mode = count_mode_map.get(self._count_mode, SampleCountMode.Both)
        
        # Create builder
        builder = TreeBuilder(build_mode, sample_count_mode)
        
        # Load library maps if provided
        if self._libmap_files:
            builder.load_library_maps(self._libmap_files)
        
        # Build tree from sample files
        if self._sample_files:
            builder.build_from_files(self._sample_files, self._time_per_sample)
        
        return {
            'tree': builder.tree,
            'builder': builder
        }


class HotspotAnalysisNode(DataflowNode):
    """
    Node for finding performance hotspots in a PerformanceTree.
    
    Inputs:
        - tree: PerformanceTree to analyze
    
    Outputs:
        - hotspots: List of HotspotInfo objects
        - summary: Dictionary with analysis summary
    
    Example:
        >>> node = HotspotAnalysisNode(top_n=10, mode='exclusive')
    """
    
    def __init__(
        self,
        top_n: int = 10,
        mode: str = 'exclusive',
        name: str = "HotspotAnalysis"
    ):
        """
        Initialize HotspotAnalysisNode.
        
        Args:
            top_n: Number of top hotspots to find
            mode: Analysis mode ('exclusive' for self time, 'inclusive' for total time)
            name: Node name
        """
        super().__init__(
            name=name,
            inputs={'tree': 'PerformanceTree'},
            outputs={'hotspots': 'List[HotspotInfo]', 'summary': 'dict'}
        )
        self._top_n = top_n
        self._mode = mode.lower()
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Find hotspots in the performance tree."""
        self.validate_inputs(inputs)
        tree = inputs['tree']
        
        try:
            from .. import HotspotAnalyzer
        except ImportError:
            raise RuntimeError("PerFlow native module not available.")
        
        if self._mode == 'inclusive' or self._mode == 'total':
            hotspots = HotspotAnalyzer.find_total_hotspots(tree, self._top_n)
        else:
            hotspots = HotspotAnalyzer.find_hotspots(tree, self._top_n)
        
        # Build summary
        summary = {
            'total_samples': tree.total_samples,
            'hotspot_count': len(hotspots),
            'mode': self._mode,
            'top_function': hotspots[0].function_name if hotspots else None,
            'top_percentage': hotspots[0].self_percentage if hotspots else 0.0,
        }
        
        return {
            'hotspots': hotspots,
            'summary': summary
        }


class BalanceAnalysisNode(DataflowNode):
    """
    Node for analyzing workload balance across processes.
    
    Inputs:
        - tree: PerformanceTree to analyze
    
    Outputs:
        - balance: BalanceAnalysisResult object
        - summary: Dictionary with analysis summary
    
    Example:
        >>> node = BalanceAnalysisNode()
    """
    
    def __init__(self, name: str = "BalanceAnalysis"):
        """
        Initialize BalanceAnalysisNode.
        
        Args:
            name: Node name
        """
        super().__init__(
            name=name,
            inputs={'tree': 'PerformanceTree'},
            outputs={'balance': 'BalanceAnalysisResult', 'summary': 'dict'}
        )
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze workload balance."""
        self.validate_inputs(inputs)
        tree = inputs['tree']
        
        try:
            from .. import BalanceAnalyzer
        except ImportError:
            raise RuntimeError("PerFlow native module not available.")
        
        balance = BalanceAnalyzer.analyze(tree)
        
        summary = {
            'process_count': len(balance.process_samples),
            'mean_samples': balance.mean_samples,
            'imbalance_factor': balance.imbalance_factor,
            'most_loaded': balance.most_loaded_process,
            'least_loaded': balance.least_loaded_process,
            'is_balanced': balance.imbalance_factor < 0.1,
        }
        
        return {
            'balance': balance,
            'summary': summary
        }


class FilterNode(DataflowNode):
    """
    Node for filtering tree nodes by various criteria.
    
    Inputs:
        - tree: PerformanceTree to filter
    
    Outputs:
        - nodes: List of filtered TreeNode objects
        - count: Number of matching nodes
    
    Example:
        >>> # Filter by minimum samples
        >>> node = FilterNode(min_samples=100)
        >>> 
        >>> # Filter by function name pattern
        >>> node = FilterNode(function_pattern="compute_*")
        >>> 
        >>> # Filter by library
        >>> node = FilterNode(library_name="libmath.so")
    """
    
    def __init__(
        self,
        min_samples: Optional[int] = None,
        max_samples: Optional[int] = None,
        min_self_samples: Optional[int] = None,
        function_pattern: Optional[str] = None,
        library_name: Optional[str] = None,
        predicate: Optional[Callable[[Any], bool]] = None,
        name: str = "Filter"
    ):
        """
        Initialize FilterNode.
        
        Args:
            min_samples: Minimum total samples threshold
            max_samples: Maximum total samples threshold
            min_self_samples: Minimum self samples threshold
            function_pattern: Function name pattern to match (supports * wildcard)
            library_name: Library name to filter by
            predicate: Custom predicate function taking a TreeNode
            name: Node name
        """
        super().__init__(
            name=name,
            inputs={'tree': 'PerformanceTree'},
            outputs={'nodes': 'List[TreeNode]', 'count': 'int'}
        )
        self._min_samples = min_samples
        self._max_samples = max_samples
        self._min_self_samples = min_self_samples
        self._function_pattern = function_pattern
        self._library_name = library_name
        self._predicate = predicate
    
    def _matches_pattern(self, text: str, pattern: str) -> bool:
        """Simple pattern matching with * wildcard."""
        if '*' not in pattern:
            return text == pattern
        
        parts = pattern.split('*')
        if len(parts) == 2:
            start, end = parts
            return text.startswith(start) and text.endswith(end)
        
        # More complex pattern - just check starts/ends with first/last
        if parts[0] and not text.startswith(parts[0]):
            return False
        if parts[-1] and not text.endswith(parts[-1]):
            return False
        return True
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Filter tree nodes."""
        self.validate_inputs(inputs)
        tree = inputs['tree']
        
        nodes = tree.all_nodes
        filtered = []
        
        for node in nodes:
            # Apply filters
            if self._min_samples is not None and node.total_samples < self._min_samples:
                continue
            if self._max_samples is not None and node.total_samples > self._max_samples:
                continue
            if self._min_self_samples is not None and node.self_samples < self._min_self_samples:
                continue
            if self._function_pattern is not None:
                if not self._matches_pattern(node.function_name, self._function_pattern):
                    continue
            if self._library_name is not None:
                if node.library_name != self._library_name:
                    continue
            if self._predicate is not None:
                if not self._predicate(node):
                    continue
            
            filtered.append(node)
        
        return {
            'nodes': filtered,
            'count': len(filtered)
        }


class TreeTraversalNode(DataflowNode):
    """
    Node for traversing the performance tree and collecting data.
    
    Inputs:
        - tree: PerformanceTree to traverse
    
    Outputs:
        - results: List of results from visitor function
        - visited_count: Number of nodes visited
    
    Example:
        >>> # Collect all function names
        >>> node = TreeTraversalNode(
        ...     visitor=lambda n: n.function_name,
        ...     order='preorder'
        ... )
    """
    
    def __init__(
        self,
        visitor: Optional[Callable[[Any], Any]] = None,
        order: str = 'preorder',
        stop_condition: Optional[Callable[[Any], bool]] = None,
        name: str = "TreeTraversal"
    ):
        """
        Initialize TreeTraversalNode.
        
        Args:
            visitor: Function to apply to each node, returns value to collect
            order: Traversal order ('preorder', 'postorder', or 'levelorder')
            stop_condition: Optional function that returns True to stop traversal
            name: Node name
        """
        super().__init__(
            name=name,
            inputs={'tree': 'PerformanceTree'},
            outputs={'results': 'list', 'visited_count': 'int'}
        )
        self._visitor = visitor or (lambda n: n)
        self._order = order.lower()
        self._stop_condition = stop_condition
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Traverse the tree and collect results."""
        self.validate_inputs(inputs)
        tree = inputs['tree']
        
        results = []
        visited_count = [0]  # Use list to allow modification in closure
        should_stop = [False]
        
        def visitor_wrapper(node):
            if should_stop[0]:
                return False
            
            visited_count[0] += 1
            result = self._visitor(node)
            if result is not None:
                results.append(result)
            
            if self._stop_condition and self._stop_condition(node):
                should_stop[0] = True
                return False
            
            return True
        
        if self._order == 'preorder':
            tree.traverse_preorder(visitor_wrapper)
        elif self._order == 'postorder':
            tree.traverse_postorder(visitor_wrapper)
        elif self._order == 'levelorder':
            tree.traverse_levelorder(visitor_wrapper)
        else:
            raise ValueError(f"Unknown traversal order: {self._order}")
        
        return {
            'results': results,
            'visited_count': visited_count[0]
        }


class TransformNode(DataflowNode):
    """
    Node for applying custom transformations to data.
    
    This is a general-purpose node for applying any transformation function
    to its inputs.
    
    Example:
        >>> # Transform hotspots to a report format
        >>> node = TransformNode(
        ...     transform=lambda inputs: {
        ...         'report': [
        ...             f"{h.function_name}: {h.self_percentage:.1f}%"
        ...             for h in inputs['hotspots']
        ...         ]
        ...     },
        ...     input_ports={'hotspots': 'List[HotspotInfo]'},
        ...     output_ports={'report': 'List[str]'}
        ... )
    """
    
    def __init__(
        self,
        transform: Callable[[Dict[str, Any]], Dict[str, Any]],
        input_ports: Optional[Dict[str, str]] = None,
        output_ports: Optional[Dict[str, str]] = None,
        name: str = "Transform"
    ):
        """
        Initialize TransformNode.
        
        Args:
            transform: Function that takes inputs dict and returns outputs dict
            input_ports: Dictionary of input port names to type descriptions
            output_ports: Dictionary of output port names to type descriptions
            name: Node name
        """
        super().__init__(
            name=name,
            inputs=input_ports or {'data': 'Any'},
            outputs=output_ports or {'result': 'Any'}
        )
        self._transform = transform
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Apply the transformation."""
        return self._transform(inputs)


class MergeNode(DataflowNode):
    """
    Node for merging results from multiple upstream nodes.
    
    Inputs: Multiple named inputs (configurable)
    
    Outputs:
        - merged: Dictionary containing all inputs
        - keys: List of input keys
    
    Example:
        >>> node = MergeNode(
        ...     input_ports=['hotspots', 'balance'],
        ...     merge_function=lambda inputs: {...}
        ... )
    """
    
    def __init__(
        self,
        input_ports: Optional[List[str]] = None,
        merge_function: Optional[Callable[[Dict[str, Any]], Any]] = None,
        name: str = "Merge"
    ):
        """
        Initialize MergeNode.
        
        Args:
            input_ports: List of input port names to merge
            merge_function: Optional function to combine inputs (default: dict merge)
            name: Node name
        """
        ports = input_ports or ['input1', 'input2']
        input_dict = {port: 'Any' for port in ports}
        
        super().__init__(
            name=name,
            inputs=input_dict,
            outputs={'merged': 'dict', 'keys': 'List[str]'}
        )
        self._merge_function = merge_function
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Merge all inputs."""
        if self._merge_function:
            merged = self._merge_function(inputs)
        else:
            merged = dict(inputs)
        
        return {
            'merged': merged,
            'keys': list(inputs.keys())
        }


class CustomNode(DataflowNode):
    """
    Node for creating custom analysis operations.
    
    This provides a simple way to create custom nodes without subclassing.
    
    Example:
        >>> node = CustomNode(
        ...     execute_fn=lambda inputs: {'result': inputs['tree'].total_samples * 2},
        ...     input_ports={'tree': 'PerformanceTree'},
        ...     output_ports={'result': 'int'},
        ...     name='DoubleCount'
        ... )
    """
    
    def __init__(
        self,
        execute_fn: Callable[[Dict[str, Any]], Dict[str, Any]],
        input_ports: Optional[Dict[str, str]] = None,
        output_ports: Optional[Dict[str, str]] = None,
        name: str = "Custom"
    ):
        """
        Initialize CustomNode.
        
        Args:
            execute_fn: Function to execute, takes inputs dict, returns outputs dict
            input_ports: Dictionary of input port names to type descriptions
            output_ports: Dictionary of output port names to type descriptions
            name: Node name
        """
        super().__init__(
            name=name,
            inputs=input_ports or {},
            outputs=output_ports or {'result': 'Any'}
        )
        self._execute_fn = execute_fn
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Execute the custom function."""
        return self._execute_fn(inputs)


class AggregateNode(DataflowNode):
    """
    Node for aggregating data from tree nodes.
    
    Inputs:
        - tree: PerformanceTree to aggregate
    
    Outputs:
        - aggregated: Aggregated result
        - groups: Dictionary of grouped results (if grouping is used)
    
    Example:
        >>> # Sum samples by library
        >>> node = AggregateNode(
        ...     group_by=lambda n: n.library_name,
        ...     aggregate_fn=lambda nodes: sum(n.total_samples for n in nodes)
        ... )
    """
    
    def __init__(
        self,
        aggregate_fn: Callable[[List[Any]], Any],
        group_by: Optional[Callable[[Any], str]] = None,
        filter_fn: Optional[Callable[[Any], bool]] = None,
        name: str = "Aggregate"
    ):
        """
        Initialize AggregateNode.
        
        Args:
            aggregate_fn: Function to aggregate a list of nodes
            group_by: Optional function to group nodes by a key
            filter_fn: Optional function to filter nodes before aggregation
            name: Node name
        """
        super().__init__(
            name=name,
            inputs={'tree': 'PerformanceTree'},
            outputs={'aggregated': 'Any', 'groups': 'dict'}
        )
        self._aggregate_fn = aggregate_fn
        self._group_by = group_by
        self._filter_fn = filter_fn
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Aggregate tree data."""
        self.validate_inputs(inputs)
        tree = inputs['tree']
        
        nodes = tree.all_nodes
        
        # Apply filter if provided
        if self._filter_fn:
            nodes = [n for n in nodes if self._filter_fn(n)]
        
        groups = {}
        
        if self._group_by:
            # Group nodes
            for node in nodes:
                key = self._group_by(node)
                if key not in groups:
                    groups[key] = []
                groups[key].append(node)
            
            # Aggregate each group
            aggregated = {key: self._aggregate_fn(group_nodes) 
                         for key, group_nodes in groups.items()}
        else:
            # Aggregate all nodes
            aggregated = self._aggregate_fn(nodes)
        
        return {
            'aggregated': aggregated,
            'groups': groups
        }
