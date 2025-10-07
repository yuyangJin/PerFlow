"""
Unit tests for PerfData and ProfileInfo classes

This module contains comprehensive tests for the PerfData and ProfileInfo classes,
which handle profiling data collection and metadata.
"""

import pytest
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData, ProfileInfo
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData


class TestProfileInfoCreation:
    """Test ProfileInfo object creation and initialization."""
    
    def test_profile_info_creation_default(self):
        """Test creating a ProfileInfo with default values."""
        info = ProfileInfo()
        
        assert info.getPid() is None
        assert info.getTid() is None
        assert info.getNumProcesses() is None
        assert info.getNumThreads() is None
        assert info.getProfileFormat() is None
        assert info.getSampleRate() is None
        assert info.getProfileStartTime() is None
        assert info.getProfileEndTime() is None
        assert info.getApplicationName() is None
        assert info.getMetrics() == []
    
    def test_profile_info_creation_with_parameters(self):
        """Test creating a ProfileInfo with all parameters."""
        metrics = ["cycles", "instructions", "cache_misses"]
        
        info = ProfileInfo(
            pid=100,
            tid=200,
            num_processes=4,
            num_threads=8,
            profile_format="VTune",
            sample_rate=1000.0,
            profile_start_time=0.0,
            profile_end_time=10.0,
            application_name="test_app",
            metrics=metrics
        )
        
        assert info.getPid() == 100
        assert info.getTid() == 200
        assert info.getNumProcesses() == 4
        assert info.getNumThreads() == 8
        assert info.getProfileFormat() == "VTune"
        assert info.getSampleRate() == 1000.0
        assert info.getProfileStartTime() == 0.0
        assert info.getProfileEndTime() == 10.0
        assert info.getApplicationName() == "test_app"
        assert info.getMetrics() == metrics


class TestProfileInfoGettersSetters:
    """Test ProfileInfo getters and setters."""
    
    def test_pid_getter_setter(self):
        """Test PID getter and setter."""
        info = ProfileInfo()
        info.setPid(123)
        assert info.getPid() == 123
    
    def test_tid_getter_setter(self):
        """Test TID getter and setter."""
        info = ProfileInfo()
        info.setTid(456)
        assert info.getTid() == 456
    
    def test_num_processes_getter_setter(self):
        """Test number of processes getter and setter."""
        info = ProfileInfo()
        info.setNumProcesses(16)
        assert info.getNumProcesses() == 16
    
    def test_num_threads_getter_setter(self):
        """Test number of threads getter and setter."""
        info = ProfileInfo()
        info.setNumThreads(32)
        assert info.getNumThreads() == 32
    
    def test_profile_format_getter_setter(self):
        """Test profile format getter and setter."""
        info = ProfileInfo()
        info.setProfileFormat("Nsight")
        assert info.getProfileFormat() == "Nsight"
    
    def test_sample_rate_getter_setter(self):
        """Test sample rate getter and setter."""
        info = ProfileInfo()
        info.setSampleRate(5000.0)
        assert info.getSampleRate() == 5000.0
    
    def test_start_time_getter_setter(self):
        """Test start time getter and setter."""
        info = ProfileInfo()
        info.setProfileStartTime(1.5)
        assert info.getProfileStartTime() == 1.5
    
    def test_end_time_getter_setter(self):
        """Test end time getter and setter."""
        info = ProfileInfo()
        info.setProfileEndTime(20.5)
        assert info.getProfileEndTime() == 20.5
    
    def test_application_name_getter_setter(self):
        """Test application name getter and setter."""
        info = ProfileInfo()
        info.setApplicationName("my_parallel_app")
        assert info.getApplicationName() == "my_parallel_app"


class TestProfileInfoDuration:
    """Test ProfileInfo duration calculation."""
    
    def test_profile_duration_calculation(self):
        """Test calculating profile duration."""
        info = ProfileInfo()
        info.setProfileStartTime(5.0)
        info.setProfileEndTime(15.0)
        
        assert info.getProfileDuration() == 10.0
    
    def test_profile_duration_with_initialization(self):
        """Test duration calculation with constructor initialization."""
        info = ProfileInfo(profile_start_time=0.0, profile_end_time=100.0)
        assert info.getProfileDuration() == 100.0
    
    def test_profile_duration_none_when_times_not_set(self):
        """Test that duration is None when times are not set."""
        info = ProfileInfo()
        assert info.getProfileDuration() is None
        
        info.setProfileStartTime(5.0)
        assert info.getProfileDuration() is None
        
        info.setProfileEndTime(10.0)
        assert info.getProfileDuration() == 5.0


class TestProfileInfoMetrics:
    """Test ProfileInfo metrics management."""
    
    def test_metrics_getter_setter(self):
        """Test metrics getter and setter."""
        info = ProfileInfo()
        metrics = ["cycles", "instructions", "branches"]
        info.setMetrics(metrics)
        assert info.getMetrics() == metrics
    
    def test_add_metric(self):
        """Test adding individual metrics."""
        info = ProfileInfo()
        info.addMetric("cycles")
        info.addMetric("instructions")
        
        metrics = info.getMetrics()
        assert len(metrics) == 2
        assert "cycles" in metrics
        assert "instructions" in metrics
    
    def test_add_duplicate_metric(self):
        """Test that duplicate metrics are not added."""
        info = ProfileInfo()
        info.addMetric("cycles")
        info.addMetric("cycles")
        
        assert len(info.getMetrics()) == 1
    
    def test_metrics_with_initialization(self):
        """Test metrics initialized in constructor."""
        metrics = ["metric1", "metric2", "metric3"]
        info = ProfileInfo(metrics=metrics)
        assert info.getMetrics() == metrics


class TestPerfDataCreation:
    """Test PerfData object creation and initialization."""
    
    def test_perf_data_creation(self):
        """Test creating a PerfData object."""
        perf_data = PerfData()
        
        assert perf_data.getSampleCount() == 0
        assert perf_data.getSamples() == []
        assert isinstance(perf_data.getProfileInfo(), ProfileInfo)
        assert perf_data.getAggregatedData() == {}


class TestPerfDataSampleManagement:
    """Test PerfData sample management."""
    
    def test_add_sample(self):
        """Test adding samples to PerfData."""
        perf_data = PerfData()
        
        sample1 = SampleData(timestamp=1.0, function_name="func1")
        sample2 = SampleData(timestamp=2.0, function_name="func2")
        
        perf_data.addSample(sample1)
        perf_data.addSample(sample2)
        
        assert perf_data.getSampleCount() == 2
    
    def test_get_samples(self):
        """Test getting all samples."""
        perf_data = PerfData()
        
        sample1 = SampleData(timestamp=1.0)
        sample2 = SampleData(timestamp=2.0)
        
        perf_data.addSample(sample1)
        perf_data.addSample(sample2)
        
        samples = perf_data.getSamples()
        assert len(samples) == 2
        assert samples[0] == sample1
        assert samples[1] == sample2
    
    def test_get_sample_by_index(self):
        """Test getting a specific sample by index."""
        perf_data = PerfData()
        
        sample1 = SampleData(timestamp=1.0, function_name="func1")
        sample2 = SampleData(timestamp=2.0, function_name="func2")
        
        perf_data.addSample(sample1)
        perf_data.addSample(sample2)
        
        assert perf_data.getSample(0) == sample1
        assert perf_data.getSample(1) == sample2
    
    def test_get_sample_out_of_bounds(self):
        """Test getting a sample with invalid index."""
        perf_data = PerfData()
        perf_data.addSample(SampleData())
        
        assert perf_data.getSample(-1) is None
        assert perf_data.getSample(10) is None
    
    def test_sample_count(self):
        """Test sample count."""
        perf_data = PerfData()
        assert perf_data.getSampleCount() == 0
        
        perf_data.addSample(SampleData())
        assert perf_data.getSampleCount() == 1
        
        perf_data.addSample(SampleData())
        perf_data.addSample(SampleData())
        assert perf_data.getSampleCount() == 3


class TestPerfDataProfileInfo:
    """Test PerfData profile info management."""
    
    def test_get_profile_info(self):
        """Test getting profile info."""
        perf_data = PerfData()
        info = perf_data.getProfileInfo()
        assert isinstance(info, ProfileInfo)
    
    def test_set_profile_info(self):
        """Test setting profile info."""
        perf_data = PerfData()
        
        info = ProfileInfo(
            pid=100,
            tid=200,
            application_name="test_app"
        )
        
        perf_data.setProfileInfo(info)
        
        retrieved_info = perf_data.getProfileInfo()
        assert retrieved_info.getPid() == 100
        assert retrieved_info.getTid() == 200
        assert retrieved_info.getApplicationName() == "test_app"


class TestPerfDataClear:
    """Test PerfData clear functionality."""
    
    def test_clear_samples(self):
        """Test clearing all samples."""
        perf_data = PerfData()
        
        perf_data.addSample(SampleData(timestamp=1.0))
        perf_data.addSample(SampleData(timestamp=2.0))
        
        assert perf_data.getSampleCount() == 2
        
        perf_data.clear()
        
        assert perf_data.getSampleCount() == 0
        assert perf_data.getSamples() == []
    
    def test_clear_aggregated_data(self):
        """Test that clear also clears aggregated data."""
        perf_data = PerfData()
        
        sample = SampleData(function_name="func1")
        sample.setMetric("cycles", 1000.0)
        perf_data.addSample(sample)
        
        perf_data.aggregateByFunction()
        assert len(perf_data.getAggregatedData()) > 0
        
        perf_data.clear()
        assert perf_data.getAggregatedData() == {}


class TestPerfDataAggregation:
    """Test PerfData aggregation functionality."""
    
    def test_aggregate_by_function(self):
        """Test aggregating samples by function name."""
        perf_data = PerfData()
        
        # Add samples for func1
        sample1 = SampleData(function_name="func1")
        sample1.setMetric("cycles", 1000.0)
        sample1.setMetric("instructions", 500.0)
        
        sample2 = SampleData(function_name="func1")
        sample2.setMetric("cycles", 2000.0)
        sample2.setMetric("instructions", 1000.0)
        
        # Add samples for func2
        sample3 = SampleData(function_name="func2")
        sample3.setMetric("cycles", 500.0)
        sample3.setMetric("instructions", 250.0)
        
        perf_data.addSample(sample1)
        perf_data.addSample(sample2)
        perf_data.addSample(sample3)
        
        aggregated = perf_data.aggregateByFunction()
        
        assert "func1" in aggregated
        assert "func2" in aggregated
        assert aggregated["func1"]["cycles"] == 3000.0
        assert aggregated["func1"]["instructions"] == 1500.0
        assert aggregated["func2"]["cycles"] == 500.0
        assert aggregated["func2"]["instructions"] == 250.0
    
    def test_aggregate_by_function_empty(self):
        """Test aggregating with no samples."""
        perf_data = PerfData()
        aggregated = perf_data.aggregateByFunction()
        assert aggregated == {}
    
    def test_get_aggregated_data(self):
        """Test getting aggregated data."""
        perf_data = PerfData()
        
        sample = SampleData(function_name="func1")
        sample.setMetric("cycles", 1000.0)
        perf_data.addSample(sample)
        
        perf_data.aggregateByFunction()
        
        aggregated = perf_data.getAggregatedData()
        assert "func1" in aggregated
        assert aggregated["func1"]["cycles"] == 1000.0


class TestPerfDataTopFunctions:
    """Test PerfData top functions functionality."""
    
    def test_get_top_functions(self):
        """Test getting top functions by metric."""
        perf_data = PerfData()
        
        # Create samples with different cycle counts
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
            perf_data.addSample(sample)
        
        top_3 = perf_data.getTopFunctions("cycles", top_n=3)
        
        assert len(top_3) == 3
        assert top_3[0] == ("func3", 8000.0)
        assert top_3[1] == ("func5", 6000.0)
        assert top_3[2] == ("func1", 5000.0)
    
    def test_get_top_functions_default_count(self):
        """Test getting top functions with default count."""
        perf_data = PerfData()
        
        for i in range(15):
            sample = SampleData(function_name=f"func{i}")
            sample.setMetric("cycles", float(i * 100))
            perf_data.addSample(sample)
        
        top_functions = perf_data.getTopFunctions("cycles")
        
        # Default is top 10
        assert len(top_functions) == 10
    
    def test_get_top_functions_auto_aggregates(self):
        """Test that getTopFunctions automatically aggregates if needed."""
        perf_data = PerfData()
        
        sample1 = SampleData(function_name="func1")
        sample1.setMetric("cycles", 1000.0)
        perf_data.addSample(sample1)
        
        # Should work even without explicit aggregation
        top = perf_data.getTopFunctions("cycles", top_n=1)
        assert len(top) == 1
        assert top[0] == ("func1", 1000.0)
    
    def test_get_top_functions_nonexistent_metric(self):
        """Test getting top functions for a metric that doesn't exist."""
        perf_data = PerfData()
        
        sample = SampleData(function_name="func1")
        sample.setMetric("cycles", 1000.0)
        perf_data.addSample(sample)
        
        top = perf_data.getTopFunctions("nonexistent_metric")
        assert top == []


class TestPerfDataIntegration:
    """Integration tests for PerfData."""
    
    def test_complete_workflow(self):
        """Test a complete profiling workflow."""
        # Create PerfData
        perf_data = PerfData()
        
        # Set profile info
        info = ProfileInfo(
            pid=1000,
            tid=2000,
            num_processes=4,
            profile_format="VTune",
            sample_rate=1000.0,
            application_name="benchmark_app"
        )
        info.addMetric("cycles")
        info.addMetric("instructions")
        
        perf_data.setProfileInfo(info)
        
        # Add samples
        for i in range(100):
            sample = SampleData(
                timestamp=float(i) * 0.001,
                pid=1000,
                tid=2000,
                function_name=f"func{i % 5}"  # 5 different functions
            )
            sample.setMetric("cycles", float((i % 5 + 1) * 1000))
            sample.setMetric("instructions", float((i % 5 + 1) * 500))
            perf_data.addSample(sample)
        
        # Verify sample count
        assert perf_data.getSampleCount() == 100
        
        # Aggregate data
        aggregated = perf_data.aggregateByFunction()
        assert len(aggregated) == 5
        
        # Get top functions
        top_funcs = perf_data.getTopFunctions("cycles", top_n=3)
        assert len(top_funcs) == 3
        
        # Verify profile info
        retrieved_info = perf_data.getProfileInfo()
        assert retrieved_info.getApplicationName() == "benchmark_app"
        assert retrieved_info.getSampleRate() == 1000.0
