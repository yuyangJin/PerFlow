#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
NPB Hotspot Analysis Script
Analyzes performance sampling data from PerFlow using the npb_hotspot_analyzer tool
"""

import sys
import os
import json
import subprocess
import argparse
from pathlib import Path
from typing import Dict, List, Tuple
from collections import defaultdict

# ============================================================================
# Configuration
# ============================================================================

def get_analyzer_path():
    """Find the npb_hotspot_analyzer executable"""
    # Try several possible locations
    possible_paths = [
        "./build/examples/npb_hotspot_analyzer",
        "./examples/npb_hotspot_analyzer",
        os.path.join(os.environ.get("PERFLOW_BUILD_DIR", "../../build"), "examples/npb_hotspot_analyzer"),
    ]
    
    for path in possible_paths:
        full_path = Path(path).resolve()
        if full_path.exists():
            return str(full_path)
    
    # Try to find in PATH
    try:
        result = subprocess.run(["which", "npb_hotspot_analyzer"], 
                              capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except:
        pass
    
    return None

def get_process_scales(benchmark: str, standard_scales: List[int], square_scales: List[int]) -> List[int]:
    """Get appropriate process scales for a benchmark"""
    if benchmark.lower() in ['bt', 'sp']:
        return square_scales
    return standard_scales

# ============================================================================
# Hotspot Analysis
# ============================================================================

def find_sample_data_dirs(parent_dir: str, benchmarks: List[str], problem_class: str, 
                         process_scales: List[int], square_scales: List[int]) -> List[Tuple[str, str, str, int, int]]:
    """
    Find all sample data directories under parent_dir
    
    Returns:
        List of tuples: (dir_path, benchmark, class, num_procs, iteration)
    """
    sample_dirs = []
    sample_data_base = os.path.join(parent_dir, "overhead", "sample_data")
    
    if not os.path.isdir(sample_data_base):
        print(f"Warning: Sample data directory not found: {sample_data_base}", file=sys.stderr)
        return sample_dirs
    
    # Look for directories matching pattern: benchmark_class_npN_iterM
    for entry in os.listdir(sample_data_base):
        full_path = os.path.join(sample_data_base, entry)
        if not os.path.isdir(full_path):
            continue
        
        # Parse directory name
        parts = entry.split('_')
        if len(parts) < 4:
            continue
        
        benchmark = parts[0]
        bclass = parts[1]
        
        # Extract nprocs (format: npN)
        np_part = parts[2]
        if not np_part.startswith('np'):
            continue
        try:
            nprocs = int(np_part[2:])
        except ValueError:
            continue
        
        # Extract iteration (format: iterM)
        iter_part = parts[3]
        if not iter_part.startswith('iter'):
            continue
        try:
            iteration = int(iter_part[4:])
        except ValueError:
            continue
        
        # Filter by benchmarks if specified
        if benchmarks and benchmark not in benchmarks:
            continue
        
        # Filter by class if specified
        if problem_class and bclass != problem_class:
            continue
        
        # Filter by process scales if specified
        if process_scales or square_scales:
            bench_scales = get_process_scales(benchmark, process_scales, square_scales)
            if bench_scales and nprocs not in bench_scales:
                continue
        
        sample_dirs.append((full_path, benchmark, bclass, nprocs, iteration))
    
    return sorted(sample_dirs, key=lambda x: (x[1], x[3], x[4]))

def analyze_sample_dir(analyzer_path: str, sample_dir: str, output_file: str) -> Dict:
    """
    Analyze a single sample data directory using the npb_hotspot_analyzer tool
    
    Returns:
        Dictionary containing hotspot analysis results
    """
    # Run the analyzer
    try:
        result = subprocess.run(
            [analyzer_path, sample_dir, output_file],
            capture_output=True,
            text=True,
            check=True,
            timeout=300  # 5 minute timeout
        )
        
        # Print analyzer output if verbose
        if os.environ.get('VERBOSE'):
            if result.stdout:
                print(result.stdout)
            if result.stderr:
                print(result.stderr, file=sys.stderr)
    
    except subprocess.CalledProcessError as e:
        print(f"Warning: Hotspot analyzer failed for {sample_dir}", file=sys.stderr)
        if os.environ.get('VERBOSE'):
            if e.stdout:
                print("STDOUT:", file=sys.stderr)
                print(e.stdout, file=sys.stderr)
            if e.stderr:
                print("STDERR:", file=sys.stderr)
                print(e.stderr, file=sys.stderr)
        return None
    except subprocess.TimeoutExpired:
        print(f"Warning: Hotspot analyzer timed out for {sample_dir}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Warning: Error running analyzer for {sample_dir}: {e}", file=sys.stderr)
        return None
    
    # Load and return the results
    try:
        if os.path.exists(output_file):
            with open(output_file, 'r') as f:
                results = json.load(f)
            return results
    except Exception as e:
        print(f"Warning: Failed to load results from {output_file}: {e}", file=sys.stderr)
    
    return None

def aggregate_hotspots(all_results: List[Dict]) -> Dict:
    """
    Aggregate hotspot results from multiple sample directories
    
    Returns:
        Aggregated hotspot dictionary
    """
    # Aggregate by function name
    aggregated = defaultdict(lambda: {
        'function': '',
        'library': '',
        'filename': '',
        'line_number': 0,
        'exclusive_samples': 0,
        'inclusive_samples': 0,
        'count': 0
    })
    
    total_samples = 0
    
    for result in all_results:
        if not result:
            continue
        
        result_total = result.get('total_samples', 0)
        total_samples += result_total
        
        for hotspot in result.get('hotspots', []):
            func = hotspot['function']
            agg = aggregated[func]
            
            # Update aggregated data
            if not agg['function']:
                agg['function'] = hotspot['function']
                agg['library'] = hotspot['library']
                agg['filename'] = hotspot['filename']
                agg['line_number'] = hotspot['line_number']
            
            agg['exclusive_samples'] += hotspot['exclusive_samples']
            agg['inclusive_samples'] += hotspot['inclusive_samples']
            agg['count'] += 1
    
    # Convert to list and calculate percentages
    hotspots = []
    for func, data in aggregated.items():
        hotspot = {
            'function': data['function'],
            'library': data['library'],
            'filename': data['filename'],
            'line_number': data['line_number'],
            'exclusive_samples': data['exclusive_samples'],
            'inclusive_samples': data['inclusive_samples'],
            'exclusive_percentage': (100.0 * data['exclusive_samples'] / total_samples) if total_samples > 0 else 0.0,
            'inclusive_percentage': (100.0 * data['inclusive_samples'] / total_samples) if total_samples > 0 else 0.0,
        }
        hotspots.append(hotspot)
    
    # Sort by exclusive samples
    hotspots.sort(key=lambda x: x['exclusive_samples'], reverse=True)
    
    return {
        'total_samples': total_samples,
        'hotspots': hotspots
    }

def print_hotspot_summary(results: Dict, top_n: int = 20):
    """Print a summary of hotspot analysis results"""
    hotspots = results.get("hotspots", [])
    total_samples = results.get("total_samples", 0)
    
    print("\n" + "=" * 80)
    print("HOTSPOT ANALYSIS SUMMARY")
    print("=" * 80)
    print(f"Total samples: {total_samples}")
    print(f"Unique functions: {len(hotspots)}")
    print("")
    
    if not hotspots:
        print("No hotspots found.")
        return
    
    print(f"Top {min(top_n, len(hotspots))} Hotspots (by exclusive sample count):")
    print("-" * 80)
    print(f"{'Rank':<6} {'Function':<40} {'Exclusive %':<12} {'Inclusive %':<12}")
    print("-" * 80)
    
    for i, hotspot in enumerate(hotspots[:top_n], 1):
        func = hotspot['function']
        if len(func) > 38:
            func = func[:35] + "..."
        
        print(f"{i:<6} {func:<40} {hotspot['exclusive_percentage']:>10.2f}% {hotspot['inclusive_percentage']:>10.2f}%")
        
        # Print additional details for top 5
        if i <= 5:
            if hotspot['library']:
                lib = hotspot['library']
                if len(lib) > 50:
                    lib = "..." + lib[-47:]
                print(f"       Library: {lib}")
            if hotspot['filename'] and hotspot['filename'] != "":
                loc = hotspot['filename']
                if hotspot['line_number'] > 0:
                    loc += f":{hotspot['line_number']}"
                if len(loc) > 50:
                    loc = "..." + loc[-47:]
                print(f"       Location: {loc}")
            print("")
    
    print("=" * 80)

# ============================================================================
# Main
# ============================================================================

def parse_args():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='NPB Hotspot Analysis Script - Analyzes performance sampling data from PerFlow',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Analyze all sample data in results directory
  %(prog)s -r ./npb_results

  # Analyze specific benchmarks
  %(prog)s -r ./npb_results -b "cg ep ft"

  # Analyze with specific process scales
  %(prog)s -r ./npb_results -s "4 16 64"

Environment Variables:
  VERBOSE         Enable verbose output
        '''
    )
    
    parser.add_argument('-r', '--results-dir', 
                       default=os.environ.get('NPB_RESULTS_DIR', './npb_results'),
                       help='Results directory (default: ./npb_results)')
    parser.add_argument('-b', '--benchmarks',
                       help='Space-separated list of benchmarks (default: all found)')
    parser.add_argument('-c', '--class',
                       dest='problem_class',
                       default=os.environ.get('NPB_CLASS', ''),
                       help='Problem class filter (default: all)')
    parser.add_argument('-s', '--scales',
                       help='Space-separated list of process scales (default: all found)')
    parser.add_argument('--square-scales',
                       default=os.environ.get('NPB_PROCESS_SCALES_SQUARE', '1 4 9 16 36 64 121 256 484'),
                       help='Process scales for BT/SP (default: "1 4 9 16 36 64 121 256 484")')
    parser.add_argument('-o', '--output',
                       help='Output JSON file (default: <results_dir>/hotspot_analysis.json)')
    parser.add_argument('-t', '--top',
                       type=int,
                       default=20,
                       help='Number of top hotspots to display (default: 20)')
    
    # Support positional argument for backward compatibility
    parser.add_argument('results_dir_pos',
                       nargs='?',
                       help='Results directory (positional argument, overrides -r)')
    
    args = parser.parse_args()
    
    # Use positional argument if provided
    if args.results_dir_pos:
        args.results_dir = args.results_dir_pos
    
    # Parse benchmark list
    if args.benchmarks:
        args.benchmarks = args.benchmarks.split()
    else:
        args.benchmarks = []
    
    # Parse process scales
    if args.scales:
        args.scales = [int(x) for x in args.scales.split()]
    else:
        args.scales = []
    
    args.square_scales = [int(x) for x in args.square_scales.split()]
    
    # Set default output file
    if not args.output:
        args.output = os.path.join(args.results_dir, 'hotspot_analysis.json')
    
    return args

def main():
    args = parse_args()
    
    # Find the analyzer executable
    analyzer_path = get_analyzer_path()
    if not analyzer_path:
        print("Error: Cannot find npb_hotspot_analyzer executable", file=sys.stderr)
        print("Please build PerFlow first or set PERFLOW_BUILD_DIR environment variable", file=sys.stderr)
        print("", file=sys.stderr)
        print("Build instructions:", file=sys.stderr)
        print("  cd /path/to/PerFlow", file=sys.stderr)
        print("  mkdir -p build && cd build", file=sys.stderr)
        print("  cmake ..", file=sys.stderr)
        print("  make", file=sys.stderr)
        return 1
    
    print(f"Using analyzer: {analyzer_path}")
    print(f"Results directory: {args.results_dir}")
    
    # Find all sample data directories
    print("\nScanning for sample data directories...")
    sample_dirs = find_sample_data_dirs(
        args.results_dir,
        args.benchmarks,
        args.problem_class,
        args.scales,
        args.square_scales
    )
    
    if not sample_dirs:
        print("Error: No sample data directories found", file=sys.stderr)
        print(f"Expected location: {args.results_dir}/overhead/sample_data/", file=sys.stderr)
        print("Make sure you have run the overhead analysis first.", file=sys.stderr)
        return 1
    
    print(f"Found {len(sample_dirs)} sample data directories")
    
    # Analyze each directory
    print("\nAnalyzing sample data...")
    all_results = []
    temp_dir = os.path.join(args.results_dir, '.hotspot_temp')
    os.makedirs(temp_dir, exist_ok=True)
    
    for i, (sample_dir, benchmark, pclass, nprocs, iteration) in enumerate(sample_dirs, 1):
        print(f"  [{i}/{len(sample_dirs)}] {benchmark}.{pclass} np={nprocs} iter={iteration}...", end=' ')
        
        temp_output = os.path.join(temp_dir, f"{benchmark}_{pclass}_np{nprocs}_iter{iteration}.json")
        result = analyze_sample_dir(analyzer_path, sample_dir, temp_output)
        
        if result:
            all_results.append(result)
            print(f"OK ({result.get('total_samples', 0)} samples)")
        else:
            print("FAILED")
    
    if not all_results:
        print("\nError: No sample data was successfully analyzed", file=sys.stderr)
        return 1
    
    print(f"\nSuccessfully analyzed {len(all_results)}/{len(sample_dirs)} directories")
    
    # Aggregate results
    print("\nAggregating hotspot data...")
    aggregated = aggregate_hotspots(all_results)
    
    # Save aggregated results
    print(f"Saving results to: {args.output}")
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, 'w') as f:
        json.dump(aggregated, f, indent=2)
    
    # Print summary
    print_hotspot_summary(aggregated, args.top)
    
    print(f"\nHotspot analysis complete!")
    print(f"Results saved to: {args.output}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
