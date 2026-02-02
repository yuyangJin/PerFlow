#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
NPB Visualization Script
Generates visualizations for overhead and hotspot analysis
"""

import sys
import os
import json
import csv
from pathlib import Path
from collections import defaultdict

try:
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not available. Visualizations will not be generated.")

# ============================================================================
# Overhead Visualization
# ============================================================================

def plot_overhead_analysis(csv_file: str, output_file: str):
    """
    Generate line chart showing overhead percentage vs. process scale
    """
    if not HAS_MATPLOTLIB:
        print("Skipping overhead visualization (matplotlib not available)")
        return
    
    # Read data from CSV
    benchmarks = defaultdict(lambda: defaultdict(list))
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            benchmark = row['benchmark']
            nprocs = int(row['num_processes'])
            overhead = row['overhead_percent']
            
            if overhead != 'N/A':
                try:
                    overhead_val = float(overhead)
                    benchmarks[benchmark][nprocs].append(overhead_val)
                except ValueError:
                    pass
    
    if not benchmarks:
        print("No overhead data to visualize")
        return
    
    # Calculate average overhead for each benchmark and process scale
    avg_overhead = defaultdict(dict)
    for benchmark, data in benchmarks.items():
        for nprocs, values in data.items():
            avg_overhead[benchmark][nprocs] = sum(values) / len(values)
    
    # Create plot
    fig, ax = plt.subplots(figsize=(12, 7))
    
    colors = plt.cm.tab10(np.linspace(0, 1, len(avg_overhead)))
    
    for i, (benchmark, data) in enumerate(sorted(avg_overhead.items())):
        process_scales = sorted(data.keys())
        overhead_values = [data[ps] for ps in process_scales]
        
        ax.plot(process_scales, overhead_values, 
                marker='o', linewidth=2, markersize=8,
                label=benchmark.upper(), color=colors[i])
    
    ax.set_xlabel('Number of Processes', fontsize=12, fontweight='bold')
    ax.set_ylabel('Overhead (%)', fontsize=12, fontweight='bold')
    ax.set_title('PerFlow MPI Sampler Overhead Analysis\n(NPB Benchmarks)', 
                 fontsize=14, fontweight='bold')
    ax.set_xscale('log', base=2)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.legend(loc='best', fontsize=10, framealpha=0.9)
    
    # Add horizontal line at 0% for reference
    ax.axhline(y=0, color='gray', linestyle='--', alpha=0.5, linewidth=1)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Overhead visualization saved to: {output_file}")
    plt.close()

# ============================================================================
# Hotspot Visualization
# ============================================================================

def plot_hotspot_analysis(json_file: str, output_file: str, top_n: int = 5):
    """
    Generate stacked bar chart showing hotspot distribution
    """
    if not HAS_MATPLOTLIB:
        print("Skipping hotspot visualization (matplotlib not available)")
        return
    
    # Read hotspot data from JSON
    with open(json_file, 'r') as f:
        data = json.load(f)
    
    if 'benchmarks' not in data or not data['benchmarks']:
        print("No hotspot data to visualize")
        return
    
    # Prepare data for visualization
    # Group by benchmark
    for benchmark_data in data['benchmarks']:
        benchmark_name = benchmark_data['benchmark']
        process_scales = []
        hotspot_data = defaultdict(list)
        
        for scale_data in benchmark_data['process_scales']:
            nprocs = scale_data['num_processes']
            process_scales.append(nprocs)
            
            # Get top N hotspots
            hotspots = scale_data.get('hotspots', [])[:top_n]
            
            # Collect percentages for each hotspot function
            for hotspot in hotspots:
                func_name = hotspot['function']
                percentage = hotspot['percentage']
                hotspot_data[func_name].append(percentage)
            
            # Fill missing data with 0
            for func_name in hotspot_data.keys():
                if len(hotspot_data[func_name]) < len(process_scales):
                    hotspot_data[func_name].append(0.0)
        
        if not process_scales:
            continue
        
        # Create stacked bar chart
        fig, ax = plt.subplots(figsize=(12, 7))
        
        # Prepare data for stacking
        functions = list(hotspot_data.keys())
        x_pos = np.arange(len(process_scales))
        bar_width = 0.6
        
        # Create stacked bars
        bottom = np.zeros(len(process_scales))
        colors = plt.cm.Set3(np.linspace(0, 1, len(functions)))
        
        for i, func in enumerate(functions):
            values = hotspot_data[func]
            # Truncate function name if too long
            func_display = func if len(func) <= 30 else func[:27] + '...'
            ax.bar(x_pos, values, bar_width, bottom=bottom,
                   label=func_display, color=colors[i])
            bottom += values
        
        ax.set_xlabel('Number of Processes', fontsize=12, fontweight='bold')
        ax.set_ylabel('Percentage of Total Time (%)', fontsize=12, fontweight='bold')
        ax.set_title(f'Hotspot Distribution - {benchmark_name.upper()}\n'
                     f'(Top {top_n} Functions, Exclusive Time)',
                     fontsize=14, fontweight='bold')
        ax.set_xticks(x_pos)
        ax.set_xticklabels([str(ps) for ps in process_scales])
        ax.legend(loc='upper left', bbox_to_anchor=(1, 1), fontsize=9)
        ax.grid(True, axis='y', alpha=0.3, linestyle='--')
        
        plt.tight_layout()
        
        # Save individual benchmark plot
        output_path = output_file.replace('.png', f'_{benchmark_name}.png')
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        print(f"Hotspot visualization for {benchmark_name} saved to: {output_path}")
        plt.close()
    
    # Create combined overview plot
    create_combined_hotspot_plot(data, output_file, top_n)

def create_combined_hotspot_plot(data: dict, output_file: str, top_n: int):
    """
    Create a combined plot showing all benchmarks
    """
    if not HAS_MATPLOTLIB:
        return
    
    benchmarks = data['benchmarks']
    if not benchmarks:
        return
    
    # Create subplots for each benchmark
    n_benchmarks = len(benchmarks)
    n_cols = 2
    n_rows = (n_benchmarks + n_cols - 1) // n_cols
    
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(16, 5 * n_rows))
    if n_benchmarks == 1:
        axes = np.array([axes])
    axes = axes.flatten()
    
    for idx, benchmark_data in enumerate(benchmarks):
        ax = axes[idx]
        benchmark_name = benchmark_data['benchmark']
        
        process_scales = []
        total_samples = []
        
        for scale_data in benchmark_data['process_scales']:
            process_scales.append(scale_data['num_processes'])
            total_samples.append(scale_data['total_samples'])
        
        if process_scales:
            ax.plot(process_scales, total_samples, marker='o', linewidth=2, markersize=8)
            ax.set_xlabel('Number of Processes', fontsize=10)
            ax.set_ylabel('Total Samples', fontsize=10)
            ax.set_title(f'{benchmark_name.upper()}', fontsize=11, fontweight='bold')
            ax.set_xscale('log', base=2)
            ax.grid(True, alpha=0.3)
    
    # Hide unused subplots
    for idx in range(n_benchmarks, len(axes)):
        axes[idx].axis('off')
    
    plt.suptitle('Sample Collection Summary Across Benchmarks', 
                 fontsize=14, fontweight='bold', y=1.00)
    plt.tight_layout()
    
    combined_file = output_file.replace('.png', '_combined.png')
    plt.savefig(combined_file, dpi=300, bbox_inches='tight')
    print(f"Combined hotspot visualization saved to: {combined_file}")
    plt.close()

# ============================================================================
# Combined Report
# ============================================================================

def generate_summary_report(results_dir: str, output_file: str):
    """
    Generate a text summary report
    """
    results_path = Path(results_dir)
    
    report_lines = []
    report_lines.append("=" * 80)
    report_lines.append("NPB BENCHMARK ANALYSIS SUMMARY REPORT")
    report_lines.append("=" * 80)
    report_lines.append("")
    
    # Baseline results
    baseline_csv = results_path / 'baseline' / 'baseline_results.csv'
    if baseline_csv.exists():
        report_lines.append("BASELINE PERFORMANCE")
        report_lines.append("-" * 80)
        
        benchmarks = defaultdict(lambda: defaultdict(list))
        with open(baseline_csv, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                benchmark = row['benchmark']
                nprocs = int(row['num_processes'])
                time = float(row['execution_time_sec'])
                benchmarks[benchmark][nprocs].append(time)
        
        for benchmark in sorted(benchmarks.keys()):
            report_lines.append(f"\n{benchmark.upper()}:")
            for nprocs in sorted(benchmarks[benchmark].keys()):
                avg_time = sum(benchmarks[benchmark][nprocs]) / len(benchmarks[benchmark][nprocs])
                report_lines.append(f"  {nprocs:4d} processes: {avg_time:8.3f} seconds")
        
        report_lines.append("")
    
    # Overhead results
    overhead_csv = results_path / 'overhead' / 'overhead_results.csv'
    if overhead_csv.exists():
        report_lines.append("")
        report_lines.append("OVERHEAD ANALYSIS")
        report_lines.append("-" * 80)
        
        benchmarks = defaultdict(lambda: defaultdict(list))
        with open(overhead_csv, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                benchmark = row['benchmark']
                nprocs = int(row['num_processes'])
                overhead = row['overhead_percent']
                if overhead != 'N/A':
                    try:
                        benchmarks[benchmark][nprocs].append(float(overhead))
                    except ValueError:
                        pass
        
        for benchmark in sorted(benchmarks.keys()):
            report_lines.append(f"\n{benchmark.upper()}:")
            for nprocs in sorted(benchmarks[benchmark].keys()):
                avg_overhead = sum(benchmarks[benchmark][nprocs]) / len(benchmarks[benchmark][nprocs])
                report_lines.append(f"  {nprocs:4d} processes: {avg_overhead:6.2f}% overhead")
        
        report_lines.append("")
    
    # Hotspot analysis
    hotspot_json = results_path / 'hotspot_analysis.json'
    if hotspot_json.exists():
        report_lines.append("")
        report_lines.append("HOTSPOT ANALYSIS (Top 3 Functions)")
        report_lines.append("-" * 80)
        
        with open(hotspot_json, 'r') as f:
            hotspot_data = json.load(f)
        
        for benchmark_data in hotspot_data.get('benchmarks', []):
            benchmark_name = benchmark_data['benchmark']
            report_lines.append(f"\n{benchmark_name.upper()}:")
            
            for scale_data in benchmark_data['process_scales']:
                nprocs = scale_data['num_processes']
                hotspots = scale_data.get('hotspots', [])[:3]
                
                report_lines.append(f"  {nprocs} processes:")
                for i, hotspot in enumerate(hotspots, 1):
                    func = hotspot['function']
                    pct = hotspot['percentage']
                    # Truncate long function names
                    if len(func) > 50:
                        func = func[:47] + "..."
                    report_lines.append(f"    {i}. {func}: {pct:.2f}%")
        
        report_lines.append("")
    
    report_lines.append("=" * 80)
    report_lines.append("END OF REPORT")
    report_lines.append("=" * 80)
    
    # Write report
    with open(output_file, 'w') as f:
        f.write('\n'.join(report_lines))
    
    print(f"Summary report saved to: {output_file}")

# ============================================================================
# Main
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 npb_visualize.py <results_dir>")
        print()
        print("Arguments:")
        print("  results_dir   Directory containing NPB benchmark results")
        print()
        print("This script generates:")
        print("  - Overhead line chart (overhead_analysis.png)")
        print("  - Hotspot stacked bar charts (hotspot_analysis_*.png)")
        print("  - Summary text report (summary_report.txt)")
        sys.exit(1)
    
    results_dir = sys.argv[1]
    results_path = Path(results_dir)
    
    if not results_path.exists():
        print(f"Error: Results directory not found: {results_dir}")
        sys.exit(1)
    
    print("NPB Benchmark Visualization")
    print("=" * 60)
    print(f"Results directory: {results_dir}")
    print()
    
    # Generate overhead visualization
    overhead_csv = results_path / 'overhead' / 'overhead_results.csv'
    if overhead_csv.exists():
        print("Generating overhead visualization...")
        plot_overhead_analysis(
            str(overhead_csv),
            str(results_path / 'overhead_analysis.png')
        )
    else:
        print("Warning: Overhead results not found, skipping overhead visualization")
    
    # Generate hotspot visualization
    hotspot_json = results_path / 'hotspot_analysis.json'
    if hotspot_json.exists():
        print("Generating hotspot visualizations...")
        plot_hotspot_analysis(
            str(hotspot_json),
            str(results_path / 'hotspot_analysis.png'),
            top_n=5
        )
    else:
        print("Warning: Hotspot analysis not found, skipping hotspot visualization")
    
    # Generate summary report
    print("Generating summary report...")
    generate_summary_report(
        results_dir,
        str(results_path / 'summary_report.txt')
    )
    
    print()
    print("=" * 60)
    print("Visualization complete!")

if __name__ == '__main__':
    main()
