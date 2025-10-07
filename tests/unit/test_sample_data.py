"""
Unit tests for SampleData class

This module contains comprehensive tests for the SampleData class,
which represents a single profiling sample from program execution.
"""

import pytest
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData


class TestSampleDataCreation:
    """Test SampleData object creation and initialization."""
    
    def test_sample_data_creation_default(self):
        """Test creating a SampleData with default values."""
        sample = SampleData()
        
        assert sample.getTimestamp() is None
        assert sample.getPid() is None
        assert sample.getTid() is None
        assert sample.getFunctionName() is None
        assert sample.getModuleName() is None
        assert sample.getSourceFile() is None
        assert sample.getLineNumber() is None
        assert sample.getInstructionPointer() is None
        assert sample.getMetrics() == {}
        assert sample.getCallStack() == []
    
    def test_sample_data_creation_with_parameters(self):
        """Test creating a SampleData with all parameters."""
        metrics = {"cycles": 1000.0, "instructions": 500.0}
        call_stack = ["main", "foo", "bar"]
        
        sample = SampleData(
            timestamp=1.5,
            pid=100,
            tid=200,
            function_name="compute",
            module_name="libmath.so",
            source_file="compute.c",
            line_number=42,
            instruction_pointer=0x1234,
            metrics=metrics,
            call_stack=call_stack
        )
        
        assert sample.getTimestamp() == 1.5
        assert sample.getPid() == 100
        assert sample.getTid() == 200
        assert sample.getFunctionName() == "compute"
        assert sample.getModuleName() == "libmath.so"
        assert sample.getSourceFile() == "compute.c"
        assert sample.getLineNumber() == 42
        assert sample.getInstructionPointer() == 0x1234
        assert sample.getMetrics() == metrics
        assert sample.getCallStack() == call_stack


class TestSampleDataGettersSetters:
    """Test SampleData getters and setters."""
    
    def test_timestamp_getter_setter(self):
        """Test timestamp getter and setter."""
        sample = SampleData()
        sample.setTimestamp(2.5)
        assert sample.getTimestamp() == 2.5
    
    def test_pid_getter_setter(self):
        """Test PID getter and setter."""
        sample = SampleData()
        sample.setPid(123)
        assert sample.getPid() == 123
    
    def test_tid_getter_setter(self):
        """Test TID getter and setter."""
        sample = SampleData()
        sample.setTid(456)
        assert sample.getTid() == 456
    
    def test_function_name_getter_setter(self):
        """Test function name getter and setter."""
        sample = SampleData()
        sample.setFunctionName("process_data")
        assert sample.getFunctionName() == "process_data"
    
    def test_module_name_getter_setter(self):
        """Test module name getter and setter."""
        sample = SampleData()
        sample.setModuleName("libcore.so")
        assert sample.getModuleName() == "libcore.so"
    
    def test_source_file_getter_setter(self):
        """Test source file getter and setter."""
        sample = SampleData()
        sample.setSourceFile("/path/to/source.c")
        assert sample.getSourceFile() == "/path/to/source.c"
    
    def test_line_number_getter_setter(self):
        """Test line number getter and setter."""
        sample = SampleData()
        sample.setLineNumber(100)
        assert sample.getLineNumber() == 100
    
    def test_instruction_pointer_getter_setter(self):
        """Test instruction pointer getter and setter."""
        sample = SampleData()
        sample.setInstructionPointer(0xABCD)
        assert sample.getInstructionPointer() == 0xABCD


class TestSampleDataMetrics:
    """Test SampleData metrics management."""
    
    def test_metrics_getter_setter(self):
        """Test metrics getter and setter."""
        sample = SampleData()
        metrics = {"cycles": 2000.0, "cache_misses": 50.0}
        sample.setMetrics(metrics)
        assert sample.getMetrics() == metrics
    
    def test_get_specific_metric(self):
        """Test getting a specific metric value."""
        sample = SampleData()
        sample.setMetric("cycles", 1500.0)
        assert sample.getMetric("cycles") == 1500.0
    
    def test_get_nonexistent_metric(self):
        """Test getting a metric that doesn't exist."""
        sample = SampleData()
        assert sample.getMetric("nonexistent") is None
    
    def test_set_specific_metric(self):
        """Test setting a specific metric value."""
        sample = SampleData()
        sample.setMetric("instructions", 3000.0)
        sample.setMetric("branches", 100.0)
        
        assert sample.getMetric("instructions") == 3000.0
        assert sample.getMetric("branches") == 100.0
        assert len(sample.getMetrics()) == 2
    
    def test_update_existing_metric(self):
        """Test updating an existing metric value."""
        sample = SampleData()
        sample.setMetric("cycles", 1000.0)
        sample.setMetric("cycles", 2000.0)
        assert sample.getMetric("cycles") == 2000.0


class TestSampleDataCallStack:
    """Test SampleData call stack management."""
    
    def test_call_stack_getter_setter(self):
        """Test call stack getter and setter."""
        sample = SampleData()
        call_stack = ["main", "process", "compute"]
        sample.setCallStack(call_stack)
        assert sample.getCallStack() == call_stack
    
    def test_add_to_call_stack(self):
        """Test adding frames to the call stack."""
        sample = SampleData()
        sample.addToCallStack("main")
        sample.addToCallStack("foo")
        sample.addToCallStack("bar")
        
        stack = sample.getCallStack()
        assert len(stack) == 3
        assert stack[0] == "main"
        assert stack[1] == "foo"
        assert stack[2] == "bar"
    
    def test_call_stack_depth(self):
        """Test getting call stack depth."""
        sample = SampleData()
        assert sample.getCallStackDepth() == 0
        
        sample.addToCallStack("main")
        assert sample.getCallStackDepth() == 1
        
        sample.addToCallStack("foo")
        sample.addToCallStack("bar")
        assert sample.getCallStackDepth() == 3
    
    def test_call_stack_with_initialization(self):
        """Test call stack initialized in constructor."""
        call_stack = ["frame1", "frame2", "frame3", "frame4"]
        sample = SampleData(call_stack=call_stack)
        assert sample.getCallStackDepth() == 4
        assert sample.getCallStack() == call_stack


class TestSampleDataIntegration:
    """Integration tests for SampleData."""
    
    def test_complete_sample_workflow(self):
        """Test creating and modifying a complete sample."""
        # Create sample
        sample = SampleData(
            timestamp=10.5,
            pid=1000,
            tid=2000
        )
        
        # Set location information
        sample.setFunctionName("matrix_multiply")
        sample.setModuleName("libblas.so")
        sample.setSourceFile("matrix.c")
        sample.setLineNumber(150)
        
        # Add metrics
        sample.setMetric("cycles", 50000.0)
        sample.setMetric("instructions", 25000.0)
        sample.setMetric("cache_misses", 500.0)
        
        # Build call stack
        sample.addToCallStack("main")
        sample.addToCallStack("compute_batch")
        sample.addToCallStack("matrix_multiply")
        
        # Verify all data
        assert sample.getTimestamp() == 10.5
        assert sample.getPid() == 1000
        assert sample.getTid() == 2000
        assert sample.getFunctionName() == "matrix_multiply"
        assert sample.getModuleName() == "libblas.so"
        assert sample.getSourceFile() == "matrix.c"
        assert sample.getLineNumber() == 150
        assert sample.getMetric("cycles") == 50000.0
        assert sample.getMetric("instructions") == 25000.0
        assert sample.getMetric("cache_misses") == 500.0
        assert sample.getCallStackDepth() == 3
    
    def test_multiple_samples_independence(self):
        """Test that multiple samples are independent."""
        sample1 = SampleData(timestamp=1.0, function_name="func1")
        sample2 = SampleData(timestamp=2.0, function_name="func2")
        
        sample1.setMetric("cycles", 1000.0)
        sample2.setMetric("cycles", 2000.0)
        
        sample1.addToCallStack("main")
        sample2.addToCallStack("main")
        sample2.addToCallStack("helper")
        
        # Verify independence
        assert sample1.getTimestamp() == 1.0
        assert sample2.getTimestamp() == 2.0
        assert sample1.getMetric("cycles") == 1000.0
        assert sample2.getMetric("cycles") == 2000.0
        assert sample1.getCallStackDepth() == 1
        assert sample2.getCallStackDepth() == 2
