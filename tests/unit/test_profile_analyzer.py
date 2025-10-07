"""
Unit tests for ProfileAnalyzer and HotspotAnalyzer classes

This module contains comprehensive tests for profile analysis functionality.
"""

import pytest
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData, ProfileInfo
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData
from perflow.task.profile_analysis.profile_analyzer import ProfileAnalyzer
from perflow.task.profile_analysis.hotspot_analyzer import HotspotAnalyzer


class TestProfileAnalyzerCreation:
    """Test ProfileAnalyzer object creation and initialization."""
    
    def test_profile_analyzer_creation(self):
        """Test creating a ProfileAnalyzer."""
        analyzer = ProfileAnalyzer()
        
        assert analyzer.getProfile() is None
        assert analyzer.getCallbacks() == {}
    
    def test_profile_analyzer_creation_with_profile(self):
        """Test creating a ProfileAnalyzer with a profile."""
        profile = PerfData()
        analyzer = ProfileAnalyzer(profile)
        
        assert analyzer.getProfile() == profile


class TestProfileAnalyzerProfileManagement:
    """Test ProfileAnalyzer profile management."""
    
    def test_set_profile(self):
        """Test setting profile."""
        analyzer = ProfileAnalyzer()
        profile = PerfData()
        
        analyzer.setProfile(profile)
        assert analyzer.getProfile() == profile
    
    def test_get_profile(self):
        """Test getting profile."""
        profile = PerfData()
        analyzer = ProfileAnalyzer(profile)
        
        assert analyzer.getProfile() == profile


class TestProfileAnalyzerCallbacks:
    """Test ProfileAnalyzer callback management."""
    
    def test_register_callback(self):
        """Test registering a callback."""
        analyzer = ProfileAnalyzer()
        
        def test_callback(sample):
            pass
        
        analyzer.registerCallback("test", test_callback)
        callbacks = analyzer.getCallbacks()
        
        assert "test" in callbacks
        assert callbacks["test"] == test_callback
    
    def test_unregister_callback(self):
        """Test unregistering a callback."""
        analyzer = ProfileAnalyzer()
        
        def test_callback(sample):
            pass
        
        analyzer.registerCallback("test", test_callback)
        assert "test" in analyzer.getCallbacks()
        
        analyzer.unregisterCallback("test")
        assert "test" not in analyzer.getCallbacks()
    
    def test_clear_callbacks(self):
        """Test clearing all callbacks."""
        analyzer = ProfileAnalyzer()
        
        analyzer.registerCallback("cb1", lambda s: None)
        analyzer.registerCallback("cb2", lambda s: None)
        
        assert len(analyzer.getCallbacks()) == 2
        
        analyzer.clearCallbacks()
        assert len(analyzer.getCallbacks()) == 0
    
    def test_get_callbacks(self):
        """Test getting all callbacks."""
        analyzer = ProfileAnalyzer()
        
        cb1 = lambda s: None
        cb2 = lambda s: None
        
        analyzer.registerCallback("cb1", cb1)
        analyzer.registerCallback("cb2", cb2)
        
        callbacks = analyzer.getCallbacks()
        assert len(callbacks) == 2
        assert callbacks["cb1"] == cb1
        assert callbacks["cb2"] == cb2


class TestProfileAnalyzerAnalysis:
    """Test ProfileAnalyzer analysis functionality."""
    
    def test_analyze_empty_profile(self):
        """Test analyzing an empty profile."""
        analyzer = ProfileAnalyzer()
        profile = PerfData()
        analyzer.setProfile(profile)
        
        # Should complete without error
        analyzer.analyze()
    
    def test_analyze_with_samples(self):
        """Test analyzing a profile with samples."""
        analyzer = ProfileAnalyzer()
        profile = PerfData()
        
        # Add samples
        for i in range(5):
            sample = SampleData(timestamp=float(i))
            profile.addSample(sample)
        
        analyzer.setProfile(profile)
        
        # Track callback invocations
        invocations = []
        
        def track_callback(sample):
            invocations.append(sample)
        
        analyzer.registerCallback("tracker", track_callback)
        analyzer.analyze()
        
        # Verify callback was called for each sample
        assert len(invocations) == 5
    
    def test_analyze_with_multiple_callbacks(self):
        """Test analyzing with multiple callbacks."""
        analyzer = ProfileAnalyzer()
        profile = PerfData()
        
        sample = SampleData(timestamp=1.0)
        profile.addSample(sample)
        
        analyzer.setProfile(profile)
        
        # Track callback invocations
        invocations = {"cb1": 0, "cb2": 0}
        
        def callback1(s):
            invocations["cb1"] += 1
        
        def callback2(s):
            invocations["cb2"] += 1
        
        analyzer.registerCallback("cb1", callback1)
        analyzer.registerCallback("cb2", callback2)
        analyzer.analyze()
        
        # Both callbacks should be invoked
        assert invocations["cb1"] == 1
        assert invocations["cb2"] == 1
    
    def test_analyze_without_profile(self):
        """Test analyzing without setting a profile."""
        analyzer = ProfileAnalyzer()
        
        # Should complete without error
        analyzer.analyze()


class TestHotspotAnalyzerCreation:
    """Test HotspotAnalyzer object creation and initialization."""
    
    def test_hotspot_analyzer_creation(self):
        """Test creating a HotspotAnalyzer."""
        analyzer = HotspotAnalyzer()
        
        assert analyzer.getProfile() is None
        assert analyzer.getHotspots() == {}
    
    def test_hotspot_analyzer_creation_with_profile(self):
        """Test creating a HotspotAnalyzer with a profile."""
        profile = PerfData()
        analyzer = HotspotAnalyzer(profile)
        
        assert analyzer.getProfile() == profile
    
    def test_hotspot_analyzer_creation_with_threshold(self):
        """Test creating a HotspotAnalyzer with a threshold."""
        analyzer = HotspotAnalyzer(threshold=1000.0)
        
        assert analyzer.m_threshold == 1000.0


class TestHotspotAnalyzerDetection:
    """Test HotspotAnalyzer hotspot detection."""
    
    def test_detect_hotspots(self):
        """Test detecting hotspots in profile data."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        # Add samples for different functions
        for i in range(10):
            sample = SampleData(function_name="func1")
            sample.setMetric("cycles", 1000.0)
            sample.setMetric("instructions", 500.0)
            profile.addSample(sample)
        
        for i in range(5):
            sample = SampleData(function_name="func2")
            sample.setMetric("cycles", 500.0)
            sample.setMetric("instructions", 250.0)
            profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        hotspots = analyzer.getHotspots()
        
        assert "func1" in hotspots
        assert "func2" in hotspots
        assert hotspots["func1"]["sample_count"] == 10
        assert hotspots["func2"]["sample_count"] == 5
        assert hotspots["func1"]["total_cycles"] == 10000.0
        assert hotspots["func2"]["total_cycles"] == 2500.0
    
    def test_detect_hotspots_custom_metrics(self):
        """Test detecting hotspots with custom metrics."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        sample = SampleData(function_name="func1")
        sample.setMetric("cycles", 1000.0)
        sample.setMetric("cache_misses", 50.0)
        sample.setMetric("branch_misses", 10.0)
        profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        hotspots = analyzer.getHotspots()
        
        assert "func1" in hotspots
        assert hotspots["func1"]["total_cache_misses"] == 50.0
        assert hotspots["func1"]["total_branch_misses"] == 10.0


class TestHotspotAnalyzerTopHotspots:
    """Test HotspotAnalyzer top hotspots functionality."""
    
    def test_get_top_hotspots(self):
        """Test getting top hotspots."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        # Add samples with different cycle counts
        funcs_cycles = [
            ("func1", 5000.0),
            ("func2", 3000.0),
            ("func3", 8000.0),
            ("func4", 1000.0),
            ("func5", 6000.0)
        ]
        
        for func_name, cycles in funcs_cycles:
            sample = SampleData(function_name=func_name)
            sample.setMetric("cycles", cycles)
            profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        top_3 = analyzer.getTopHotspots("total_cycles", top_n=3)
        
        assert len(top_3) == 3
        assert top_3[0][0] == "func3"
        assert top_3[0][1]["total_cycles"] == 8000.0
        assert top_3[1][0] == "func5"
        assert top_3[2][0] == "func1"
    
    def test_get_top_hotspots_default_metric(self):
        """Test getting top hotspots with default metric."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        sample = SampleData(function_name="func1")
        sample.setMetric("cycles", 1000.0)
        profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        top = analyzer.getTopHotspots()
        
        assert len(top) == 1
        assert top[0][0] == "func1"


class TestHotspotAnalyzerThreshold:
    """Test HotspotAnalyzer threshold functionality."""
    
    def test_get_hotspots_by_threshold(self):
        """Test filtering hotspots by threshold."""
        analyzer = HotspotAnalyzer(threshold=5000.0)
        profile = PerfData()
        
        # Add samples with different cycle counts
        funcs_cycles = [
            ("func1", 3000.0),
            ("func2", 6000.0),
            ("func3", 8000.0),
            ("func4", 4000.0)
        ]
        
        for func_name, cycles in funcs_cycles:
            sample = SampleData(function_name=func_name)
            sample.setMetric("cycles", cycles)
            profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        hotspots = analyzer.getHotspotsByThreshold("total_cycles")
        
        # Only func2 and func3 exceed threshold
        assert len(hotspots) == 2
        assert hotspots[0][0] == "func3"
        assert hotspots[1][0] == "func2"
    
    def test_get_hotspots_by_custom_threshold(self):
        """Test filtering hotspots with custom threshold."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        sample1 = SampleData(function_name="func1")
        sample1.setMetric("cycles", 3000.0)
        profile.addSample(sample1)
        
        sample2 = SampleData(function_name="func2")
        sample2.setMetric("cycles", 6000.0)
        profile.addSample(sample2)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        hotspots = analyzer.getHotspotsByThreshold("total_cycles", threshold=5000.0)
        
        assert len(hotspots) == 1
        assert hotspots[0][0] == "func2"


class TestHotspotAnalyzerStatistics:
    """Test HotspotAnalyzer statistics functionality."""
    
    def test_get_total_samples(self):
        """Test getting total sample count."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        for i in range(10):
            sample = SampleData(function_name="func1")
            profile.addSample(sample)
        
        for i in range(5):
            sample = SampleData(function_name="func2")
            profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        assert analyzer.getTotalSamples() == 15
    
    def test_get_hotspot_percentage(self):
        """Test calculating hotspot percentage."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        for i in range(10):
            sample = SampleData(function_name="func1")
            profile.addSample(sample)
        
        for i in range(5):
            sample = SampleData(function_name="func2")
            profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        assert analyzer.getHotspotPercentage("func1") == pytest.approx(66.666, rel=0.01)
        assert analyzer.getHotspotPercentage("func2") == pytest.approx(33.333, rel=0.01)
    
    def test_get_hotspot_percentage_nonexistent_function(self):
        """Test getting percentage for nonexistent function."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        sample = SampleData(function_name="func1")
        profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        assert analyzer.getHotspotPercentage("nonexistent") == 0.0


class TestHotspotAnalyzerClear:
    """Test HotspotAnalyzer clear functionality."""
    
    def test_clear(self):
        """Test clearing hotspot data."""
        analyzer = HotspotAnalyzer()
        profile = PerfData()
        
        sample = SampleData(function_name="func1")
        profile.addSample(sample)
        
        analyzer.setProfile(profile)
        analyzer.analyze()
        
        assert len(analyzer.getHotspots()) > 0
        
        analyzer.clear()
        
        assert len(analyzer.getHotspots()) == 0


class TestHotspotAnalyzerIntegration:
    """Integration tests for HotspotAnalyzer."""
    
    def test_complete_hotspot_analysis(self):
        """Test a complete hotspot analysis workflow."""
        # Create profile data
        profile = PerfData()
        info = ProfileInfo(application_name="test_app")
        profile.setProfileInfo(info)
        
        # Add samples for multiple functions
        functions = {
            "main": (20, 1000.0, 500.0),
            "compute": (50, 5000.0, 2500.0),
            "io_read": (10, 2000.0, 1000.0),
            "io_write": (5, 1000.0, 500.0),
            "helper": (15, 1500.0, 750.0)
        }
        
        for func_name, (count, cycles_per_sample, insts_per_sample) in functions.items():
            for i in range(count):
                sample = SampleData(
                    timestamp=float(i),
                    function_name=func_name
                )
                sample.setMetric("cycles", cycles_per_sample)
                sample.setMetric("instructions", insts_per_sample)
                profile.addSample(sample)
        
        # Create analyzer without threshold for top hotspots test
        analyzer = HotspotAnalyzer(profile, threshold=0.0)
        analyzer.analyze()
        
        # Verify total samples
        assert analyzer.getTotalSamples() == 100
        
        # Get top hotspots
        top_3 = analyzer.getTopHotspots("total_cycles", top_n=3)
        
        assert len(top_3) == 3
        assert top_3[0][0] == "compute"
        assert top_3[0][1]["total_cycles"] == 250000.0
        
        # Get hotspots by threshold
        high_hotspots = analyzer.getHotspotsByThreshold("total_cycles", threshold=50000.0)
        assert len(high_hotspots) == 1
        assert high_hotspots[0][0] == "compute"
        
        # Check percentages
        assert analyzer.getHotspotPercentage("compute") == 50.0
        assert analyzer.getHotspotPercentage("main") == 20.0
