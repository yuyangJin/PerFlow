"""
Unit tests for OTF2Reader class
"""
import pytest
from perflow.task.reader.otf2_reader import OTF2Reader
from perflow.perf_data_struct.dynamic.trace.trace import Trace
from perflow.flow.flow import FlowNode


class TestOTF2Reader:
    """Test OTF2Reader class"""
    
    def test_otf2reader_creation(self):
        """Test creating OTF2Reader"""
        reader = OTF2Reader()
        assert reader.getFilePath() is None
        assert reader.getTrace() is None
    
    def test_otf2reader_creation_with_path(self):
        """Test creating OTF2Reader with file path"""
        reader = OTF2Reader("/path/to/trace.otf2")
        assert reader.getFilePath() == "/path/to/trace.otf2"
    
    def test_otf2reader_inherits_from_flownode(self):
        """Test that OTF2Reader inherits from FlowNode"""
        reader = OTF2Reader()
        assert isinstance(reader, FlowNode)
    
    def test_otf2reader_set_file_path(self):
        """Test setting file path"""
        reader = OTF2Reader()
        reader.setFilePath("/test/trace.otf2")
        assert reader.getFilePath() == "/test/trace.otf2"
    
    def test_otf2reader_load(self):
        """Test loading trace"""
        import tempfile
        import os
        
        # Create a temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("ENTER,1,main,0,0,0.0,0\n")
            f.write("LEAVE,2,main,0,0,1.0,0\n")
            temp_file = f.name
        
        try:
            reader = OTF2Reader(temp_file)
            trace = reader.load()
            
            assert trace is not None
            assert isinstance(trace, Trace)
            assert reader.getTrace() == trace
        finally:
            os.unlink(temp_file)
    
    def test_otf2reader_run(self):
        """Test running OTF2Reader"""
        import tempfile
        import os
        
        # Create a temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("ENTER,1,main,0,0,0.0,0\n")
            f.write("LEAVE,2,main,0,0,1.0,0\n")
            temp_file = f.name
        
        try:
            reader = OTF2Reader(temp_file)
            reader.run()
            
            # Should have trace in outputs
            outputs = reader.get_outputs().get_data()
            assert len(outputs) == 1
            
            # Output should be a Trace
            output_trace = list(outputs)[0]
            assert isinstance(output_trace, Trace)
        finally:
            os.unlink(temp_file)
