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
    
    def visualize(self, output_file: str = "cct_visualization.png", 
                  view_type: str = "tree", metric: str = "cycles",
                  figsize: Tuple[int, int] = (12, 8), dpi: int = 100) -> None:
        """
        Visualize the calling context tree.
        
        Args:
            output_file: Path to save the visualization
            view_type: Type of visualization - "tree" for top-down tree view, 
                      "ring" for ring/sunburst chart
            metric: Metric to use for sizing/coloring (default: "cycles")
            figsize: Figure size in inches (width, height)
            dpi: DPI for the output image
        """
        try:
            import matplotlib.pyplot as plt
            import matplotlib.patches as mpatches
            from matplotlib.patches import Rectangle, Wedge
            import numpy as np
        except ImportError:
            raise ImportError(
                "matplotlib is required for visualization. "
                "Install it with: pip install matplotlib"
            )
        
        if not self.m_root:
            raise ValueError("Cannot visualize empty CCT. Build from profile first.")
        
        if view_type == "tree":
            self._visualize_tree(output_file, metric, figsize, dpi)
        elif view_type == "ring":
            self._visualize_ring(output_file, metric, figsize, dpi)
        else:
            raise ValueError(f"Unknown view_type: {view_type}. Use 'tree' or 'ring'")
    
    def _visualize_tree(self, output_file: str, metric: str, 
                       figsize: Tuple[int, int], dpi: int) -> None:
        """
        Create a top-down tree visualization.
        
        Args:
            output_file: Path to save the visualization
            metric: Metric to use for coloring
            figsize: Figure size
            dpi: DPI for output
        """
        import matplotlib.pyplot as plt
        import matplotlib.patches as mpatches
        from matplotlib.patches import Rectangle
        import numpy as np
        
        # Calculate positions for all nodes
        positions = {}
        widths = {}
        
        def calculate_positions(node_id: int, x: float, y: float, width: float) -> float:
            """Calculate positions for tree layout."""
            positions[node_id] = (x + width / 2, y)
            widths[node_id] = width
            
            children = self.getChildren(node_id)
            if not children:
                return width
            
            # Calculate total metric for children to determine width distribution
            child_metrics = []
            for child_id in children:
                inclusive = self.getInclusiveMetrics(child_id)
                child_metrics.append(inclusive.get(metric, 1.0))
            
            total_metric = sum(child_metrics) if sum(child_metrics) > 0 else len(children)
            
            # Position children
            current_x = x
            for child_id, child_metric in zip(children, child_metrics):
                child_width = width * (child_metric / total_metric)
                calculate_positions(child_id, current_x, y - 1, child_width)
                current_x += child_width
            
            return width
        
        # Start from root
        calculate_positions(self.m_root.getId(), 0, 0, 10)
        
        # Create figure
        fig, ax = plt.subplots(figsize=figsize, dpi=dpi)
        
        # Get max metric for color normalization
        max_metric = 0
        for node_id in self.m_node_metrics.keys():
            inclusive = self.getInclusiveMetrics(node_id)
            max_metric = max(max_metric, inclusive.get(metric, 0))
        
        # Draw nodes and edges
        for node_id, (x, y) in positions.items():
            node = self.getNode(node_id)
            if not node:
                continue
            
            # Get metrics for coloring
            inclusive = self.getInclusiveMetrics(node_id)
            node_metric = inclusive.get(metric, 0)
            
            # Color based on metric value (red = high, yellow = medium, green = low)
            if max_metric > 0:
                ratio = node_metric / max_metric
                color = plt.cm.YlOrRd(ratio)
            else:
                color = 'lightblue'
            
            # Draw node
            width = widths.get(node_id, 0.5)
            rect = Rectangle((x - width/2, y - 0.3), width, 0.6, 
                           facecolor=color, edgecolor='black', linewidth=1)
            ax.add_patch(rect)
            
            # Add label
            label = node.getName()
            if len(label) > 15:
                label = label[:12] + "..."
            
            sample_count = self.m_node_sample_counts.get(node_id, 0)
            if sample_count > 0:
                label += f"\n{sample_count} samples"
            
            ax.text(x, y, label, ha='center', va='center', fontsize=8, 
                   weight='bold' if sample_count > 0 else 'normal')
            
            # Draw edges to children
            for child_id in self.getChildren(node_id):
                if child_id in positions:
                    child_x, child_y = positions[child_id]
                    ax.plot([x, child_x], [y - 0.3, child_y + 0.3], 
                           'k-', linewidth=1, alpha=0.6)
        
        # Set axis properties
        ax.set_aspect('equal')
        ax.axis('off')
        
        # Add title
        plt.title(f'Calling Context Tree - {metric.capitalize()}', 
                 fontsize=14, weight='bold', pad=20)
        
        # Add colorbar legend
        sm = plt.cm.ScalarMappable(cmap=plt.cm.YlOrRd, 
                                   norm=plt.Normalize(vmin=0, vmax=max_metric))
        sm.set_array([])
        cbar = plt.colorbar(sm, ax=ax, fraction=0.046, pad=0.04)
        cbar.set_label(f'{metric.capitalize()}', rotation=270, labelpad=15)
        
        # Auto-adjust layout
        plt.tight_layout()
        
        # Save figure
        plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
        plt.close()
    
    def _visualize_ring(self, output_file: str, metric: str, 
                       figsize: Tuple[int, int], dpi: int) -> None:
        """
        Create a ring/sunburst chart visualization.
        
        Args:
            output_file: Path to save the visualization
            metric: Metric to use for sizing
            figsize: Figure size
            dpi: DPI for output
        """
        import matplotlib.pyplot as plt
        from matplotlib.patches import Wedge
        import numpy as np
        
        fig, ax = plt.subplots(figsize=figsize, dpi=dpi, subplot_kw={'aspect': 'equal'})
        
        # Get max metric for color normalization
        max_metric = 0
        for node_id in self.m_node_metrics.keys():
            inclusive = self.getInclusiveMetrics(node_id)
            max_metric = max(max_metric, inclusive.get(metric, 0))
        
        # Calculate ring data
        def draw_ring_level(node_id: int, depth: int, start_angle: float, 
                           end_angle: float, parent_metric: float) -> None:
            """Recursively draw ring levels."""
            node = self.getNode(node_id)
            if not node:
                return
            
            # Get metrics
            inclusive = self.getInclusiveMetrics(node_id)
            node_metric = inclusive.get(metric, 1.0)
            
            # Calculate ring parameters
            inner_radius = depth * 0.5
            outer_radius = (depth + 1) * 0.5
            
            # Color based on metric
            if max_metric > 0:
                ratio = node_metric / max_metric
                color = plt.cm.YlOrRd(ratio)
            else:
                color = 'lightblue'
            
            # Draw wedge
            wedge = Wedge((0, 0), outer_radius, start_angle, end_angle,
                         width=outer_radius - inner_radius, 
                         facecolor=color, edgecolor='white', linewidth=1.5)
            ax.add_patch(wedge)
            
            # Add label if space permits
            angle_range = end_angle - start_angle
            if angle_range > 5:  # Only label if enough space
                mid_angle = (start_angle + end_angle) / 2
                mid_radius = (inner_radius + outer_radius) / 2
                
                x = mid_radius * np.cos(np.radians(mid_angle))
                y = mid_radius * np.sin(np.radians(mid_angle))
                
                label = node.getName()
                if len(label) > 10:
                    label = label[:8] + ".."
                
                rotation = mid_angle
                if 90 < mid_angle < 270:
                    rotation = mid_angle - 180
                
                ax.text(x, y, label, ha='center', va='center', 
                       fontsize=7, rotation=rotation, weight='bold')
            
            # Draw children
            children = self.getChildren(node_id)
            if children:
                # Calculate angular distribution based on metrics
                child_metrics = []
                for child_id in children:
                    child_inclusive = self.getInclusiveMetrics(child_id)
                    child_metrics.append(child_inclusive.get(metric, 1.0))
                
                total_child_metric = sum(child_metrics)
                if total_child_metric <= 0:
                    total_child_metric = len(children)
                    child_metrics = [1.0] * len(children)
                
                # Distribute angle range among children
                current_angle = start_angle
                angle_range = end_angle - start_angle
                
                for child_id, child_metric in zip(children, child_metrics):
                    child_angle_range = angle_range * (child_metric / total_child_metric)
                    draw_ring_level(child_id, depth + 1, current_angle, 
                                  current_angle + child_angle_range, node_metric)
                    current_angle += child_angle_range
        
        # Start from root
        draw_ring_level(self.m_root.getId(), 0, 0, 360, max_metric)
        
        # Set axis limits
        max_depth = self.getContextDepth()
        max_radius = (max_depth + 2) * 0.5
        ax.set_xlim(-max_radius, max_radius)
        ax.set_ylim(-max_radius, max_radius)
        ax.axis('off')
        
        # Add title
        plt.title(f'Calling Context Tree - Ring Chart ({metric.capitalize()})', 
                 fontsize=14, weight='bold', pad=20)
        
        # Add colorbar
        sm = plt.cm.ScalarMappable(cmap=plt.cm.YlOrRd, 
                                   norm=plt.Normalize(vmin=0, vmax=max_metric))
        sm.set_array([])
        cbar = plt.colorbar(sm, ax=ax, fraction=0.046, pad=0.04)
        cbar.set_label(f'{metric.capitalize()}', rotation=270, labelpad=15)
        
        # Add legend for depth
        legend_text = f"Depth: {max_depth} levels"
        ax.text(0.02, 0.98, legend_text, transform=ax.transAxes,
               fontsize=10, verticalalignment='top',
               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
        
        # Save figure
        plt.tight_layout()
        plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
        plt.close()
