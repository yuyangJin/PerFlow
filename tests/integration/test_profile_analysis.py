"""
Integration tests for profile analysis workflow

This module contains integration tests that verify the complete workflow
of profile data creation, analysis, and result generation.
"""

import pytest
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData, ProfileInfo
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData
from perflow.task.profile_analysis.profile_analyzer import ProfileAnalyzer
from perflow.task.profile_analysis.hotspot_analyzer import HotspotAnalyzer
from perflow.flow.flow import FlowGraph, FlowNode


class TestProfileAnalysisWorkflow:
    """Integration tests for profile analysis workflow."""
    
    def test_basic_profile_analysis_workflow(self):
        """Test a basic profile analysis workflow."""
        # Create profile data
        profile = PerfData()
        profile_info = ProfileInfo(
            pid=1000,
            application_name="test_app",
            profile_format="test"
        )
        profile.setProfileInfo(profile_info)
        
        # Add samples
        for i in range(50):
            sample = SampleData(
                timestamp=float(i),
                function_name=f"func{i % 5}"
            )
            sample.setMetric("cycles", float((i % 5 + 1) * 1000))
            profile.addSample(sample)
        
        # Create and run analyzer
        analyzer = ProfileAnalyzer(profile)
        
        # Track callback invocations
        invoked_samples = []
        
        def track_callback(sample):
            invoked_samples.append(sample)
        
        analyzer.registerCallback("tracker", track_callback)
        analyzer.analyze()
        
        # Verify results
        assert len(invoked_samples) == 50
    
    def test_hotspot_analysis_workflow(self):
        """Test hotspot analysis workflow."""
        # Create profile with known hotspots
        profile = PerfData()
        
        # Function 1: High cycle count (hotspot)
        for i in range(100):
            sample = SampleData(function_name="hotspot_func")
            sample.setMetric("cycles", 10000.0)
            profile.addSample(sample)
        
        # Function 2: Low cycle count
        for i in range(50):
            sample = SampleData(function_name="normal_func")
            sample.setMetric("cycles", 1000.0)
            profile.addSample(sample)
        
        # Create and run analyzer
        analyzer = HotspotAnalyzer(profile)
        analyzer.analyze()
        
        # Verify hotspot detection
        hotspots = analyzer.getHotspots()
        assert len(hotspots) == 2
        assert "hotspot_func" in hotspots
        assert "normal_func" in hotspots
        
        # Verify top hotspot
        top = analyzer.getTopHotspots("total_cycles", top_n=1)
        assert len(top) == 1
        assert top[0][0] == "hotspot_func"
    
    def test_profile_analysis_with_flow_graph(self):
        """Test profile analysis using FlowGraph."""
        # Create profile data
        profile = PerfData()
        for i in range(30):
            sample = SampleData(function_name=f"func{i % 3}")
            sample.setMetric("cycles", float(i * 100))
            profile.addSample(sample)
        
        # Create workflow
        workflow = FlowGraph()
        
        # Create analyzer node
        analyzer = HotspotAnalyzer()
        analyzer.get_inputs().add_data(profile)
        
        # Add node and run workflow
        workflow.add_node(analyzer)
        workflow.run()
        
        # Verify analysis was performed
        assert analyzer.getTotalSamples() == 30
        assert len(analyzer.getHotspots()) == 3
    
    def test_multi_profile_analysis(self):
        """Test analyzing multiple profiles."""
        profiles = []
        
        # Create multiple profiles
        for p in range(3):
            profile = PerfData()
            profile_info = ProfileInfo(pid=1000 + p)
            profile.setProfileInfo(profile_info)
            
            for i in range(20):
                sample = SampleData(function_name=f"func{i % 4}")
                sample.setMetric("cycles", float(i * 100))
                profile.addSample(sample)
            
            profiles.append(profile)
        
        # Analyze each profile
        results = []
        for profile in profiles:
            analyzer = HotspotAnalyzer(profile)
            analyzer.analyze()
            results.append({
                "total_samples": analyzer.getTotalSamples(),
                "num_hotspots": len(analyzer.getHotspots())
            })
        
        # Verify all profiles were analyzed
        assert len(results) == 3
        for result in results:
            assert result["total_samples"] == 20
            assert result["num_hotspots"] == 4
    
    def test_profile_analysis_with_metrics(self):
        """Test profile analysis with multiple metrics."""
        profile = PerfData()
        
        # Add samples with multiple metrics
        for i in range(40):
            sample = SampleData(function_name=f"func{i % 4}")
            sample.setMetric("cycles", float(i * 1000))
            sample.setMetric("instructions", float(i * 500))
            sample.setMetric("cache_misses", float(i * 10))
            profile.addSample(sample)
        
        # Analyze
        analyzer = HotspotAnalyzer(profile)
        analyzer.analyze()
        
        # Check all metrics were captured
        hotspots = analyzer.getHotspots()
        for func_name, metrics in hotspots.items():
            assert "total_cycles" in metrics
            assert "total_instructions" in metrics
            assert "total_cache_misses" in metrics
            assert metrics["total_cycles"] > 0
            assert metrics["total_instructions"] > 0
            assert metrics["total_cache_misses"] > 0
    
    def test_profile_analysis_with_threshold_filtering(self):
        """Test profile analysis with threshold-based filtering."""
        profile = PerfData()
        
        # Add samples with varying cycle counts
        functions = [
            ("low_cycles", 10, 100.0),
            ("medium_cycles", 20, 5000.0),
            ("high_cycles", 30, 10000.0),
        ]
        
        for func_name, count, cycles in functions:
            for i in range(count):
                sample = SampleData(function_name=func_name)
                sample.setMetric("cycles", cycles)
                profile.addSample(sample)
        
        # Analyze with threshold
        threshold = 150000.0  # Only high_cycles exceeds this
        analyzer = HotspotAnalyzer(profile, threshold=threshold)
        analyzer.analyze()
        
        # Get hotspots above threshold
        high_hotspots = analyzer.getHotspotsByThreshold("total_cycles")
        
        # Verify filtering
        assert len(high_hotspots) == 1
        assert high_hotspots[0][0] == "high_cycles"


class TestProfileAnalysisPipeline:
    """Integration tests for profile analysis pipelines."""
    
    def test_sequential_analysis_pipeline(self):
        """Test a pipeline with sequential analysis steps."""
        # Create profile
        profile = PerfData()
        for i in range(50):
            sample = SampleData(function_name=f"func{i % 5}")
            sample.setMetric("cycles", float(i * 100))
            profile.addSample(sample)
        
        # Step 1: Basic analysis
        analyzer1 = ProfileAnalyzer(profile)
        sample_count = [0]
        
        def count_samples(sample):
            sample_count[0] += 1
        
        analyzer1.registerCallback("counter", count_samples)
        analyzer1.analyze()
        
        assert sample_count[0] == 50
        
        # Step 2: Hotspot analysis
        analyzer2 = HotspotAnalyzer(profile)
        analyzer2.analyze()
        
        assert analyzer2.getTotalSamples() == 50
        assert len(analyzer2.getHotspots()) == 5
    
    def test_profile_aggregation_workflow(self):
        """Test profile data aggregation workflow."""
        profile = PerfData()
        
        # Add many samples to test aggregation
        for func_idx in range(5):
            func_name = f"function_{func_idx}"
            for sample_idx in range(20):
                sample = SampleData(function_name=func_name)
                sample.setMetric("cycles", 1000.0 * (func_idx + 1))
                sample.setMetric("instructions", 500.0 * (func_idx + 1))
                profile.addSample(sample)
        
        # Aggregate using PerfData
        aggregated = profile.aggregateByFunction()
        
        # Verify aggregation
        assert len(aggregated) == 5
        for func_idx in range(5):
            func_name = f"function_{func_idx}"
            assert func_name in aggregated
            assert aggregated[func_name]["cycles"] == 1000.0 * (func_idx + 1) * 20
        
        # Also verify through hotspot analyzer
        analyzer = HotspotAnalyzer(profile)
        analyzer.analyze()
        hotspots = analyzer.getHotspots()
        
        assert len(hotspots) == 5
        for func_idx in range(5):
            func_name = f"function_{func_idx}"
            assert hotspots[func_name]["sample_count"] == 20
            assert hotspots[func_name]["total_cycles"] == 1000.0 * (func_idx + 1) * 20
    
    def test_profile_comparison_workflow(self):
        """Test comparing profiles from different runs."""
        # Create two profiles with different characteristics
        profile1 = PerfData()
        profile_info1 = ProfileInfo(application_name="run1")
        profile1.setProfileInfo(profile_info1)
        
        profile2 = PerfData()
        profile_info2 = ProfileInfo(application_name="run2")
        profile2.setProfileInfo(profile_info2)
        
        # Profile 1: Function A is hotspot
        for i in range(100):
            sample = SampleData(function_name="func_a")
            sample.setMetric("cycles", 10000.0)
            profile1.addSample(sample)
        
        for i in range(20):
            sample = SampleData(function_name="func_b")
            sample.setMetric("cycles", 1000.0)
            profile1.addSample(sample)
        
        # Profile 2: Function B is optimized, A is still slow
        for i in range(100):
            sample = SampleData(function_name="func_a")
            sample.setMetric("cycles", 10000.0)
            profile2.addSample(sample)
        
        for i in range(20):
            sample = SampleData(function_name="func_b")
            sample.setMetric("cycles", 500.0)  # Optimized
            profile2.addSample(sample)
        
        # Analyze both
        analyzer1 = HotspotAnalyzer(profile1)
        analyzer1.analyze()
        
        analyzer2 = HotspotAnalyzer(profile2)
        analyzer2.analyze()
        
        # Compare results
        hotspots1 = analyzer1.getHotspots()
        hotspots2 = analyzer2.getHotspots()
        
        # Function A should be similar in both
        assert abs(hotspots1["func_a"]["total_cycles"] - hotspots2["func_a"]["total_cycles"]) < 1000
        
        # Function B should be lower in profile 2
        assert hotspots2["func_b"]["total_cycles"] < hotspots1["func_b"]["total_cycles"]


class TestProfileAnalysisEdgeCases:
    """Integration tests for edge cases in profile analysis."""
    
    def test_empty_profile_analysis(self):
        """Test analyzing an empty profile."""
        profile = PerfData()
        analyzer = HotspotAnalyzer(profile)
        analyzer.analyze()
        
        assert analyzer.getTotalSamples() == 0
        assert len(analyzer.getHotspots()) == 0
        assert analyzer.getTopHotspots("total_cycles") == []
    
    def test_single_sample_analysis(self):
        """Test analyzing a profile with a single sample."""
        profile = PerfData()
        sample = SampleData(function_name="single_func")
        sample.setMetric("cycles", 1000.0)
        profile.addSample(sample)
        
        analyzer = HotspotAnalyzer(profile)
        analyzer.analyze()
        
        assert analyzer.getTotalSamples() == 1
        assert len(analyzer.getHotspots()) == 1
        assert analyzer.getHotspotPercentage("single_func") == 100.0
    
    def test_samples_without_function_names(self):
        """Test analyzing samples without function names."""
        profile = PerfData()
        
        # Some samples with function names
        for i in range(10):
            sample = SampleData(function_name="func_a")
            sample.setMetric("cycles", 1000.0)
            profile.addSample(sample)
        
        # Some samples without function names
        for i in range(5):
            sample = SampleData()  # No function name
            sample.setMetric("cycles", 1000.0)
            profile.addSample(sample)
        
        analyzer = HotspotAnalyzer(profile)
        analyzer.analyze()
        
        # Only samples with function names should be in hotspots
        hotspots = analyzer.getHotspots()
        assert len(hotspots) == 1
        assert "func_a" in hotspots
    
    def test_samples_without_metrics(self):
        """Test analyzing samples without metrics."""
        profile = PerfData()
        
        for i in range(10):
            sample = SampleData(function_name="func_a")
            # No metrics set
            profile.addSample(sample)
        
        analyzer = HotspotAnalyzer(profile)
        analyzer.analyze()
        
        hotspots = analyzer.getHotspots()
        assert len(hotspots) == 1
        assert hotspots["func_a"]["sample_count"] == 10
        assert hotspots["func_a"]["total_cycles"] == 0.0
        assert hotspots["func_a"]["total_instructions"] == 0.0
