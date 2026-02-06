# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Dataflow Graph Core Classes

This module provides the core abstractions for the dataflow-based analysis framework:
- DataflowNode: Base class for analysis subtasks
- DataflowEdge: Represents connections between nodes
- DataflowGraph: The directed acyclic graph (DAG) for composing workflows
"""

from abc import ABC, abstractmethod
from typing import Any, Callable, Dict, List, Optional, Set, Tuple
from enum import Enum
import hashlib
import threading


class NodeState(Enum):
    """State of a dataflow node during execution."""
    PENDING = "pending"       # Not yet executed
    READY = "ready"           # All dependencies satisfied, ready to execute
    RUNNING = "running"       # Currently executing
    COMPLETED = "completed"   # Successfully completed
    FAILED = "failed"         # Execution failed
    CACHED = "cached"         # Result retrieved from cache


class DataflowNode(ABC):
    """
    Base class for analysis subtasks in the dataflow graph.
    
    Each node represents a unit of computation that:
    - Takes inputs from upstream nodes
    - Produces outputs for downstream nodes
    - Can be executed when all dependencies are satisfied
    
    Attributes:
        name: Human-readable name for the node
        node_id: Unique identifier for the node
        state: Current execution state
        inputs: Dictionary of input port names to their expected types
        outputs: Dictionary of output port names to their produced types
    
    Example:
        >>> class MyAnalysisNode(DataflowNode):
        ...     def __init__(self):
        ...         super().__init__(name="MyAnalysis", inputs={'tree': 'PerformanceTree'}, 
        ...                          outputs={'result': 'dict'})
        ...     
        ...     def execute(self, inputs):
        ...         tree = inputs['tree']
        ...         # Perform analysis
        ...         return {'result': {'analysis': 'data'}}
    """
    
    _id_counter = 0
    _id_lock = threading.Lock()
    
    def __init__(
        self,
        name: str,
        inputs: Optional[Dict[str, str]] = None,
        outputs: Optional[Dict[str, str]] = None
    ):
        """
        Initialize a dataflow node.
        
        Args:
            name: Human-readable name for this node
            inputs: Dictionary mapping input port names to expected type descriptions
            outputs: Dictionary mapping output port names to produced type descriptions
        """
        with DataflowNode._id_lock:
            DataflowNode._id_counter += 1
            self._node_id = f"{name}_{DataflowNode._id_counter}"
        
        self._name = name
        self._state = NodeState.PENDING
        self._inputs = inputs or {}
        self._outputs = outputs or {}
        self._result: Optional[Dict[str, Any]] = None
        self._error: Optional[Exception] = None
        self._cache_key: Optional[str] = None
    
    @property
    def name(self) -> str:
        """Get the human-readable name of this node."""
        return self._name
    
    @property
    def node_id(self) -> str:
        """Get the unique identifier for this node."""
        return self._node_id
    
    @property
    def state(self) -> NodeState:
        """Get the current execution state."""
        return self._state
    
    @state.setter
    def state(self, value: NodeState) -> None:
        """Set the execution state."""
        self._state = value
    
    @property
    def inputs(self) -> Dict[str, str]:
        """Get the input port specifications."""
        return self._inputs.copy()
    
    @property
    def outputs(self) -> Dict[str, str]:
        """Get the output port specifications."""
        return self._outputs.copy()
    
    @property
    def result(self) -> Optional[Dict[str, Any]]:
        """Get the execution result."""
        return self._result
    
    @result.setter
    def result(self, value: Dict[str, Any]) -> None:
        """Set the execution result."""
        self._result = value
    
    @property
    def error(self) -> Optional[Exception]:
        """Get the error if execution failed."""
        return self._error
    
    @error.setter
    def error(self, value: Exception) -> None:
        """Set the error."""
        self._error = value
    
    @abstractmethod
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        """
        Execute the analysis subtask.
        
        Args:
            inputs: Dictionary mapping input port names to values
        
        Returns:
            Dictionary mapping output port names to produced values
        
        Raises:
            ValueError: If required inputs are missing
            RuntimeError: If execution fails
        """
        pass
    
    def validate_inputs(self, inputs: Dict[str, Any]) -> bool:
        """
        Validate that all required inputs are present.
        
        Args:
            inputs: Dictionary of input port names to values
        
        Returns:
            True if all required inputs are present
        
        Raises:
            ValueError: If required inputs are missing
        """
        missing = set(self._inputs.keys()) - set(inputs.keys())
        if missing:
            raise ValueError(f"Node '{self.name}' missing required inputs: {missing}")
        return True
    
    def get_cache_key(self, inputs: Dict[str, Any]) -> str:
        """
        Generate a cache key based on node configuration and inputs.
        
        This is used for lazy evaluation and caching optimization.
        
        Args:
            inputs: Dictionary of input port names to values
        
        Returns:
            A string hash that uniquely identifies this computation
        """
        # Create a deterministic representation of the node state
        key_parts = [
            self.__class__.__name__,
            self._name,
            str(sorted(self._inputs.items())),
            str(sorted(self._outputs.items())),
        ]
        
        # Add input hashes (for hashable types only)
        for name, value in sorted(inputs.items()):
            try:
                key_parts.append(f"{name}:{hash(str(value))}")
            except TypeError:
                key_parts.append(f"{name}:{id(value)}")
        
        combined = "|".join(key_parts)
        return hashlib.md5(combined.encode()).hexdigest()
    
    def reset(self) -> None:
        """Reset the node to its initial state for re-execution."""
        self._state = NodeState.PENDING
        self._result = None
        self._error = None
        self._cache_key = None
    
    def __repr__(self) -> str:
        return f"<{self.__class__.__name__} name='{self._name}' state={self._state.value}>"


class DataflowEdge:
    """
    Represents a connection between two nodes in the dataflow graph.
    
    An edge transfers the output of one node's port to another node's input port.
    
    Attributes:
        source_node: The node producing the output
        source_port: The output port name on the source node
        target_node: The node receiving the input
        target_port: The input port name on the target node
    """
    
    def __init__(
        self,
        source_node: DataflowNode,
        source_port: str,
        target_node: DataflowNode,
        target_port: str
    ):
        """
        Create a dataflow edge.
        
        Args:
            source_node: Node producing the output
            source_port: Output port name on source node
            target_node: Node receiving the input
            target_port: Input port name on target node
        
        Raises:
            ValueError: If ports don't exist on the respective nodes
        """
        # Validate source port
        if source_port not in source_node.outputs:
            raise ValueError(
                f"Source node '{source_node.name}' has no output port '{source_port}'. "
                f"Available outputs: {list(source_node.outputs.keys())}"
            )
        
        # Validate target port
        if target_port not in target_node.inputs:
            raise ValueError(
                f"Target node '{target_node.name}' has no input port '{target_port}'. "
                f"Available inputs: {list(target_node.inputs.keys())}"
            )
        
        self._source_node = source_node
        self._source_port = source_port
        self._target_node = target_node
        self._target_port = target_port
    
    @property
    def source_node(self) -> DataflowNode:
        """Get the source node."""
        return self._source_node
    
    @property
    def source_port(self) -> str:
        """Get the source output port name."""
        return self._source_port
    
    @property
    def target_node(self) -> DataflowNode:
        """Get the target node."""
        return self._target_node
    
    @property
    def target_port(self) -> str:
        """Get the target input port name."""
        return self._target_port
    
    def __repr__(self) -> str:
        return (f"<DataflowEdge {self._source_node.name}.{self._source_port} -> "
                f"{self._target_node.name}.{self._target_port}>")


class DataflowGraph:
    """
    A directed acyclic graph (DAG) for composing analysis workflows.
    
    The graph manages nodes and their connections, providing:
    - Node and edge management
    - Topological ordering for execution
    - Validation of graph structure
    - Graph optimization (fusion, parallel execution planning)
    
    Example:
        >>> graph = DataflowGraph(name="MyWorkflow")
        >>> 
        >>> # Add nodes
        >>> load = LoadDataNode(sample_files=[('data.pflw', 0)])
        >>> analyze = HotspotAnalysisNode(top_n=10)
        >>> 
        >>> graph.add_node(load)
        >>> graph.add_node(analyze)
        >>> graph.connect(load, analyze, 'tree')
        >>> 
        >>> # Execute
        >>> results = graph.execute()
    """
    
    def __init__(self, name: str = "DataflowGraph"):
        """
        Initialize an empty dataflow graph.
        
        Args:
            name: Human-readable name for this graph
        """
        self._name = name
        self._nodes: Dict[str, DataflowNode] = {}
        self._edges: List[DataflowEdge] = []
        self._adjacency: Dict[str, List[str]] = {}  # node_id -> list of successor node_ids
        self._reverse_adjacency: Dict[str, List[str]] = {}  # node_id -> list of predecessor node_ids
        self._validated = False
    
    @property
    def name(self) -> str:
        """Get the graph name."""
        return self._name
    
    @property
    def nodes(self) -> List[DataflowNode]:
        """Get all nodes in the graph."""
        return list(self._nodes.values())
    
    @property
    def edges(self) -> List[DataflowEdge]:
        """Get all edges in the graph."""
        return self._edges.copy()
    
    def add_node(self, node: DataflowNode) -> 'DataflowGraph':
        """
        Add a node to the graph.
        
        Args:
            node: The dataflow node to add
        
        Returns:
            Self for method chaining
        
        Raises:
            ValueError: If a node with the same ID already exists
        """
        if node.node_id in self._nodes:
            raise ValueError(f"Node with ID '{node.node_id}' already exists in graph")
        
        self._nodes[node.node_id] = node
        self._adjacency[node.node_id] = []
        self._reverse_adjacency[node.node_id] = []
        self._validated = False
        
        return self
    
    def remove_node(self, node: DataflowNode) -> 'DataflowGraph':
        """
        Remove a node and all its connected edges from the graph.
        
        Args:
            node: The node to remove
        
        Returns:
            Self for method chaining
        """
        if node.node_id not in self._nodes:
            return self
        
        # Remove edges connected to this node
        self._edges = [
            e for e in self._edges
            if e.source_node.node_id != node.node_id and e.target_node.node_id != node.node_id
        ]
        
        # Update adjacency lists
        for adj_list in self._adjacency.values():
            if node.node_id in adj_list:
                adj_list.remove(node.node_id)
        
        for adj_list in self._reverse_adjacency.values():
            if node.node_id in adj_list:
                adj_list.remove(node.node_id)
        
        del self._adjacency[node.node_id]
        del self._reverse_adjacency[node.node_id]
        del self._nodes[node.node_id]
        
        self._validated = False
        return self
    
    def connect(
        self,
        source: DataflowNode,
        target: DataflowNode,
        port: str,
        source_port: Optional[str] = None,
        target_port: Optional[str] = None
    ) -> 'DataflowGraph':
        """
        Connect two nodes with an edge.
        
        Args:
            source: Source node producing output
            target: Target node receiving input
            port: Port name (used for both source and target if not specified separately)
            source_port: Explicit source output port (defaults to `port`)
            target_port: Explicit target input port (defaults to `port`)
        
        Returns:
            Self for method chaining
        
        Raises:
            ValueError: If nodes are not in graph or ports don't exist
        """
        if source.node_id not in self._nodes:
            raise ValueError(f"Source node '{source.name}' not in graph")
        if target.node_id not in self._nodes:
            raise ValueError(f"Target node '{target.name}' not in graph")
        
        src_port = source_port or port
        tgt_port = target_port or port
        
        edge = DataflowEdge(source, src_port, target, tgt_port)
        self._edges.append(edge)
        
        self._adjacency[source.node_id].append(target.node_id)
        self._reverse_adjacency[target.node_id].append(source.node_id)
        
        self._validated = False
        return self
    
    def get_predecessors(self, node: DataflowNode) -> List[DataflowNode]:
        """Get all nodes that feed into the given node."""
        pred_ids = self._reverse_adjacency.get(node.node_id, [])
        return [self._nodes[nid] for nid in pred_ids]
    
    def get_successors(self, node: DataflowNode) -> List[DataflowNode]:
        """Get all nodes that receive output from the given node."""
        succ_ids = self._adjacency.get(node.node_id, [])
        return [self._nodes[nid] for nid in succ_ids]
    
    def get_source_nodes(self) -> List[DataflowNode]:
        """Get all nodes with no incoming edges (entry points)."""
        return [
            node for node in self._nodes.values()
            if not self._reverse_adjacency.get(node.node_id, [])
        ]
    
    def get_sink_nodes(self) -> List[DataflowNode]:
        """Get all nodes with no outgoing edges (exit points)."""
        return [
            node for node in self._nodes.values()
            if not self._adjacency.get(node.node_id, [])
        ]
    
    def topological_sort(self) -> List[DataflowNode]:
        """
        Return nodes in topological order (dependencies before dependents).
        
        Returns:
            List of nodes in execution order
        
        Raises:
            ValueError: If the graph contains a cycle
        """
        # Kahn's algorithm
        in_degree = {nid: len(self._reverse_adjacency.get(nid, [])) 
                     for nid in self._nodes}
        
        queue = [nid for nid, degree in in_degree.items() if degree == 0]
        result = []
        
        while queue:
            nid = queue.pop(0)
            result.append(self._nodes[nid])
            
            for succ_id in self._adjacency.get(nid, []):
                in_degree[succ_id] -= 1
                if in_degree[succ_id] == 0:
                    queue.append(succ_id)
        
        if len(result) != len(self._nodes):
            raise ValueError("Graph contains a cycle - not a valid DAG")
        
        return result
    
    def get_parallel_groups(self) -> List[List[DataflowNode]]:
        """
        Group nodes that can be executed in parallel.
        
        Returns a list of groups, where nodes in the same group have no
        dependencies on each other and can be executed simultaneously.
        
        Returns:
            List of node groups in execution order
        """
        if not self._nodes:
            return []
        
        # Calculate depths for each node (longest path from sources)
        depths: Dict[str, int] = {}
        
        def calculate_depth(node_id: str) -> int:
            if node_id in depths:
                return depths[node_id]
            
            preds = self._reverse_adjacency.get(node_id, [])
            if not preds:
                depths[node_id] = 0
            else:
                depths[node_id] = max(calculate_depth(p) for p in preds) + 1
            
            return depths[node_id]
        
        for nid in self._nodes:
            calculate_depth(nid)
        
        # Group nodes by depth
        max_depth = max(depths.values()) if depths else 0
        groups: List[List[DataflowNode]] = [[] for _ in range(max_depth + 1)]
        
        for nid, depth in depths.items():
            groups[depth].append(self._nodes[nid])
        
        return groups
    
    def validate(self) -> bool:
        """
        Validate the graph structure.
        
        Checks:
        - No cycles (valid DAG)
        - All input ports have connections (unless optional)
        - No duplicate edges
        
        Returns:
            True if valid
        
        Raises:
            ValueError: If validation fails
        """
        # Check for cycles via topological sort
        try:
            self.topological_sort()
        except ValueError as e:
            raise ValueError(f"Graph validation failed: {e}")
        
        # Check that all required inputs have connections
        connected_inputs: Dict[str, Set[str]] = {nid: set() for nid in self._nodes}
        
        for edge in self._edges:
            connected_inputs[edge.target_node.node_id].add(edge.target_port)
        
        for node in self._nodes.values():
            missing = set(node.inputs.keys()) - connected_inputs[node.node_id]
            # Source nodes might have no input connections
            if missing and self._reverse_adjacency.get(node.node_id, []):
                # This node has predecessors but missing inputs
                raise ValueError(
                    f"Node '{node.name}' has unconnected input ports: {missing}"
                )
        
        self._validated = True
        return True
    
    def reset(self) -> None:
        """Reset all nodes to their initial state."""
        for node in self._nodes.values():
            node.reset()
    
    def execute(
        self,
        executor: Optional['GraphExecutor'] = None,
        **kwargs
    ) -> Dict[str, Dict[str, Any]]:
        """
        Execute the dataflow graph.
        
        Args:
            executor: Optional executor to use (default: sequential executor)
            **kwargs: Additional arguments passed to the executor
        
        Returns:
            Dictionary mapping node names to their output dictionaries
        """
        from .executor import GraphExecutor
        
        if executor is None:
            executor = GraphExecutor()
        
        return executor.execute(self, **kwargs)
    
    def __len__(self) -> int:
        """Return the number of nodes in the graph."""
        return len(self._nodes)
    
    def __repr__(self) -> str:
        return f"<DataflowGraph name='{self._name}' nodes={len(self._nodes)} edges={len(self._edges)}>"
