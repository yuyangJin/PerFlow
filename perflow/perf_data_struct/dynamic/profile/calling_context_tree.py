'''
module calling context tree
'''

from typing import Dict, List, Optional, Tuple
from ...base import Tree, Node
from .sample_data import SampleData
from .perf_data import PerfData


'''
@class CallingContextTree
Represents the calling context tree built from profiling data
'''


class CallingContextTree(Tree):
    """
    CallingContextTree (CCT) represents the hierarchical calling contexts in profiling data.
    
    A CCT aggregates profiling samples by their call paths, creating a tree where each
    node represents a unique calling context (function + call stack). This enables
    analysis of performance in different calling contexts.
    
    Attributes:
        m_context_nodes: Dictionary mapping calling context to node ID
        m_node_metrics: Dictionary mapping node IDs to aggregated metrics
        m_node_sample_counts: Dictionary mapping node IDs to sample counts
    """
    
    def __init__(self) -> None:
        """Initialize a CallingContextTree."""
        super().__init__()
        self.m_context_nodes: Dict[Tuple[str, ...], int] = {}
        self.m_node_metrics: Dict[int, Dict[str, float]] = {}
        self.m_node_sample_counts: Dict[int, int] = {}
        self.m_next_node_id: int = 0
    
    def buildFromProfile(self, perf_data: PerfData) -> None:
        """
        Build the calling context tree from profiling data.
        
        Args:
            perf_data: PerfData object containing profiling samples
        """
        # Clear existing tree
        self.clear()
        self.m_context_nodes.clear()
        self.m_node_metrics.clear()
        self.m_node_sample_counts.clear()
        self.m_next_node_id = 0
        
        # Create root node
        root = Node(self._get_next_id(), "<root>", "context")
        self.setRoot(root)
        self.m_node_metrics[root.getId()] = {}
        self.m_node_sample_counts[root.getId()] = 0
        
        # Process each sample
        for sample in perf_data.getSamples():
            self._add_sample(sample)
    
    def _get_next_id(self) -> int:
        """Get the next available node ID."""
        node_id = self.m_next_node_id
        self.m_next_node_id += 1
        return node_id
    
    def _add_sample(self, sample: SampleData) -> None:
        """
        Add a sample to the calling context tree.
        
        Args:
            sample: SampleData to add
        """
        # Get call stack from sample
        call_stack = sample.getCallStack()
        if not call_stack:
            # If no call stack, use function name only
            func_name = sample.getFunctionName()
            if func_name:
                call_stack = [func_name]
            else:
                return
        
        # Call stacks are typically stored with deepest frame last (main -> ... -> leaf)
        # We want to build the tree from root (main) to leaf
        # So we use the call stack as-is
        
        # Navigate/create nodes along the call path
        current_parent_id = self.m_root.getId()
        
        for i, func_name in enumerate(call_stack):
            # Create context tuple (path from root to this function)
            context = tuple(call_stack[:i+1])
            
            # Check if this context already exists
            if context in self.m_context_nodes:
                node_id = self.m_context_nodes[context]
            else:
                # Create new node for this context
                node_id = self._get_next_id()
                node = Node(node_id, func_name, "context")
                node.setAttribute("context_path", context)
                node.setAttribute("depth", i + 1)
                
                self.addChild(current_parent_id, node)
                self.m_context_nodes[context] = node_id
                self.m_node_metrics[node_id] = {}
                self.m_node_sample_counts[node_id] = 0
            
            current_parent_id = node_id
        
        # Aggregate metrics for the leaf node (final context)
        leaf_id = current_parent_id
        self.m_node_sample_counts[leaf_id] += 1
        
        # Add metrics from sample
        for metric_name, value in sample.getMetrics().items():
            if metric_name not in self.m_node_metrics[leaf_id]:
                self.m_node_metrics[leaf_id][metric_name] = 0.0
            self.m_node_metrics[leaf_id][metric_name] += value
    
    def getNodeMetrics(self, node_id: int) -> Dict[str, float]:
        """
        Get aggregated metrics for a node.
        
        Args:
            node_id: ID of the node
            
        Returns:
            Dictionary of aggregated metrics
        """
        return self.m_node_metrics.get(node_id, {})
    
    def getNodeSampleCount(self, node_id: int) -> int:
        """
        Get the number of samples for a node.
        
        Args:
            node_id: ID of the node
            
        Returns:
            Sample count
        """
        return self.m_node_sample_counts.get(node_id, 0)
    
    def getInclusiveMetrics(self, node_id: int) -> Dict[str, float]:
        """
        Get inclusive metrics (including descendants) for a node.
        
        Args:
            node_id: ID of the node
            
        Returns:
            Dictionary of inclusive metrics
        """
        inclusive_metrics: Dict[str, float] = {}
        
        # Add this node's metrics
        for metric, value in self.m_node_metrics.get(node_id, {}).items():
            inclusive_metrics[metric] = value
        
        # Recursively add children's metrics
        for child_id in self.getChildren(node_id):
            child_inclusive = self.getInclusiveMetrics(child_id)
            for metric, value in child_inclusive.items():
                if metric not in inclusive_metrics:
                    inclusive_metrics[metric] = 0.0
                inclusive_metrics[metric] += value
        
        return inclusive_metrics
    
    def getExclusiveMetrics(self, node_id: int) -> Dict[str, float]:
        """
        Get exclusive metrics (excluding descendants) for a node.
        
        Args:
            node_id: ID of the node
            
        Returns:
            Dictionary of exclusive metrics
        """
        return self.m_node_metrics.get(node_id, {})
    
    def getHotPaths(self, metric: str = "cycles", top_n: int = 10) -> List[Tuple[Tuple[str, ...], float]]:
        """
        Get the hottest calling paths by a specific metric.
        
        Args:
            metric: Metric to sort by
            top_n: Number of top paths to return
            
        Returns:
            List of (context_path, metric_value) tuples
        """
        paths = []
        
        for context, node_id in self.m_context_nodes.items():
            metrics = self.m_node_metrics.get(node_id, {})
            if metric in metrics:
                paths.append((context, metrics[metric]))
        
        # Sort by metric value in descending order
        paths.sort(key=lambda x: x[1], reverse=True)
        
        return paths[:top_n]
    
    def getContextDepth(self) -> int:
        """
        Get the maximum depth of the calling context tree.
        
        Returns:
            Maximum depth
        """
        max_depth = 0
        for node_id in self.m_node_metrics.keys():
            depth = self.getDepth(node_id)
            max_depth = max(max_depth, depth)
        return max_depth
    
    def getFunctionStats(self, function_name: str) -> Dict[str, any]:
        """
        Get statistics for all contexts involving a specific function.
        
        Args:
            function_name: Name of the function
            
        Returns:
            Dictionary with statistics
        """
        total_samples = 0
        total_metrics: Dict[str, float] = {}
        contexts = []
        
        for context, node_id in self.m_context_nodes.items():
            if function_name in context:
                node = self.getNode(node_id)
                if node and node.getName() == function_name:
                    samples = self.m_node_sample_counts.get(node_id, 0)
                    total_samples += samples
                    contexts.append((context, samples))
                    
                    # Aggregate metrics
                    for metric, value in self.m_node_metrics.get(node_id, {}).items():
                        if metric not in total_metrics:
                            total_metrics[metric] = 0.0
                        total_metrics[metric] += value
        
        return {
            "total_samples": total_samples,
            "total_metrics": total_metrics,
            "contexts": contexts,
            "num_contexts": len(contexts)
        }
    
    def clear(self) -> None:
        """Clear the calling context tree."""
        super().clear()
        self.m_context_nodes.clear()
        self.m_node_metrics.clear()
        self.m_node_sample_counts.clear()
        self.m_next_node_id = 0
        self.m_root = None
