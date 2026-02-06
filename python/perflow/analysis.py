"""
High-level analysis tasks for PerFlow.

This module provides pre-built analysis tasks that can be composed
into workflows using the dataflow graph abstraction.
"""

from typing import List, Tuple, Dict, Any, Optional
import os

from .dataflow import Task

try:
    from ._perflow_bindings import (
        TreeBuilder, TreeBuildMode, SampleCountMode, ColorScheme,
        ParallelBuildStrategy, BalanceAnalyzer, HotspotAnalyzer, 
        TreeTraversal, TreeVisualizer
    )
    cpf_available = True
except ImportError:
    TreeBuilder = None
    TreeBuildMode = None
    SampleCountMode = None
    ColorScheme = None
    ParallelBuildStrategy = None
    BalanceAnalyzer = None
    HotspotAnalyzer = None
    TreeTraversal = None
    TreeVisualizer = None
    cpf_available = False


class LoadTreeTask(Task):
    """Task to load a performance tree from sample files."""
    
    def __init__(self, sample_files: List[Tuple[str, int]], 
                 libmap_files: Optional[List[Tuple[str, int]]] = None,
                 time_per_sample: float = 1000.0,
                 build_mode = None,
                 count_mode = None,
                 parallel: bool = True,
                 parallel_strategy = None,
                 name: str = None):
        """
        Initialize LoadTreeTask.
        
        Args:
            sample_files: List of (file_path, process_id) tuples
            libmap_files: Optional list of (libmap_path, process_id) tuples
            time_per_sample: Time per sample in microseconds
            build_mode: TreeBuildMode (CONTEXT_FREE or CONTEXT_AWARE)
            count_mode: SampleCountMode (EXCLUSIVE, INCLUSIVE, or BOTH)
            parallel: Whether to load files in parallel
            parallel_strategy: ParallelBuildStrategy (COARSE_LOCK, FINE_GRAINED_LOCK, 
                              THREAD_LOCAL_MERGE, or LOCK_FREE)
            name: Optional task name
        """
        super().__init__(name or "LoadTree")
        self.sample_files = sample_files
        self.libmap_files = libmap_files or []
        self.time_per_sample = time_per_sample
        self.build_mode = build_mode or (TreeBuildMode.CONTEXT_FREE if cpf_available else None)
        self.count_mode = count_mode or (SampleCountMode.EXCLUSIVE if cpf_available else None)
        self.parallel = parallel
        self.parallel_strategy = parallel_strategy or (
            ParallelBuildStrategy.THREAD_LOCAL_MERGE if cpf_available and parallel else None)
    
    def execute(self, **inputs) -> Any:
        """Execute the task."""
        if not cpf_available:
            raise RuntimeError("C++ bindings not available")
        
        # Create tree builder
        builder = TreeBuilder(self.build_mode, self.count_mode)
        
        # Set parallel strategy if doing parallel build
        if self.parallel and self.parallel_strategy:
            builder.set_parallel_strategy(self.parallel_strategy)
        
        # Load library maps if provided
        if self.libmap_files:
            builder.load_library_maps(self.libmap_files)
        
        # Build tree from sample files
        if self.parallel:
            success = builder.build_from_files_parallel(
                self.sample_files, self.time_per_sample)
        else:
            success = builder.build_from_files(
                self.sample_files, self.time_per_sample)
        
        if success == 0:
            raise RuntimeError("Failed to load any sample files")
        
        return builder.tree()


class BalanceAnalysisTask(Task):
    """Task to analyze workload balance."""
    
    def __init__(self, name: str = None):
        """Initialize BalanceAnalysisTask."""
        super().__init__(name or "BalanceAnalysis")
    
    def execute(self, tree=None, **inputs) -> Any:
        """Execute the task."""
        if not cpf_available:
            raise RuntimeError("C++ bindings not available")
        
        if tree is None:
            raise ValueError("tree input is required")
        
        return BalanceAnalyzer.analyze(tree)


class HotspotAnalysisTask(Task):
    """Task to find performance hotspots."""
    
    def __init__(self, top_n: int = 10, mode: str = "self", name: str = None):
        """
        Initialize HotspotAnalysisTask.
        
        Args:
            top_n: Number of top hotspots to return
            mode: Analysis mode - "self" (exclusive), "total" (inclusive)
            name: Optional task name
        """
        super().__init__(name or f"HotspotAnalysis_{mode}")
        self.top_n = top_n
        self.mode = mode
    
    def execute(self, tree=None, **inputs) -> Any:
        """Execute the task."""
        if not cpf_available:
            raise RuntimeError("C++ bindings not available")
        
        if tree is None:
            raise ValueError("tree input is required")
        
        if self.mode == "self":
            return HotspotAnalyzer.find_self_hotspots(tree, self.top_n)
        elif self.mode == "total":
            return HotspotAnalyzer.find_total_hotspots(tree, self.top_n)
        else:
            raise ValueError(f"Invalid mode: {self.mode}")


class FilterNodesTask(Task):
    """Task to filter tree nodes based on criteria."""
    
    def __init__(self, predicate, name: str = None):
        """
        Initialize FilterNodesTask.
        
        Args:
            predicate: Function to filter nodes
            name: Optional task name
        """
        super().__init__(name or "FilterNodes")
        self.predicate = predicate
    
    def execute(self, tree=None, **inputs) -> Any:
        """Execute the task."""
        if not cpf_available:
            raise RuntimeError("C++ bindings not available")
        
        if tree is None:
            raise ValueError("tree input is required")
        
        root = tree.root()
        return TreeTraversal.find_nodes(root, self.predicate)


class TraverseTreeTask(Task):
    """Task to traverse the tree with a visitor."""
    
    def __init__(self, visitor, traversal_mode: str = "dfs_pre", 
                 max_depth: int = 0, name: str = None):
        """
        Initialize TraverseTreeTask.
        
        Args:
            visitor: TreeVisitor instance
            traversal_mode: "dfs_pre", "dfs_post", or "bfs"
            max_depth: Maximum depth to traverse (0 = unlimited)
            name: Optional task name
        """
        super().__init__(name or f"TraverseTree_{traversal_mode}")
        self.visitor = visitor
        self.traversal_mode = traversal_mode
        self.max_depth = max_depth
    
    def execute(self, tree=None, **inputs) -> Any:
        """Execute the task."""
        if not cpf_available:
            raise RuntimeError("C++ bindings not available")
        
        if tree is None:
            raise ValueError("tree input is required")
        
        root = tree.root()
        
        if self.traversal_mode == "dfs_pre":
            TreeTraversal.depth_first_preorder(root, self.visitor, self.max_depth)
        elif self.traversal_mode == "dfs_post":
            TreeTraversal.depth_first_postorder(root, self.visitor, self.max_depth)
        elif self.traversal_mode == "bfs":
            TreeTraversal.breadth_first(root, self.visitor, self.max_depth)
        else:
            raise ValueError(f"Invalid traversal mode: {self.traversal_mode}")
        
        return self.visitor


class ExportVisualizationTask(Task):
    """Task to export tree visualization."""
    
    def __init__(self, output_path: str, 
                 color_scheme = None,
                 max_depth: int = 0,
                 name: str = None):
        """
        Initialize ExportVisualizationTask.
        
        Args:
            output_path: Output file path
            color_scheme: ColorScheme (GRAYSCALE, HEATMAP, or RAINBOW)
            max_depth: Maximum depth to visualize (0 = unlimited)
            name: Optional task name
        """
        super().__init__(name or "ExportVisualization")
        self.output_path = output_path
        self.color_scheme = color_scheme or (ColorScheme.HEATMAP if cpf_available else None)
        self.max_depth = max_depth
    
    def execute(self, tree=None, **inputs) -> Any:
        """Execute the task."""
        if not cpf_available:
            raise RuntimeError("C++ bindings not available")
        
        if tree is None:
            raise ValueError("tree input is required")
        
        # Ensure output directory exists
        os.makedirs(os.path.dirname(self.output_path), exist_ok=True)
        
        # Export visualization
        success = TreeVisualizer.generate_pdf(
            tree, self.output_path, self.color_scheme, self.max_depth)
        
        if not success:
            raise RuntimeError(f"Failed to export visualization to {self.output_path}")
        
        return self.output_path


class AggregateResultsTask(Task):
    """Task to aggregate multiple analysis results."""
    
    def __init__(self, aggregation_func, name: str = None):
        """
        Initialize AggregateResultsTask.
        
        Args:
            aggregation_func: Function to aggregate results
            name: Optional task name
        """
        super().__init__(name or "AggregateResults")
        self.aggregation_func = aggregation_func
    
    def execute(self, **inputs) -> Any:
        """Execute the task."""
        return self.aggregation_func(inputs)


class ReportTask(Task):
    """Task to generate analysis reports."""
    
    def __init__(self, output_path: str, 
                 format: str = "text",
                 name: str = None):
        """
        Initialize ReportTask.
        
        Args:
            output_path: Output file path
            format: Report format ("text", "json", "html")
            name: Optional task name
        """
        super().__init__(name or f"Report_{format}")
        self.output_path = output_path
        self.format = format
    
    def execute(self, **inputs) -> Any:
        """Execute the task."""
        # Ensure output directory exists
        os.makedirs(os.path.dirname(self.output_path), exist_ok=True)
        
        if self.format == "text":
            return self._generate_text_report(inputs)
        elif self.format == "json":
            return self._generate_json_report(inputs)
        elif self.format == "html":
            return self._generate_html_report(inputs)
        else:
            raise ValueError(f"Invalid format: {self.format}")
    
    def _generate_text_report(self, results: Dict[str, Any]) -> str:
        """Generate a text report."""
        lines = ["=" * 80]
        lines.append("PerFlow Analysis Report")
        lines.append("=" * 80)
        lines.append("")
        
        for key, value in results.items():
            lines.append(f"## {key}")
            lines.append(str(value))
            lines.append("")
        
        content = "\n".join(lines)
        
        with open(self.output_path, 'w') as f:
            f.write(content)
        
        return self.output_path
    
    def _generate_json_report(self, results: Dict[str, Any]) -> str:
        """Generate a JSON report."""
        import json
        
        # Convert results to JSON-serializable format
        json_results = {}
        for key, value in results.items():
            try:
                json_results[key] = self._to_json_serializable(value)
            except Exception as e:
                json_results[key] = f"<non-serializable: {type(value).__name__}>"
        
        with open(self.output_path, 'w') as f:
            json.dump(json_results, f, indent=2)
        
        return self.output_path
    
    def _generate_html_report(self, results: Dict[str, Any]) -> str:
        """Generate an HTML report."""
        lines = ["<!DOCTYPE html>"]
        lines.append("<html><head><title>PerFlow Analysis Report</title>")
        lines.append("<style>")
        lines.append("body { font-family: Arial, sans-serif; margin: 20px; }")
        lines.append("h1 { color: #333; }")
        lines.append("h2 { color: #666; margin-top: 30px; }")
        lines.append("pre { background: #f4f4f4; padding: 10px; border-radius: 5px; }")
        lines.append("</style></head><body>")
        lines.append("<h1>PerFlow Analysis Report</h1>")
        
        for key, value in results.items():
            lines.append(f"<h2>{key}</h2>")
            lines.append(f"<pre>{str(value)}</pre>")
        
        lines.append("</body></html>")
        
        content = "\n".join(lines)
        
        with open(self.output_path, 'w') as f:
            f.write(content)
        
        return self.output_path
    
    def _to_json_serializable(self, obj):
        """Convert object to JSON-serializable format."""
        if hasattr(obj, '__dict__'):
            return {k: self._to_json_serializable(v) 
                   for k, v in obj.__dict__.items() 
                   if not k.startswith('_')}
        elif isinstance(obj, (list, tuple)):
            return [self._to_json_serializable(item) for item in obj]
        elif isinstance(obj, dict):
            return {k: self._to_json_serializable(v) for k, v in obj.items()}
        else:
            return obj
