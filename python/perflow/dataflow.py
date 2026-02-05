"""
Dataflow Graph Abstraction for Performance Analysis

This module provides a flexible dataflow graph system for composing
performance analysis tasks. Analysis workflows are represented as
directed acyclic graphs (DAGs) where:

- Nodes represent analysis subtasks
- Edges represent data dependencies between subtasks
- The graph executor handles scheduling, optimization, and caching
"""

import threading
from concurrent.futures import ThreadPoolExecutor
from abc import ABC, abstractmethod
from typing import Any, Dict, List, Optional, Set, Callable
from collections import defaultdict, deque
import pickle
import hashlib


class Task(ABC):
    """
    Base class for analysis tasks.
    
    Tasks are the computational units in the dataflow graph.
    Each task processes input data and produces output results.
    """
    
    def __init__(self, name: str = None):
        """
        Initialize a task.
        
        Args:
            name: Optional name for the task (auto-generated if not provided)
        """
        self.name = name or f"{self.__class__.__name__}_{id(self)}"
        self._result_cache = None
        self._cache_valid = False
    
    @abstractmethod
    def execute(self, **inputs) -> Any:
        """
        Execute the task with given inputs.
        
        Args:
            **inputs: Input data from upstream tasks
            
        Returns:
            Task result
        """
        pass
    
    def get_cache_key(self, **inputs) -> str:
        """
        Generate a cache key based on task configuration and inputs.
        
        Args:
            **inputs: Input data
            
        Returns:
            Cache key string
        """
        try:
            # Create a deterministic hash of the task and its inputs
            data = {
                'task_type': self.__class__.__name__,
                'task_name': self.name,
                'inputs': inputs
            }
            serialized = pickle.dumps(data, protocol=pickle.HIGHEST_PROTOCOL)
            return hashlib.sha256(serialized).hexdigest()
        except Exception:
            # Fallback to a simple key if serialization fails
            return f"{self.name}_{hash(str(inputs))}"
    
    def invalidate_cache(self):
        """Invalidate the cached result."""
        self._cache_valid = False
        self._result_cache = None
    
    def __repr__(self):
        return f"<{self.__class__.__name__} name='{self.name}'>"


class Node:
    """
    Represents a node in the dataflow graph.
    
    A node wraps a Task and manages its execution context,
    including input/output connections and result caching.
    """
    
    def __init__(self, task: Task):
        """
        Initialize a node.
        
        Args:
            task: The task to execute
        """
        self.task = task
        self.inputs: Dict[str, 'Edge'] = {}
        self.outputs: List['Edge'] = []
        self._result = None
        self._executed = False
    
    def add_input(self, name: str, edge: 'Edge'):
        """Add an input edge to this node."""
        self.inputs[name] = edge
    
    def add_output(self, edge: 'Edge'):
        """Add an output edge from this node."""
        self.outputs.append(edge)
    
    def can_execute(self, executed_nodes: Set['Node']) -> bool:
        """
        Check if this node can be executed.
        
        Args:
            executed_nodes: Set of nodes that have already been executed
            
        Returns:
            True if all input dependencies are satisfied
        """
        for edge in self.inputs.values():
            if edge.source not in executed_nodes:
                return False
        return True
    
    def execute(self, results: Dict['Node', Any]) -> Any:
        """
        Execute this node's task.
        
        Args:
            results: Dictionary mapping nodes to their results
            
        Returns:
            Task result
        """
        if self._executed:
            return self._result
        
        # Gather inputs from upstream nodes
        input_data = {}
        for name, edge in self.inputs.items():
            if edge.source in results:
                input_data[name] = results[edge.source]
        
        # Execute the task
        self._result = self.task.execute(**input_data)
        self._executed = True
        return self._result
    
    def reset(self):
        """Reset execution state."""
        self._executed = False
        self._result = None
    
    def __repr__(self):
        return f"<Node task={self.task}>"


class Edge:
    """
    Represents an edge in the dataflow graph.
    
    An edge represents a data dependency between two nodes.
    """
    
    def __init__(self, source: Node, target: Node, name: str = None):
        """
        Initialize an edge.
        
        Args:
            source: Source node (produces data)
            target: Target node (consumes data)
            name: Optional name for the input parameter
        """
        self.source = source
        self.target = target
        self.name = name or "input"
    
    def __repr__(self):
        return f"<Edge {self.source.task.name} -> {self.target.task.name}>"


class Graph:
    """
    Dataflow graph for performance analysis workflows.
    
    The graph manages nodes, edges, and execution scheduling.
    It supports optimization techniques like graph fusion,
    parallel execution, lazy evaluation, and result caching.
    """
    
    def __init__(self, name: str = "workflow"):
        """
        Initialize a dataflow graph.
        
        Args:
            name: Name of the workflow
        """
        self.name = name
        self.nodes: List[Node] = []
        self.edges: List[Edge] = []
        self.cache: Dict[str, Any] = {}
        self.cache_enabled = True
        self.parallel_enabled = True
    
    def add_node(self, task: Task) -> Node:
        """
        Add a node to the graph.
        
        Args:
            task: Task to add
            
        Returns:
            The created node
        """
        node = Node(task)
        self.nodes.append(node)
        return node
    
    def add_edge(self, source: Node, target: Node, name: str = None) -> Edge:
        """
        Add an edge between two nodes.
        
        Args:
            source: Source node
            target: Target node
            name: Name of the input parameter
            
        Returns:
            The created edge
        """
        edge = Edge(source, target, name)
        self.edges.append(edge)
        source.add_output(edge)
        target.add_input(name, edge)
        return edge
    
    def topological_sort(self) -> List[Node]:
        """
        Perform topological sort on the graph.
        
        Returns:
            List of nodes in execution order
            
        Raises:
            ValueError: If the graph contains cycles
        """
        # Calculate in-degrees
        in_degree = {node: 0 for node in self.nodes}
        for edge in self.edges:
            in_degree[edge.target] += 1
        
        # Initialize queue with nodes that have no dependencies
        queue = deque([node for node in self.nodes if in_degree[node] == 0])
        result = []
        
        while queue:
            node = queue.popleft()
            result.append(node)
            
            # Reduce in-degree for dependent nodes
            for edge in node.outputs:
                in_degree[edge.target] -= 1
                if in_degree[edge.target] == 0:
                    queue.append(edge.target)
        
        if len(result) != len(self.nodes):
            raise ValueError("Graph contains cycles")
        
        return result
    
    def get_independent_groups(self, sorted_nodes: List[Node]) -> List[List[Node]]:
        """
        Group nodes that can be executed in parallel.
        
        Args:
            sorted_nodes: Topologically sorted nodes
            
        Returns:
            List of node groups (each group can be executed in parallel)
        """
        groups = []
        executed = set()
        
        while len(executed) < len(sorted_nodes):
            # Find nodes that can be executed now
            current_group = []
            for node in sorted_nodes:
                if node not in executed and node.can_execute(executed):
                    current_group.append(node)
            
            if not current_group:
                raise ValueError("Failed to schedule nodes")
            
            groups.append(current_group)
            executed.update(current_group)
        
        return groups
    
    def execute(self, lazy: bool = False, 
                inputs: Dict[Node, Any] = None) -> Dict[Node, Any]:
        """
        Execute the dataflow graph.
        
        Args:
            lazy: If True, only execute nodes needed for final outputs
            inputs: Optional initial inputs for source nodes
            
        Returns:
            Dictionary mapping nodes to their results
        """
        # Reset all nodes
        for node in self.nodes:
            node.reset()
        
        # Get execution order
        sorted_nodes = self.topological_sort()
        
        # Initialize results with provided inputs
        results = inputs.copy() if inputs else {}
        
        if self.parallel_enabled:
            # Execute in parallel groups
            groups = self.get_independent_groups(sorted_nodes)
            
            for group in groups:
                if len(group) == 1:
                    # Single node - execute directly
                    node = group[0]
                    if node not in results:
                        results[node] = node.execute(results)
                else:
                    # Multiple nodes - execute in parallel using threads
                    # (for I/O-bound tasks; could use multiprocessing for CPU-bound)
                    def execute_node(n):
                        return (n, n.execute(results))
                    
                    with ThreadPoolExecutor(max_workers=len(group)) as executor:
                        futures = [executor.submit(execute_node, node) for node in group]
                        for future in futures:
                            node, result = future.result()
                            results[node] = result
        else:
            # Sequential execution
            for node in sorted_nodes:
                if node not in results:
                    results[node] = node.execute(results)
        
        return results
    
    def get_output_nodes(self) -> List[Node]:
        """
        Get nodes that produce final outputs (no outgoing edges).
        
        Returns:
            List of output nodes
        """
        nodes_with_outputs = {edge.source for edge in self.edges}
        return [node for node in self.nodes if node not in nodes_with_outputs]
    
    def visualize(self) -> str:
        """
        Generate a DOT representation of the graph for visualization.
        
        Returns:
            DOT format string
        """
        lines = ["digraph workflow {"]
        lines.append("  rankdir=LR;")
        lines.append("  node [shape=box, style=rounded];")
        
        # Add nodes
        for i, node in enumerate(self.nodes):
            label = node.task.name
            lines.append(f'  n{i} [label="{label}"];')
        
        # Add edges
        node_to_id = {node: i for i, node in enumerate(self.nodes)}
        for edge in self.edges:
            src_id = node_to_id[edge.source]
            tgt_id = node_to_id[edge.target]
            label = edge.name if edge.name else ""
            lines.append(f'  n{src_id} -> n{tgt_id} [label="{label}"];')
        
        lines.append("}")
        return "\n".join(lines)
    
    def save_visualization(self, filepath: str):
        """
        Save graph visualization to a file.
        
        Args:
            filepath: Output file path (should end with .dot or .png)
        """
        dot_content = self.visualize()
        
        if filepath.endswith('.dot'):
            with open(filepath, 'w') as f:
                f.write(dot_content)
        elif filepath.endswith('.png'):
            # Try to use graphviz to render
            try:
                import subprocess
                import tempfile
                with tempfile.NamedTemporaryFile(mode='w', suffix='.dot', delete=False) as f:
                    f.write(dot_content)
                    dot_file = f.name
                subprocess.run(['dot', '-Tpng', dot_file, '-o', filepath], check=True)
            except Exception as e:
                raise RuntimeError(f"Failed to generate PNG: {e}")
        else:
            raise ValueError("Filepath must end with .dot or .png")
    
    def __repr__(self):
        return f"<Graph name='{self.name}' nodes={len(self.nodes)} edges={len(self.edges)}>"
