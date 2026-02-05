"""
Pre-built analysis workflows for common use cases.

This module provides ready-to-use workflows for typical
performance analysis scenarios.
"""

from typing import List, Tuple, Optional
import os

from .dataflow import Graph
from .analysis import (
    LoadTreeTask, BalanceAnalysisTask, HotspotAnalysisTask,
    ExportVisualizationTask, ReportTask
)

try:
    from ._perflow_bindings import ColorScheme, SampleCountMode
    cpf_available = True
except ImportError:
    ColorScheme = None
    SampleCountMode = None
    cpf_available = False


def create_basic_analysis_workflow(
    sample_files: List[Tuple[str, int]],
    output_dir: str,
    libmap_files: Optional[List[Tuple[str, int]]] = None,
    top_n: int = 10,
    parallel: bool = True
) -> Graph:
    """
    Create a basic analysis workflow.
    
    This workflow:
    1. Loads the performance tree from sample files
    2. Analyzes workload balance
    3. Finds performance hotspots
    4. Generates visualization
    5. Produces a text report
    
    Args:
        sample_files: List of (file_path, process_id) tuples
        output_dir: Output directory for results
        libmap_files: Optional list of (libmap_path, process_id) tuples
        top_n: Number of top hotspots to find
        parallel: Whether to load files in parallel
        
    Returns:
        Configured dataflow graph
    """
    # Create graph
    graph = Graph(name="BasicAnalysis")
    
    # Task 1: Load tree
    load_task = LoadTreeTask(
        sample_files=sample_files,
        libmap_files=libmap_files,
        parallel=parallel,
        name="LoadTree"
    )
    load_node = graph.add_node(load_task)
    
    # Task 2: Balance analysis
    balance_task = BalanceAnalysisTask(name="BalanceAnalysis")
    balance_node = graph.add_node(balance_task)
    graph.add_edge(load_node, balance_node, "tree")
    
    # Task 3: Hotspot analysis
    hotspot_task = HotspotAnalysisTask(top_n=top_n, mode="self", name="HotspotAnalysis")
    hotspot_node = graph.add_node(hotspot_task)
    graph.add_edge(load_node, hotspot_node, "tree")
    
    # Task 4: Export visualization
    viz_path = os.path.join(output_dir, "performance_tree.pdf")
    viz_task = ExportVisualizationTask(
        output_path=viz_path,
        color_scheme=ColorScheme.HEATMAP if cpf_available else None,
        name="ExportVisualization"
    )
    viz_node = graph.add_node(viz_task)
    graph.add_edge(load_node, viz_node, "tree")
    
    # Task 5: Generate report
    report_path = os.path.join(output_dir, "analysis_report.txt")
    
    def format_report(inputs):
        lines = ["=" * 80]
        lines.append("PerFlow Performance Analysis Report")
        lines.append("=" * 80)
        lines.append("")
        
        # Balance analysis
        if "balance" in inputs:
            balance = inputs["balance"]
            lines.append("## Workload Balance")
            lines.append(f"Mean samples:     {balance.mean_samples:.2f}")
            lines.append(f"Std deviation:    {balance.std_dev_samples:.2f}")
            lines.append(f"Min samples:      {balance.min_samples:.2f}")
            lines.append(f"Max samples:      {balance.max_samples:.2f}")
            lines.append(f"Imbalance factor: {balance.imbalance_factor:.4f}")
            lines.append(f"Most loaded:      Process {balance.most_loaded_process}")
            lines.append(f"Least loaded:     Process {balance.least_loaded_process}")
            lines.append("")
        
        # Hotspot analysis
        if "hotspots" in inputs:
            hotspots = inputs["hotspots"]
            lines.append("## Performance Hotspots (Self Time)")
            lines.append("-" * 80)
            lines.append(f"{'Rank':<6} {'Function':<40} {'Self %':<10} {'Total %':<10}")
            lines.append("-" * 80)
            for i, h in enumerate(hotspots, 1):
                func = h.function_name[:38] if len(h.function_name) > 38 else h.function_name
                lines.append(f"{i:<6} {func:<40} {h.self_percentage:>8.2f}% {h.percentage:>8.2f}%")
            lines.append("")
        
        # Visualization
        if "visualization" in inputs:
            lines.append(f"## Visualization")
            lines.append(f"Generated: {inputs['visualization']}")
            lines.append("")
        
        return "\n".join(lines)
    
    from .analysis import AggregateResultsTask
    report_task = AggregateResultsTask(
        aggregation_func=format_report,
        name="GenerateReport"
    )
    report_node = graph.add_node(report_task)
    graph.add_edge(balance_node, report_node, "balance")
    graph.add_edge(hotspot_node, report_node, "hotspots")
    graph.add_edge(viz_node, report_node, "visualization")
    
    # Save report to file (as a separate task)
    save_task = ReportTask(output_path=report_path, format="text", name="SaveReport")
    save_node = graph.add_node(save_task)
    graph.add_edge(report_node, save_node, "report")
    
    return graph


def create_comparative_analysis_workflow(
    sample_file_sets: List[List[Tuple[str, int]]],
    labels: List[str],
    output_dir: str,
    libmap_files: Optional[List[Tuple[str, int]]] = None
) -> Graph:
    """
    Create a comparative analysis workflow for multiple datasets.
    
    This workflow loads multiple performance trees and compares them.
    
    Args:
        sample_file_sets: List of sample file lists
        labels: Labels for each dataset
        output_dir: Output directory for results
        libmap_files: Optional library map files
        
    Returns:
        Configured dataflow graph
    """
    graph = Graph(name="ComparativeAnalysis")
    
    load_nodes = []
    balance_nodes = []
    hotspot_nodes = []
    
    # Create parallel analysis pipelines for each dataset
    for i, (sample_files, label) in enumerate(zip(sample_file_sets, labels)):
        # Load tree
        load_task = LoadTreeTask(
            sample_files=sample_files,
            libmap_files=libmap_files,
            name=f"LoadTree_{label}"
        )
        load_node = graph.add_node(load_task)
        load_nodes.append(load_node)
        
        # Balance analysis
        balance_task = BalanceAnalysisTask(name=f"BalanceAnalysis_{label}")
        balance_node = graph.add_node(balance_task)
        graph.add_edge(load_node, balance_node, "tree")
        balance_nodes.append(balance_node)
        
        # Hotspot analysis
        hotspot_task = HotspotAnalysisTask(top_n=10, mode="self", name=f"HotspotAnalysis_{label}")
        hotspot_node = graph.add_node(hotspot_task)
        graph.add_edge(load_node, hotspot_node, "tree")
        hotspot_nodes.append(hotspot_node)
    
    # Aggregate and compare results
    def compare_results(inputs):
        lines = ["=" * 80]
        lines.append("PerFlow Comparative Analysis Report")
        lines.append("=" * 80)
        lines.append("")
        
        # Compare balance
        lines.append("## Workload Balance Comparison")
        lines.append(f"{'Dataset':<20} {'Mean':<12} {'Imbalance':<12}")
        lines.append("-" * 80)
        
        for i, label in enumerate(labels):
            balance_key = f"balance_{i}"
            if balance_key in inputs:
                balance = inputs[balance_key]
                lines.append(f"{label:<20} {balance.mean_samples:>10.2f} {balance.imbalance_factor:>10.4f}")
        
        lines.append("")
        lines.append("## Top Hotspots Comparison")
        
        for i, label in enumerate(labels):
            hotspot_key = f"hotspots_{i}"
            if hotspot_key in inputs:
                lines.append(f"\n### {label}")
                lines.append("-" * 80)
                hotspots = inputs[hotspot_key]
                for j, h in enumerate(hotspots[:5], 1):
                    lines.append(f"{j}. {h.function_name}: {h.self_percentage:.2f}%")
        
        return "\n".join(lines)
    
    from .analysis import AggregateResultsTask
    compare_task = AggregateResultsTask(
        aggregation_func=compare_results,
        name="CompareResults"
    )
    compare_node = graph.add_node(compare_task)
    
    # Connect all results to comparison task
    for i in range(len(labels)):
        graph.add_edge(balance_nodes[i], compare_node, f"balance_{i}")
        graph.add_edge(hotspot_nodes[i], compare_node, f"hotspots_{i}")
    
    # Save comparison report
    report_path = os.path.join(output_dir, "comparison_report.txt")
    save_task = ReportTask(output_path=report_path, format="text", name="SaveReport")
    save_node = graph.add_node(save_task)
    graph.add_edge(compare_node, save_node, "report")
    
    return graph


def create_hotspot_focused_workflow(
    sample_files: List[Tuple[str, int]],
    output_dir: str,
    libmap_files: Optional[List[Tuple[str, int]]] = None,
    sample_threshold: int = 100
) -> Graph:
    """
    Create a hotspot-focused workflow.
    
    This workflow focuses on identifying and analyzing performance hotspots
    in detail.
    
    Args:
        sample_files: List of (file_path, process_id) tuples
        output_dir: Output directory for results
        libmap_files: Optional library map files
        sample_threshold: Minimum samples for a node to be considered
        
    Returns:
        Configured dataflow graph
    """
    graph = Graph(name="HotspotFocused")
    
    # Load tree
    load_task = LoadTreeTask(
        sample_files=sample_files,
        libmap_files=libmap_files,
        count_mode=SampleCountMode.BOTH if cpf_available else None,
        name="LoadTree"
    )
    load_node = graph.add_node(load_task)
    
    # Find hotspots by self time
    self_hotspot_task = HotspotAnalysisTask(top_n=20, mode="self", name="SelfHotspots")
    self_node = graph.add_node(self_hotspot_task)
    graph.add_edge(load_node, self_node, "tree")
    
    # Find hotspots by total time
    total_hotspot_task = HotspotAnalysisTask(top_n=20, mode="total", name="TotalHotspots")
    total_node = graph.add_node(total_hotspot_task)
    graph.add_edge(load_node, total_node, "tree")
    
    # Generate visualizations
    self_viz_path = os.path.join(output_dir, "hotspots_self.pdf")
    self_viz_task = ExportVisualizationTask(
        output_path=self_viz_path,
        color_scheme=ColorScheme.HEATMAP if cpf_available else None,
        name="SelfVisualization"
    )
    self_viz_node = graph.add_node(self_viz_task)
    graph.add_edge(load_node, self_viz_node, "tree")
    
    # Generate detailed report
    report_path = os.path.join(output_dir, "hotspot_report.html")
    
    def format_hotspot_report(inputs):
        html = ["<!DOCTYPE html><html><head>"]
        html.append("<title>PerFlow Hotspot Analysis</title>")
        html.append("<style>")
        html.append("body { font-family: Arial; margin: 20px; }")
        html.append("h1 { color: #333; }")
        html.append("table { border-collapse: collapse; width: 100%; margin: 20px 0; }")
        html.append("th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }")
        html.append("th { background-color: #4CAF50; color: white; }")
        html.append("tr:nth-child(even) { background-color: #f2f2f2; }")
        html.append("</style></head><body>")
        html.append("<h1>PerFlow Hotspot Analysis Report</h1>")
        
        # Self time hotspots
        if "self_hotspots" in inputs:
            html.append("<h2>Top Hotspots by Self Time (Exclusive)</h2>")
            html.append("<table><tr><th>Rank</th><th>Function</th><th>Library</th><th>Self %</th><th>Total %</th></tr>")
            for i, h in enumerate(inputs["self_hotspots"], 1):
                html.append(f"<tr><td>{i}</td><td>{h.function_name}</td><td>{h.library_name}</td>"
                          f"<td>{h.self_percentage:.2f}%</td><td>{h.percentage:.2f}%</td></tr>")
            html.append("</table>")
        
        # Total time hotspots
        if "total_hotspots" in inputs:
            html.append("<h2>Top Hotspots by Total Time (Inclusive)</h2>")
            html.append("<table><tr><th>Rank</th><th>Function</th><th>Library</th><th>Total %</th><th>Self %</th></tr>")
            for i, h in enumerate(inputs["total_hotspots"], 1):
                html.append(f"<tr><td>{i}</td><td>{h.function_name}</td><td>{h.library_name}</td>"
                          f"<td>{h.percentage:.2f}%</td><td>{h.self_percentage:.2f}%</td></tr>")
            html.append("</table>")
        
        html.append("</body></html>")
        return "".join(html)
    
    from .analysis import AggregateResultsTask
    report_task = AggregateResultsTask(
        aggregation_func=format_hotspot_report,
        name="GenerateReport"
    )
    report_node = graph.add_node(report_task)
    graph.add_edge(self_node, report_node, "self_hotspots")
    graph.add_edge(total_node, report_node, "total_hotspots")
    
    # Save report
    save_task = ReportTask(output_path=report_path, format="html", name="SaveReport")
    save_node = graph.add_node(save_task)
    graph.add_edge(report_node, save_node, "report")
    
    return graph
