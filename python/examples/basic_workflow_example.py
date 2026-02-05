#!/usr/bin/env python3
"""
Example: Basic Performance Analysis Workflow

This script demonstrates how to use the PerFlow Python frontend
to analyze performance data from sample files.

IMPORTANT: Before running this script, ensure the Python bindings are built
and accessible:
  1. Build PerFlow with: cd build && cmake .. && make
  2. Set PYTHONPATH: export PYTHONPATH=/path/to/PerFlow/build/python:$PYTHONPATH
  3. Or install with: cd build && make install
"""

import sys
import os

# Add parent directory to path for development
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import perflow
from perflow.workflows import create_basic_analysis_workflow


def main():
    """Run basic analysis workflow."""
    if len(sys.argv) < 3:
        print("Usage: basic_workflow_example.py <sample_dir> <output_dir>")
        print()
        print("Example:")
        print("  python3 basic_workflow_example.py /tmp/samples /tmp/results")
        sys.exit(1)
    
    sample_dir = sys.argv[1]
    output_dir = sys.argv[2]
    
    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)
    
    print("=" * 80)
    print("PerFlow Basic Analysis Workflow Example")
    print("=" * 80)
    print(f"Sample directory: {sample_dir}")
    print(f"Output directory: {output_dir}")
    print()
    
    # Discover sample files
    print("Discovering sample files...")
    sample_files = []
    libmap_files = []
    
    for filename in os.listdir(sample_dir):
        filepath = os.path.join(sample_dir, filename)
        
        # Look for .pflw sample files
        if filename.endswith('.pflw') and 'rank_' in filename:
            # Extract rank ID from filename
            try:
                rank_part = filename.split('rank_')[1]
                rank_id = int(rank_part.split('.')[0])
                sample_files.append((filepath, rank_id))
                print(f"  Found sample file: {filename} (rank {rank_id})")
            except (IndexError, ValueError):
                print(f"  Warning: Could not parse rank ID from {filename}")
        
        # Look for .libmap files
        elif filename.endswith('.libmap') and 'rank_' in filename:
            try:
                rank_part = filename.split('rank_')[1]
                rank_id = int(rank_part.split('.')[0])
                libmap_files.append((filepath, rank_id))
                print(f"  Found libmap file: {filename} (rank {rank_id})")
            except (IndexError, ValueError):
                print(f"  Warning: Could not parse rank ID from {filename}")
    
    if not sample_files:
        print("Error: No sample files found in", sample_dir)
        sys.exit(1)
    
    print(f"\nFound {len(sample_files)} sample files")
    if libmap_files:
        print(f"Found {len(libmap_files)} library map files")
    print()
    
    # Create workflow
    print("Creating workflow...")
    workflow = create_basic_analysis_workflow(
        sample_files=sample_files,
        output_dir=output_dir,
        libmap_files=libmap_files if libmap_files else None,
        top_n=10,
        parallel=True
    )
    
    print(f"Workflow created with {len(workflow.nodes)} nodes and {len(workflow.edges)} edges")
    print()
    
    # Save workflow visualization
    workflow_viz = os.path.join(output_dir, "workflow.dot")
    workflow.save_visualization(workflow_viz)
    print(f"Workflow visualization saved to: {workflow_viz}")
    print()
    
    # Execute workflow
    print("Executing workflow...")
    print("-" * 80)
    
    try:
        results = workflow.execute()
        print("-" * 80)
        print("Workflow execution completed successfully!")
        print()
        
        # Print summary
        print("Results:")
        print(f"  - Performance tree visualization: {output_dir}/performance_tree.pdf")
        print(f"  - Analysis report: {output_dir}/analysis_report.txt")
        print(f"  - Workflow graph: {output_dir}/workflow.dot")
        print()
        
        # Show a preview of the report
        report_path = os.path.join(output_dir, "analysis_report.txt")
        if os.path.exists(report_path):
            print("Report preview:")
            print("-" * 80)
            with open(report_path, 'r') as f:
                lines = f.readlines()
                for line in lines[:30]:  # Show first 30 lines
                    print(line, end='')
                if len(lines) > 30:
                    print(f"\n... ({len(lines) - 30} more lines)")
            print("-" * 80)
        
    except Exception as e:
        print(f"Error executing workflow: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    
    print()
    print("Analysis complete!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
