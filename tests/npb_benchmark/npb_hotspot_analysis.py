#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
NPB Hotspot Analysis Script
Analyzes performance sampling data from PerFlow to identify hotspots
"""

import sys
import os
import json
import struct
import re
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Tuple

# ============================================================================
# Configuration
# ============================================================================

# Functions to exclude from analysis
EXCLUDED_FUNCTIONS = [
    '_start',
    '__libc_start_main',
    'main',
    # PAPI-related functions
    'PAPI_',
    'papi_',
    '_papi_',
    # Internal PerFlow functions (if any appear)
    'perflow_',
    '__perflow_',
]

# ============================================================================
# Sample Data Parsing
# ============================================================================

def should_exclude_function(func_name: str) -> bool:
    """Check if a function should be excluded from analysis"""
    for pattern in EXCLUDED_FUNCTIONS:
        if pattern in func_name:
            return True
    return False

def parse_sample_file(filepath: str) -> List[Dict]:
    """
    Parse PerFlow binary sample file (.pflw)
    Returns list of sample records
    """
    samples = []
    
    try:
        with open(filepath, 'rb') as f:
            while True:
                # Read sample header (simplified - adjust based on actual format)
                # This is a placeholder - actual format may differ
                header = f.read(16)
                if len(header) < 16:
                    break
                
                # Parse basic sample info
                sample = {
                    'timestamp': 0,
                    'stack': []
                }
                
                # Read call stack (placeholder implementation)
                # Actual parsing depends on PerFlow's binary format
                stack_size = struct.unpack('I', f.read(4))[0] if f.read(4) else 0
                for _ in range(min(stack_size, 100)):  # Cap at 100 frames
                    addr = struct.unpack('Q', f.read(8))[0] if f.read(8) else 0
                    if addr == 0:
                        break
                    sample['stack'].append(hex(addr))
                
                samples.append(sample)
                
    except Exception as e:
        print(f"Warning: Error parsing {filepath}: {e}", file=sys.stderr)
        return []
    
    return samples

def parse_text_sample_file(filepath: str) -> List[Dict]:
    """
    Parse PerFlow text sample file (.txt) for easier analysis
    Returns list of sample records with function names
    """
    samples = []
    current_sample = None
    
    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                
                # New sample marker
                if line.startswith('Sample') or line.startswith('---'):
                    if current_sample:
                        samples.append(current_sample)
                    current_sample = {'stack': []}
                
                # Stack frame line (contains function name)
                elif current_sample is not None and line:
                    # Extract function name from various formats
                    # Format: "  [function_name] at address"
                    # Format: "  address: function_name"
                    # Format: "  #N  address in function_name"
                    
                    func_match = re.search(r'\[([^\]]+)\]', line)
                    if func_match:
                        func_name = func_match.group(1)
                        current_sample['stack'].append(func_name)
                    else:
                        # Try other formats
                        parts = line.split()
                        if len(parts) >= 2:
                            # Use last meaningful part as function name
                            func_name = parts[-1]
                            if func_name and not func_name.startswith('0x'):
                                current_sample['stack'].append(func_name)
            
            if current_sample:
                samples.append(current_sample)
                
    except Exception as e:
        print(f"Warning: Error parsing {filepath}: {e}", file=sys.stderr)
        return []
    
    return samples

def analyze_samples(sample_dir: str) -> Dict[str, int]:
    """
    Analyze samples from a directory
    Returns dictionary of function names to exclusive sample counts
    """
    function_counts = defaultdict(int)
    
    sample_dir_path = Path(sample_dir)
    
    # Try text files first (easier to parse)
    text_files = list(sample_dir_path.glob('*.txt'))
    binary_files = list(sample_dir_path.glob('*.pflw'))
    
    if text_files:
        print(f"  Analyzing {len(text_files)} text sample files...")
        for filepath in text_files:
            samples = parse_text_sample_file(str(filepath))
            
            # Count exclusive samples (leaf function in stack)
            for sample in samples:
                if sample['stack']:
                    # Leaf function is first in stack (exclusive time)
                    leaf_func = sample['stack'][0]
                    if not should_exclude_function(leaf_func):
                        function_counts[leaf_func] += 1
    
    elif binary_files:
        print(f"  Analyzing {len(binary_files)} binary sample files...")
        print(f"  Note: Binary parsing is simplified, text files recommended")
        for filepath in binary_files:
            samples = parse_sample_file(str(filepath))
            
            # For binary files without symbols, count by address
            for sample in samples:
                if sample['stack']:
                    leaf_addr = sample['stack'][0]
                    function_counts[leaf_addr] += 1
    
    else:
        print(f"  Warning: No sample files found in {sample_dir}")
    
    return dict(function_counts)

# ============================================================================
# Hotspot Calculation
# ============================================================================

def calculate_hotspots(sample_dirs: List[str], top_n: int = 10) -> Dict:
    """
    Calculate hotspots from multiple sample directories
    Returns aggregated hotspot data
    """
    all_hotspots = {}
    
    for sample_dir in sample_dirs:
        if not os.path.exists(sample_dir):
            print(f"Warning: Sample directory not found: {sample_dir}")
            continue
        
        print(f"Analyzing: {sample_dir}")
        function_counts = analyze_samples(sample_dir)
        
        # Aggregate counts
        for func, count in function_counts.items():
            if func not in all_hotspots:
                all_hotspots[func] = 0
            all_hotspots[func] += count
    
    # Calculate percentages
    total_samples = sum(all_hotspots.values())
    
    if total_samples == 0:
        print("Warning: No samples found")
        return {}
    
    hotspot_list = []
    for func, count in all_hotspots.items():
        percentage = (count / total_samples) * 100
        hotspot_list.append({
            'function': func,
            'samples': count,
            'percentage': percentage
        })
    
    # Sort by sample count
    hotspot_list.sort(key=lambda x: x['samples'], reverse=True)
    
    return {
        'total_samples': total_samples,
        'hotspots': hotspot_list[:top_n],
        'all_functions': len(all_hotspots)
    }

# ============================================================================
# Main Analysis
# ============================================================================

def analyze_benchmark_hotspots(results_dir: str, output_file: str):
    """
    Main function to analyze hotspots for all benchmarks
    """
    results_dir_path = Path(results_dir)
    sample_data_dir = results_dir_path / 'overhead' / 'sample_data'
    
    if not sample_data_dir.exists():
        print(f"Error: Sample data directory not found: {sample_data_dir}")
        return
    
    print("NPB Hotspot Analysis")
    print("=" * 60)
    print(f"Sample data directory: {sample_data_dir}")
    print()
    
    # Group sample directories by benchmark and process count
    benchmark_samples = defaultdict(lambda: defaultdict(list))
    
    for sample_dir in sample_data_dir.iterdir():
        if sample_dir.is_dir():
            # Parse directory name: benchmark_CLASS_npNUM_iterN
            match = re.match(r'(\w+)_([A-Z])_np(\d+)_iter(\d+)', sample_dir.name)
            if match:
                benchmark, class_name, nprocs, iter_num = match.groups()
                key = f"{benchmark}_{class_name}"
                benchmark_samples[key][int(nprocs)].append(str(sample_dir))
    
    # Analyze hotspots for each benchmark and process count
    results = {
        'metadata': {
            'analysis_date': str(Path(results_dir).stat().st_mtime),
            'sample_data_dir': str(sample_data_dir)
        },
        'benchmarks': []
    }
    
    for benchmark_key in sorted(benchmark_samples.keys()):
        print(f"\nBenchmark: {benchmark_key}")
        print("-" * 60)
        
        benchmark_data = {
            'benchmark': benchmark_key,
            'process_scales': []
        }
        
        for nprocs in sorted(benchmark_samples[benchmark_key].keys()):
            sample_dirs = benchmark_samples[benchmark_key][nprocs]
            print(f"  Process scale: {nprocs} ({len(sample_dirs)} runs)")
            
            hotspot_data = calculate_hotspots(sample_dirs, top_n=10)
            
            if hotspot_data and hotspot_data['hotspots']:
                print(f"  Total samples: {hotspot_data['total_samples']}")
                print(f"  Top 5 hotspots:")
                for i, hotspot in enumerate(hotspot_data['hotspots'][:5], 1):
                    print(f"    {i}. {hotspot['function']}: "
                          f"{hotspot['percentage']:.2f}% ({hotspot['samples']} samples)")
            
            benchmark_data['process_scales'].append({
                'num_processes': nprocs,
                'total_samples': hotspot_data.get('total_samples', 0),
                'hotspots': hotspot_data.get('hotspots', [])
            })
        
        results['benchmarks'].append(benchmark_data)
    
    # Save results to JSON
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)
    
    print()
    print("=" * 60)
    print(f"Hotspot analysis saved to: {output_file}")

# ============================================================================
# Command Line Interface
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 npb_hotspot_analysis.py <results_dir> [output_file]")
        print()
        print("Arguments:")
        print("  results_dir   Directory containing NPB benchmark results")
        print("  output_file   Output JSON file (default: results_dir/hotspot_analysis.json)")
        print()
        print("Example:")
        print("  python3 npb_hotspot_analysis.py ./npb_results")
        sys.exit(1)
    
    results_dir = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else os.path.join(results_dir, 'hotspot_analysis.json')
    
    analyze_benchmark_hotspots(results_dir, output_file)

if __name__ == '__main__':
    main()
