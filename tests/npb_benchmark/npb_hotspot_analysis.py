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
from pathlib import Path
from typing import Dict, List

# ============================================================================
# Configuration
# ============================================================================

# Path to the npb_hotspot_analyzer executable
def get_analyzer_path():
    """Find the npb_hotspot_analyzer executable"""
    # Try several possible locations
    possible_paths = [
        "../../build/examples/npb_hotspot_analyzer",
        "../../../build/examples/npb_hotspot_analyzer",
        "../../examples/npb_hotspot_analyzer",
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

# ============================================================================
# Hotspot Analysis
# ============================================================================

def analyze_hotspots(sample_data_dir: str, output_file: str = None) -> Dict:
    """
    Analyze hotspots using the npb_hotspot_analyzer tool
    
    Args:
        sample_data_dir: Directory containing PerFlow sample data files
        output_file: Optional output file path (default: sample_data_dir/hotspot_analysis.json)
    
    Returns:
        Dictionary containing hotspot analysis results
    """
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
        sys.exit(1)
    
    print(f"Using analyzer: {analyzer_path}")
    
    # Set output file
    if output_file is None:
        output_file = os.path.join(sample_data_dir, "hotspot_analysis.json")
    
    # Check if sample data directory exists
    if not os.path.isdir(sample_data_dir):
        print(f"Error: Sample data directory not found: {sample_data_dir}", file=sys.stderr)
        sys.exit(1)
    
    # Count number of rank files
    rank_files = list(Path(sample_data_dir).glob("perflow_mpi_rank_*.pflw"))
    if not rank_files:
        print(f"Error: No sample data files found in {sample_data_dir}", file=sys.stderr)
        print("Expected files: perflow_mpi_rank_*.pflw", file=sys.stderr)
        sys.exit(1)
    
    num_ranks = len(rank_files)
    print(f"Found {num_ranks} rank file(s) in {sample_data_dir}")
    
    # Run the analyzer
    print(f"\nRunning hotspot analysis...")
    try:
        result = subprocess.run(
            [analyzer_path, sample_data_dir, output_file, str(num_ranks)],
            capture_output=True,
            text=True,
            check=True
        )
        
        # Print analyzer output
        if result.stdout:
            print(result.stdout)
        
        if result.stderr:
            print(result.stderr, file=sys.stderr)
    
    except subprocess.CalledProcessError as e:
        print(f"Error: Hotspot analyzer failed with exit code {e.returncode}", file=sys.stderr)
        if e.stdout:
            print("STDOUT:", file=sys.stderr)
            print(e.stdout, file=sys.stderr)
        if e.stderr:
            print("STDERR:", file=sys.stderr)
            print(e.stderr, file=sys.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print(f"Error: Cannot execute {analyzer_path}", file=sys.stderr)
        sys.exit(1)
    
    # Load and return the results
    try:
        with open(output_file, 'r') as f:
            results = json.load(f)
        print(f"\nHotspot analysis complete. Results saved to: {output_file}")
        return results
    except Exception as e:
        print(f"Error: Failed to load results from {output_file}: {e}", file=sys.stderr)
        sys.exit(1)

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

def main():
    if len(sys.argv) < 2:
        print("Usage: npb_hotspot_analysis.py <sample_data_dir> [output_file]")
        print("")
        print("Arguments:")
        print("  sample_data_dir   Directory containing perflow_mpi_rank_*.pflw files")
        print("  output_file       Optional output JSON file (default: sample_data_dir/hotspot_analysis.json)")
        print("")
        print("Example:")
        print("  python3 npb_hotspot_analysis.py ./npb_results/overhead/sample_data")
        sys.exit(1)
    
    sample_data_dir = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    # Analyze hotspots
    results = analyze_hotspots(sample_data_dir, output_file)
    
    # Print summary
    print_hotspot_summary(results, top_n=20)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
