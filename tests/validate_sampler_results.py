#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
validate_sampler_results.py - Validation script for MPI sampler output

This script parses sampler output files and validates that expected patterns
are present for each test type. It generates a detailed report with statistics
and identifies any issues.
"""

import sys
import os
import glob
import re
from pathlib import Path
from typing import Dict, List, Tuple

# ============================================================================
# Configuration
# ============================================================================

# Expected patterns for each test type
TEST_EXPECTATIONS = {
    'mpi_sampler_test': {
        'description': 'Basic MPI Sampler Test',
        'expected_patterns': [
            r'Matrix multiplication',
            r'Vector operations',
            r'MPI collectives',
            r'completed successfully'
        ],
        'min_samples': 10
    },
    'test_compute_intensive': {
        'description': 'Compute-Intensive Workload',
        'expected_patterns': [
            r'Matrix multiplication',
            r'FFT operations',
            r'Trigonometric ops',
            r'Jacobi iteration',
            r'completed successfully'
        ],
        'min_samples': 50,
        'expected_counters': ['high_fp_ops', 'high_instructions']
    },
    'test_comm_intensive': {
        'description': 'Communication-Intensive Workload',
        'expected_patterns': [
            r'Ring communication',
            r'Mesh communication',
            r'Collective operations',
            r'Non-blocking',
            r'Alltoall',
            r'completed successfully'
        ],
        'min_samples': 20,
        'expected_counters': ['high_mpi_calls']
    },
    'test_memory_intensive': {
        'description': 'Memory-Intensive Workload',
        'expected_patterns': [
            r'Repeated allocations',
            r'Streaming access',
            r'Random access',
            r'Cache-friendly',
            r'Cache-unfriendly',
            r'Memory copy',
            r'completed successfully'
        ],
        'min_samples': 30,
        'expected_counters': ['cache_misses']
    },
    'test_hybrid': {
        'description': 'Hybrid Workload',
        'expected_patterns': [
            r'Alternating',
            r'Overlapped',
            r'Load imbalance',
            r'Stencil',
            r'Pipeline',
            r'completed successfully'
        ],
        'min_samples': 25
    },
    'test_stress': {
        'description': 'Stress Test',
        'expected_patterns': [
            r'Mixed.*frequency',
            r'Long running',
            r'Memory stress',
            r'completed successfully'
        ],
        'min_samples': 100
    }
}

# ============================================================================
# Helper Functions
# ============================================================================

def print_header(title: str):
    """Print a formatted header"""
    print("=" * 70)
    print(f"  {title}")
    print("=" * 70)

def print_section(title: str):
    """Print a section separator"""
    print(f"\n--- {title} ---")

def check_output_file(filepath: str, expected_patterns: List[str]) -> Tuple[bool, List[str]]:
    """
    Check if output file contains expected patterns
    Returns: (success, missing_patterns)
    """
    if not os.path.exists(filepath):
        return False, ["File not found"]
    
    try:
        with open(filepath, 'r') as f:
            content = f.read()
    except Exception as e:
        return False, [f"Error reading file: {e}"]
    
    missing = []
    for pattern in expected_patterns:
        if not re.search(pattern, content, re.IGNORECASE):
            missing.append(pattern)
    
    return len(missing) == 0, missing

def check_sample_files(output_dir: str, test_name: str, num_ranks: int) -> Tuple[bool, Dict]:
    """
    Check if sample files exist and contain data
    Returns: (success, stats)
    """
    stats = {
        'files_found': 0,
        'files_with_data': 0,
        'total_samples': 0,
        'sample_counts': []
    }
    
    for rank in range(num_ranks):
        # Check binary format (.pflw)
        binary_file = os.path.join(output_dir, f"{test_name}_rank_{rank}.pflw")
        text_file = os.path.join(output_dir, f"{test_name}_rank_{rank}.txt")
        
        if os.path.exists(binary_file):
            stats['files_found'] += 1
            file_size = os.path.getsize(binary_file)
            if file_size > 0:
                stats['files_with_data'] += 1
                # Estimate samples (rough approximation)
                estimated_samples = file_size // 100  # Rough estimate
                stats['total_samples'] += estimated_samples
                stats['sample_counts'].append(estimated_samples)
        
        # Also check text format
        if os.path.exists(text_file):
            try:
                with open(text_file, 'r') as f:
                    lines = f.readlines()
                    # Count sample entries in text file
                    sample_count = len([l for l in lines if 'Sample' in l or 'Count:' in l])
                    if sample_count > 0:
                        stats['sample_counts'].append(sample_count)
            except:
                pass
    
    success = stats['files_found'] >= num_ranks and stats['files_with_data'] >= num_ranks
    return success, stats

def validate_test(output_dir: str, test_name: str, expectations: Dict, num_ranks: int = 4) -> Dict:
    """
    Validate a single test's output
    Returns: validation results dictionary
    """
    results = {
        'test_name': test_name,
        'description': expectations['description'],
        'passed': True,
        'issues': []
    }
    
    # Check output file
    output_file = os.path.join(output_dir, f"{test_name}_output.txt")
    patterns_ok, missing = check_output_file(output_file, expectations['expected_patterns'])
    
    if not patterns_ok:
        results['passed'] = False
        results['issues'].append(f"Missing patterns in output: {', '.join(missing)}")
    
    # Check sample files
    samples_ok, stats = check_sample_files(output_dir, test_name, num_ranks)
    
    if not samples_ok:
        results['passed'] = False
        results['issues'].append(f"Sample files issue: found {stats['files_found']}/{num_ranks}, "
                                f"with data {stats['files_with_data']}/{num_ranks}")
    
    # Check minimum sample count
    if 'min_samples' in expectations:
        min_samples = expectations['min_samples']
        if stats['total_samples'] < min_samples:
            results['passed'] = False
            results['issues'].append(f"Too few samples: {stats['total_samples']} < {min_samples}")
    
    # Store statistics
    results['stats'] = stats
    
    return results

# ============================================================================
# Main Validation Function
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: validate_sampler_results.py <output_directory>")
        sys.exit(1)
    
    output_dir = sys.argv[1]
    
    if not os.path.exists(output_dir):
        print(f"Error: Output directory not found: {output_dir}")
        sys.exit(1)
    
    print_header("MPI Sampler Test Validation Report")
    print(f"\nOutput Directory: {output_dir}")
    print(f"Date: {os.popen('date').read().strip()}\n")
    
    # Validate each test
    all_results = []
    total_tests = 0
    passed_tests = 0
    
    for test_name, expectations in TEST_EXPECTATIONS.items():
        total_tests += 1
        
        print_section(f"Validating: {expectations['description']}")
        
        result = validate_test(output_dir, test_name, expectations)
        all_results.append(result)
        
        if result['passed']:
            passed_tests += 1
            print(f"✓ PASSED")
            print(f"  - Files found: {result['stats']['files_found']}")
            print(f"  - Files with data: {result['stats']['files_with_data']}")
            print(f"  - Total samples: {result['stats']['total_samples']}")
            if result['stats']['sample_counts']:
                avg_samples = sum(result['stats']['sample_counts']) / len(result['stats']['sample_counts'])
                print(f"  - Average samples per rank: {avg_samples:.1f}")
        else:
            print(f"✗ FAILED")
            for issue in result['issues']:
                print(f"  - Issue: {issue}")
    
    # Summary
    print_header("Validation Summary")
    
    print(f"\nTotal Tests: {total_tests}")
    print(f"Passed: {passed_tests}")
    print(f"Failed: {total_tests - passed_tests}")
    
    if passed_tests == total_tests:
        print("\n✓ All validations passed!")
        print("\nKey Observations:")
        print("- All test programs completed successfully")
        print("- Sample files generated for all ranks")
        print("- Expected patterns found in all outputs")
        print("- Minimum sample counts met")
        
        return 0
    else:
        print("\n✗ Some validations failed.")
        print("\nFailed Tests:")
        for result in all_results:
            if not result['passed']:
                print(f"  - {result['description']}")
                for issue in result['issues']:
                    print(f"    • {issue}")
        
        return 1
    
    # Additional statistics
    print("\n" + "=" * 70)
    print("  Additional Information")
    print("=" * 70)
    
    # List all files in output directory
    print("\nOutput Files:")
    all_files = sorted(glob.glob(os.path.join(output_dir, "*")))
    for f in all_files:
        size = os.path.getsize(f)
        print(f"  - {os.path.basename(f)} ({size} bytes)")
    
    print("\nValidation complete.")
    
    return 0 if passed_tests == total_tests else 1

if __name__ == "__main__":
    sys.exit(main())
