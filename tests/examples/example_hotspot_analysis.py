"""
Example: Profile Hotspot Analysis

This example demonstrates how to use PerFlow to analyze profiling data
and identify performance hotspots in an application.
"""

from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData, ProfileInfo
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData
from perflow.task.profile_analysis.hotspot_analyzer import HotspotAnalyzer
from perflow.flow.flow import FlowGraph


def create_sample_profile_data():
    """
    Create sample profiling data for demonstration.
    
    Simulates profiling data from a scientific computing application
    with multiple functions showing different performance characteristics.
    """
    profile = PerfData()
    
    # Set profile metadata
    profile_info = ProfileInfo(
        pid=1000,
        tid=0,
        num_processes=1,
        num_threads=4,
        profile_format="synthetic",
        sample_rate=1000.0,  # 1000 Hz
        profile_start_time=0.0,
        profile_end_time=10.0,
        application_name="scientific_compute"
    )
    profile_info.addMetric("cycles")
    profile_info.addMetric("instructions")
    profile_info.addMetric("cache_misses")
    
    profile.setProfileInfo(profile_info)
    
    # Define functions with different performance profiles
    # Format: (function_name, num_samples, cycles_per_sample, instructions_per_sample, cache_misses_per_sample)
    functions = [
        ("main", 50, 10000.0, 5000.0, 100.0),
        ("matrix_multiply", 1000, 50000.0, 25000.0, 5000.0),  # Compute intensive
        ("vector_add", 200, 15000.0, 8000.0, 500.0),
        ("io_read", 150, 30000.0, 5000.0, 1000.0),  # I/O intensive
        ("io_write", 100, 25000.0, 4000.0, 800.0),  # I/O intensive
        ("data_transform", 500, 20000.0, 10000.0, 1500.0),
        ("helper_init", 20, 5000.0, 2500.0, 50.0),
        ("helper_cleanup", 10, 3000.0, 1500.0, 30.0),
        ("parse_input", 30, 8000.0, 4000.0, 200.0),
        ("validate_output", 40, 12000.0, 6000.0, 300.0),
    ]
    
    # Generate samples
    for func_name, count, cycles, instructions, cache_misses in functions:
        for i in range(count):
            sample = SampleData(
                timestamp=float(i) * 0.001,  # Samples at 1ms intervals
                pid=1000,
                tid=i % 4,  # Distribute across 4 threads
                function_name=func_name,
                module_name="libcompute.so" if "matrix" in func_name or "vector" in func_name else "main"
            )
            
            # Add performance metrics with some variation
            import random
            variation = random.uniform(0.9, 1.1)
            sample.setMetric("cycles", cycles * variation)
            sample.setMetric("instructions", instructions * variation)
            sample.setMetric("cache_misses", cache_misses * variation)
            
            profile.addSample(sample)
    
    return profile


def analyze_hotspots(profile):
    """
    Analyze profile data to identify performance hotspots.
    
    Args:
        profile: PerfData object to analyze
        
    Returns:
        HotspotAnalyzer with results
    """
    analyzer = HotspotAnalyzer(profile, threshold=0.0)
    analyzer.analyze()
    
    return analyzer


def print_analysis_results(analyzer):
    """
    Print hotspot analysis results in a readable format.
    
    Args:
        analyzer: HotspotAnalyzer with analysis results
    """
    print("=" * 80)
    print("HOTSPOT ANALYSIS RESULTS")
    print("=" * 80)
    
    total_samples = analyzer.getTotalSamples()
    print(f"\nTotal samples analyzed: {total_samples}")
    
    # Print top hotspots by cycles
    print("\n" + "-" * 80)
    print("TOP 5 HOTSPOTS BY TOTAL CYCLES")
    print("-" * 80)
    
    top_hotspots = analyzer.getTopHotspots("total_cycles", top_n=5)
    
    print(f"{'Rank':<6} {'Function':<25} {'Samples':<10} {'Cycles':<15} {'% Time':<10}")
    print("-" * 80)
    
    for rank, (func_name, metrics) in enumerate(top_hotspots, 1):
        sample_count = int(metrics["sample_count"])
        total_cycles = metrics["total_cycles"]
        percentage = analyzer.getHotspotPercentage(func_name)
        
        print(f"{rank:<6} {func_name:<25} {sample_count:<10} {total_cycles:>14,.0f} {percentage:>8.2f}%")
    
    # Print cache miss hotspots
    print("\n" + "-" * 80)
    print("TOP 5 HOTSPOTS BY CACHE MISSES")
    print("-" * 80)
    
    cache_hotspots = analyzer.getTopHotspots("total_cache_misses", top_n=5)
    
    print(f"{'Rank':<6} {'Function':<25} {'Samples':<10} {'Cache Misses':<15}")
    print("-" * 80)
    
    for rank, (func_name, metrics) in enumerate(cache_hotspots, 1):
        sample_count = int(metrics["sample_count"])
        cache_misses = metrics.get("total_cache_misses", 0.0)
        
        print(f"{rank:<6} {func_name:<25} {sample_count:<10} {cache_misses:>14,.0f}")
    
    # Print detailed statistics for top hotspot
    print("\n" + "-" * 80)
    print("DETAILED STATISTICS FOR TOP HOTSPOT")
    print("-" * 80)
    
    if top_hotspots:
        top_func, top_metrics = top_hotspots[0]
        print(f"\nFunction: {top_func}")
        print(f"  Samples: {int(top_metrics['sample_count'])}")
        print(f"  Percentage of execution time: {analyzer.getHotspotPercentage(top_func):.2f}%")
        print(f"  Total cycles: {top_metrics['total_cycles']:,.0f}")
        print(f"  Total instructions: {top_metrics['total_instructions']:,.0f}")
        
        if "total_cache_misses" in top_metrics:
            print(f"  Total cache misses: {top_metrics['total_cache_misses']:,.0f}")
        
        # Calculate IPC (Instructions Per Cycle)
        if top_metrics['total_cycles'] > 0:
            ipc = top_metrics['total_instructions'] / top_metrics['total_cycles']
            print(f"  Average IPC: {ipc:.2f}")
    
    # Identify optimization opportunities
    print("\n" + "-" * 80)
    print("OPTIMIZATION RECOMMENDATIONS")
    print("-" * 80)
    
    high_cycle_hotspots = analyzer.getHotspotsByThreshold("total_cycles", threshold=5000000.0)
    high_cache_hotspots = analyzer.getHotspotsByThreshold("total_cache_misses", threshold=500000.0)
    
    if high_cycle_hotspots:
        print("\nFunctions with high cycle count (potential compute bottlenecks):")
        for func_name, metrics in high_cycle_hotspots[:3]:
            print(f"  - {func_name}: {metrics['total_cycles']:,.0f} cycles")
    
    if high_cache_hotspots:
        print("\nFunctions with high cache misses (potential memory bottlenecks):")
        for func_name, metrics in high_cache_hotspots[:3]:
            cache_misses = metrics.get("total_cache_misses", 0.0)
            print(f"  - {func_name}: {cache_misses:,.0f} cache misses")
    
    print("\n" + "=" * 80)


def run_workflow_example():
    """
    Demonstrate using FlowGraph for hotspot analysis workflow.
    """
    print("\n" + "=" * 80)
    print("WORKFLOW-BASED HOTSPOT ANALYSIS")
    print("=" * 80)
    
    # Create profile data
    profile = create_sample_profile_data()
    
    # Create workflow
    workflow = FlowGraph()
    
    # Add analysis node
    analyzer = HotspotAnalyzer()
    analyzer.get_inputs().add_data(profile)
    
    # Run workflow
    print("\nRunning analysis workflow...")
    workflow.add_node(analyzer)
    workflow.run()
    
    # Get results
    print(f"\nAnalysis complete!")
    print(f"Total samples: {analyzer.getTotalSamples()}")
    print(f"Hotspots identified: {len(analyzer.getHotspots())}")
    
    top_hotspot = analyzer.getTopHotspots("total_cycles", top_n=1)
    if top_hotspot:
        func_name, metrics = top_hotspot[0]
        print(f"Top hotspot: {func_name} ({analyzer.getHotspotPercentage(func_name):.2f}% of execution time)")


def main():
    """
    Main function demonstrating profile hotspot analysis.
    """
    print("\n" + "=" * 80)
    print("PERFLOW EXAMPLE: PROFILE HOTSPOT ANALYSIS")
    print("=" * 80)
    print("\nThis example demonstrates identifying performance hotspots")
    print("from profiling data.")
    
    # Step 1: Create sample profile data
    print("\nStep 1: Creating sample profile data...")
    profile = create_sample_profile_data()
    print(f"  Created profile with {profile.getSampleCount()} samples")
    
    profile_info = profile.getProfileInfo()
    print(f"  Application: {profile_info.getApplicationName()}")
    print(f"  Sample rate: {profile_info.getSampleRate()} Hz")
    print(f"  Duration: {profile_info.getProfileDuration()} seconds")
    
    # Step 2: Analyze for hotspots
    print("\nStep 2: Analyzing for performance hotspots...")
    analyzer = analyze_hotspots(profile)
    
    # Step 3: Display results
    print("\nStep 3: Analysis complete!")
    print_analysis_results(analyzer)
    
    # Step 4: Demonstrate workflow
    run_workflow_example()
    
    print("\nExample complete!")


if __name__ == "__main__":
    main()
