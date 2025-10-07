"""
Example demonstrating CallingContextTree visualization capabilities.

This example shows how to use the visualize() method to create:
1. Top-down tree view
2. Ring/sunburst chart
"""

from perflow.perf_data_struct.dynamic.profile.calling_context_tree import CallingContextTree
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData, ProfileInfo
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData


def main():
    """Demonstrate CCT visualization."""
    
    print("=" * 70)
    print("Calling Context Tree Visualization Example")
    print("=" * 70)
    
    # Create profile data
    profile = PerfData()
    profile_info = ProfileInfo(
        application_name="computation_app",
        sample_rate=1000.0,
        profile_format="simulation"
    )
    profile.setProfileInfo(profile_info)
    
    print("\n1. Creating sample profiling data...")
    
    # Add samples with various call stacks
    call_stacks = [
        (["main", "compute", "matrix_multiply"], 50, 50000.0, 25000.0),
        (["main", "compute", "vector_add"], 30, 15000.0, 7500.0),
        (["main", "io_read"], 20, 40000.0, 20000.0),
        (["main", "compute", "helper", "vector_add"], 15, 4500.0, 2250.0),
        (["main", "compute", "helper", "allocate"], 10, 2000.0, 1000.0),
        (["main", "initialize"], 25, 10000.0, 5000.0),
        (["main", "finalize"], 5, 1000.0, 500.0),
    ]
    
    for call_stack, count, cycles, instructions in call_stacks:
        for i in range(count):
            sample = SampleData()
            sample.setCallStack(call_stack)
            sample.setMetric("cycles", cycles)
            sample.setMetric("instructions", instructions)
            profile.addSample(sample)
    
    print(f"   Added {profile.getSampleCount()} samples")
    
    # Build CCT
    print("\n2. Building Calling Context Tree...")
    cct = CallingContextTree()
    cct.buildFromProfile(profile)
    
    print(f"   CCT has {cct.getNodeCount()} nodes")
    print(f"   Maximum calling depth: {cct.getContextDepth()}")
    
    # Generate visualizations
    print("\n3. Generating visualizations...")
    
    # Tree view
    print("   a) Creating top-down tree view...")
    try:
        cct.visualize(
            output_file="cct_tree_view.png",
            view_type="tree",
            metric="cycles",
            figsize=(14, 10),
            dpi=150
        )
        print("      ✓ Saved to: cct_tree_view.png")
    except Exception as e:
        print(f"      ✗ Error: {e}")
    
    # Ring chart
    print("   b) Creating ring/sunburst chart...")
    try:
        cct.visualize(
            output_file="cct_ring_chart.png",
            view_type="ring",
            metric="cycles",
            figsize=(12, 12),
            dpi=150
        )
        print("      ✓ Saved to: cct_ring_chart.png")
    except Exception as e:
        print(f"      ✗ Error: {e}")
    
    # Alternative metrics
    print("   c) Creating tree view with instructions metric...")
    try:
        cct.visualize(
            output_file="cct_tree_instructions.png",
            view_type="tree",
            metric="instructions",
            figsize=(14, 10),
            dpi=150
        )
        print("      ✓ Saved to: cct_tree_instructions.png")
    except Exception as e:
        print(f"      ✗ Error: {e}")
    
    # Show hottest paths
    print("\n4. Top 5 hottest calling paths (by cycles):")
    hot_paths = cct.getHotPaths("cycles", top_n=5)
    for i, (path, cycles) in enumerate(hot_paths, 1):
        path_str = " -> ".join(path)
        print(f"   {i}. {path_str}: {cycles:,.0f} cycles")
    
    print("\n" + "=" * 70)
    print("Visualization complete!")
    print("=" * 70)
    print("\nGenerated files:")
    print("  - cct_tree_view.png: Top-down tree visualization")
    print("  - cct_ring_chart.png: Ring/sunburst chart")
    print("  - cct_tree_instructions.png: Tree view with instructions metric")
    print("\nVisualization features:")
    print("  • Color intensity indicates metric value (red=high, yellow=low)")
    print("  • Node size in tree view proportional to inclusive metrics")
    print("  • Ring chart shows hierarchical structure in circular layout")
    print("  • Sample counts displayed on nodes (tree view)")


if __name__ == "__main__":
    main()
