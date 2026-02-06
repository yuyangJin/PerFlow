# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Graph Executors for PerFlow Dataflow Framework

This module provides different execution strategies for dataflow graphs:
- GraphExecutor: Basic sequential executor (default)
- ParallelExecutor: Parallel execution of independent nodes
- CachingExecutor: Executor with result caching for lazy evaluation
"""

from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Any, Callable, Dict, List, Optional, Set
import threading
import time

from .graph import DataflowGraph, DataflowNode, NodeState


class GraphExecutor:
    """
    Sequential executor that runs nodes one at a time in topological order.
    
    This is the default executor and is useful for debugging or when
    parallelism is not needed.
    
    Example:
        >>> executor = GraphExecutor()
        >>> results = graph.execute(executor=executor)
    """
    
    def __init__(self):
        """Initialize the executor."""
        self._execution_times: Dict[str, float] = {}
        self._progress_callback: Optional[Callable[[int, int, str], None]] = None
    
    def set_progress_callback(
        self, 
        callback: Callable[[int, int, str], None]
    ) -> 'GraphExecutor':
        """
        Set a progress callback function.
        
        Args:
            callback: Function(completed, total, current_node_name)
        
        Returns:
            Self for method chaining
        """
        self._progress_callback = callback
        return self
    
    def _report_progress(self, completed: int, total: int, current: str) -> None:
        """Report execution progress if callback is set."""
        if self._progress_callback:
            self._progress_callback(completed, total, current)
    
    def execute(
        self, 
        graph: DataflowGraph,
        **kwargs
    ) -> Dict[str, Dict[str, Any]]:
        """
        Execute the dataflow graph sequentially.
        
        Args:
            graph: The dataflow graph to execute
            **kwargs: Additional execution parameters
        
        Returns:
            Dictionary mapping node IDs to their output dictionaries
        """
        # Reset graph state
        graph.reset()
        
        # Validate graph
        graph.validate()
        
        # Get execution order
        execution_order = graph.topological_sort()
        total_nodes = len(execution_order)
        
        results: Dict[str, Dict[str, Any]] = {}
        self._execution_times.clear()
        
        for i, node in enumerate(execution_order):
            self._report_progress(i, total_nodes, node.name)
            
            # Collect inputs from predecessor results
            inputs = self._collect_inputs(node, graph, results)
            
            # Execute the node
            node.state = NodeState.RUNNING
            start_time = time.time()
            
            try:
                output = node.execute(inputs)
                node.result = output
                node.state = NodeState.COMPLETED
                results[node.node_id] = output
            except Exception as e:
                node.error = e
                node.state = NodeState.FAILED
                raise RuntimeError(f"Node '{node.name}' failed: {e}") from e
            finally:
                self._execution_times[node.node_id] = time.time() - start_time
        
        self._report_progress(total_nodes, total_nodes, "Complete")
        
        return results
    
    def _collect_inputs(
        self,
        node: DataflowNode,
        graph: DataflowGraph,
        results: Dict[str, Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Collect inputs for a node from predecessor outputs."""
        inputs: Dict[str, Any] = {}
        
        for edge in graph.edges:
            if edge.target_node.node_id == node.node_id:
                source_output = results.get(edge.source_node.node_id, {})
                if edge.source_port in source_output:
                    inputs[edge.target_port] = source_output[edge.source_port]
        
        return inputs
    
    def get_execution_times(self) -> Dict[str, float]:
        """Get execution time for each node (in seconds)."""
        return self._execution_times.copy()


# SequentialExecutor is an alias for GraphExecutor for backwards compatibility
SequentialExecutor = GraphExecutor


class ParallelExecutor(GraphExecutor):
    """
    Parallel executor that runs independent nodes concurrently.
    
    Uses thread pool to execute nodes that have no dependencies on each other
    simultaneously, improving performance for graphs with parallel branches.
    
    Example:
        >>> executor = ParallelExecutor(max_workers=4)
        >>> results = graph.execute(executor=executor)
    """
    
    def __init__(self, max_workers: Optional[int] = None):
        """
        Initialize the parallel executor.
        
        Args:
            max_workers: Maximum number of worker threads (default: CPU count)
        """
        super().__init__()
        self._max_workers = max_workers
        self._lock = threading.Lock()
    
    def execute(
        self, 
        graph: DataflowGraph,
        **kwargs
    ) -> Dict[str, Dict[str, Any]]:
        """Execute nodes in parallel where possible."""
        # Reset graph state
        graph.reset()
        
        # Validate graph
        graph.validate()
        
        # Get parallel execution groups
        parallel_groups = graph.get_parallel_groups()
        total_nodes = len(graph.nodes)
        completed = 0
        
        results: Dict[str, Dict[str, Any]] = {}
        self._execution_times.clear()
        
        with ThreadPoolExecutor(max_workers=self._max_workers) as executor:
            for group in parallel_groups:
                if len(group) == 1:
                    # Single node - execute directly
                    node = group[0]
                    self._report_progress(completed, total_nodes, node.name)
                    self._execute_node(node, graph, results)
                    completed += 1
                else:
                    # Multiple nodes - execute in parallel
                    futures = {}
                    for node in group:
                        inputs = self._collect_inputs(node, graph, results)
                        future = executor.submit(self._execute_single, node, inputs)
                        futures[future] = node
                    
                    # Wait for all nodes in group to complete
                    for future in as_completed(futures):
                        node = futures[future]
                        self._report_progress(completed, total_nodes, node.name)
                        
                        try:
                            output, exec_time = future.result()
                            with self._lock:
                                results[node.node_id] = output
                                self._execution_times[node.node_id] = exec_time
                            completed += 1
                        except Exception as e:
                            raise RuntimeError(f"Node '{node.name}' failed: {e}") from e
        
        self._report_progress(total_nodes, total_nodes, "Complete")
        return results
    
    def _execute_single(
        self, 
        node: DataflowNode, 
        inputs: Dict[str, Any]
    ) -> tuple:
        """Execute a single node and return output with timing."""
        node.state = NodeState.RUNNING
        start_time = time.time()
        
        try:
            output = node.execute(inputs)
            node.result = output
            node.state = NodeState.COMPLETED
            return output, time.time() - start_time
        except Exception as e:
            node.error = e
            node.state = NodeState.FAILED
            raise
    
    def _execute_node(
        self,
        node: DataflowNode,
        graph: DataflowGraph,
        results: Dict[str, Dict[str, Any]]
    ) -> None:
        """Execute a node and store its results."""
        inputs = self._collect_inputs(node, graph, results)
        
        node.state = NodeState.RUNNING
        start_time = time.time()
        
        try:
            output = node.execute(inputs)
            node.result = output
            node.state = NodeState.COMPLETED
            results[node.node_id] = output
        except Exception as e:
            node.error = e
            node.state = NodeState.FAILED
            raise RuntimeError(f"Node '{node.name}' failed: {e}") from e
        finally:
            self._execution_times[node.node_id] = time.time() - start_time
    
    def _collect_inputs(
        self,
        node: DataflowNode,
        graph: DataflowGraph,
        results: Dict[str, Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Collect inputs for a node from predecessor outputs (thread-safe)."""
        inputs: Dict[str, Any] = {}
        
        with self._lock:
            for edge in graph.edges:
                if edge.target_node.node_id == node.node_id:
                    source_output = results.get(edge.source_node.node_id, {})
                    if edge.source_port in source_output:
                        inputs[edge.target_port] = source_output[edge.source_port]
        
        return inputs


class CachingExecutor(GraphExecutor):
    """
    Executor with result caching for lazy evaluation optimization.
    
    This executor caches results based on node configurations and inputs,
    allowing re-execution of graphs to skip unchanged computations.
    
    Example:
        >>> executor = CachingExecutor()
        >>> 
        >>> # First execution computes everything
        >>> results1 = graph.execute(executor=executor)
        >>> 
        >>> # Second execution uses cached results where possible
        >>> results2 = graph.execute(executor=executor)
    """
    
    def __init__(
        self, 
        max_cache_size: int = 100,
        parallel: bool = False,
        max_workers: Optional[int] = None
    ):
        """
        Initialize the caching executor.
        
        Args:
            max_cache_size: Maximum number of cached results to keep
            parallel: Whether to use parallel execution for non-cached nodes
            max_workers: Maximum workers if parallel is True
        """
        super().__init__()
        self._cache: Dict[str, Dict[str, Any]] = {}
        self._cache_order: List[str] = []  # LRU tracking
        self._max_cache_size = max_cache_size
        self._parallel = parallel
        self._max_workers = max_workers
        self._cache_hits = 0
        self._cache_misses = 0
        self._lock = threading.Lock()
    
    def execute(
        self, 
        graph: DataflowGraph,
        force_recompute: bool = False,
        **kwargs
    ) -> Dict[str, Dict[str, Any]]:
        """
        Execute the graph with caching.
        
        Args:
            graph: The dataflow graph to execute
            force_recompute: If True, ignore cache and recompute everything
        
        Returns:
            Dictionary mapping node IDs to their output dictionaries
        """
        # Reset graph state
        graph.reset()
        
        # Validate graph
        graph.validate()
        
        # Get execution order
        execution_order = graph.topological_sort()
        total_nodes = len(execution_order)
        
        results: Dict[str, Dict[str, Any]] = {}
        self._execution_times.clear()
        
        for i, node in enumerate(execution_order):
            self._report_progress(i, total_nodes, node.name)
            
            # Collect inputs
            inputs = self._collect_inputs(node, graph, results)
            
            # Check cache
            cache_key = node.get_cache_key(inputs)
            
            if not force_recompute and cache_key in self._cache:
                # Cache hit
                with self._lock:
                    self._cache_hits += 1
                    # Update LRU order
                    if cache_key in self._cache_order:
                        self._cache_order.remove(cache_key)
                    self._cache_order.append(cache_key)
                
                node.state = NodeState.CACHED
                output = self._cache[cache_key]
                node.result = output
                results[node.node_id] = output
                self._execution_times[node.node_id] = 0.0  # Cached - no execution time
            else:
                # Cache miss - execute
                with self._lock:
                    self._cache_misses += 1
                
                node.state = NodeState.RUNNING
                start_time = time.time()
                
                try:
                    output = node.execute(inputs)
                    node.result = output
                    node.state = NodeState.COMPLETED
                    results[node.node_id] = output
                    
                    # Store in cache
                    self._store_cache(cache_key, output)
                except Exception as e:
                    node.error = e
                    node.state = NodeState.FAILED
                    raise RuntimeError(f"Node '{node.name}' failed: {e}") from e
                finally:
                    self._execution_times[node.node_id] = time.time() - start_time
        
        self._report_progress(total_nodes, total_nodes, "Complete")
        return results
    
    def _store_cache(self, key: str, value: Dict[str, Any]) -> None:
        """Store a result in cache with LRU eviction."""
        with self._lock:
            if key in self._cache:
                self._cache_order.remove(key)
            elif len(self._cache) >= self._max_cache_size:
                # Evict oldest entry
                oldest = self._cache_order.pop(0)
                del self._cache[oldest]
            
            self._cache[key] = value
            self._cache_order.append(key)
    
    def _collect_inputs(
        self,
        node: DataflowNode,
        graph: DataflowGraph,
        results: Dict[str, Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Collect inputs for a node from predecessor outputs."""
        inputs: Dict[str, Any] = {}
        
        for edge in graph.edges:
            if edge.target_node.node_id == node.node_id:
                source_output = results.get(edge.source_node.node_id, {})
                if edge.source_port in source_output:
                    inputs[edge.target_port] = source_output[edge.source_port]
        
        return inputs
    
    def clear_cache(self) -> None:
        """Clear all cached results."""
        with self._lock:
            self._cache.clear()
            self._cache_order.clear()
            self._cache_hits = 0
            self._cache_misses = 0
    
    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics."""
        with self._lock:
            total = self._cache_hits + self._cache_misses
            hit_rate = self._cache_hits / total if total > 0 else 0.0
            
            return {
                'cache_hits': self._cache_hits,
                'cache_misses': self._cache_misses,
                'hit_rate': hit_rate,
                'cache_size': len(self._cache),
                'max_cache_size': self._max_cache_size,
            }
