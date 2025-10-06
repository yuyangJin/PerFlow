"""
Unit tests for additional TraceReader classes
"""
import pytest
from perflow.task.reader.vtune_reader import VtuneReader
from perflow.task.reader.nsight_reader import NsightReader
from perflow.task.reader.hpctoolkit_reader import HpctoolkitReader
from perflow.task.reader.ctf_reader import CTFReader
from perflow.task.reader.scalatrace_reader import ScalatraceReader
from perflow.perf_data_struct.dynamic.trace.trace import Trace
from perflow.flow.flow import FlowNode


class TestVtuneReader:
    """Test VtuneReader class"""
    
    def test_vtune_reader_creation(self):
        """Test creating VtuneReader"""
        reader = VtuneReader()
        assert reader.getFilePath() is None
        assert reader.getTrace() is None
    
    def test_vtune_reader_creation_with_path(self):
        """Test creating VtuneReader with file path"""
        reader = VtuneReader("/path/to/vtune_results")
        assert reader.getFilePath() == "/path/to/vtune_results"
    
    def test_vtune_reader_inherits_from_flownode(self):
        """Test that VtuneReader inherits from FlowNode"""
        reader = VtuneReader()
        assert isinstance(reader, FlowNode)
    
    def test_vtune_reader_set_file_path(self):
        """Test setting file path"""
        reader = VtuneReader()
        reader.setFilePath("/test/vtune.result")
        assert reader.getFilePath() == "/test/vtune.result"
    
    def test_vtune_reader_load(self):
        """Test loading trace"""
        reader = VtuneReader("/test/vtune.result")
        trace = reader.load()
        
        assert trace is not None
        assert isinstance(trace, Trace)
        assert reader.getTrace() == trace
    
    def test_vtune_reader_run(self):
        """Test running VtuneReader"""
        reader = VtuneReader("/test/vtune.result")
        reader.run()
        
        outputs = reader.get_outputs().get_data()
        assert len(outputs) == 1
        
        output_trace = list(outputs)[0]
        assert isinstance(output_trace, Trace)


class TestNsightReader:
    """Test NsightReader class"""
    
    def test_nsight_reader_creation(self):
        """Test creating NsightReader"""
        reader = NsightReader()
        assert reader.getFilePath() is None
        assert reader.getTrace() is None
    
    def test_nsight_reader_creation_with_path(self):
        """Test creating NsightReader with file path"""
        reader = NsightReader("/path/to/report.nsys-rep")
        assert reader.getFilePath() == "/path/to/report.nsys-rep"
    
    def test_nsight_reader_inherits_from_flownode(self):
        """Test that NsightReader inherits from FlowNode"""
        reader = NsightReader()
        assert isinstance(reader, FlowNode)
    
    def test_nsight_reader_set_file_path(self):
        """Test setting file path"""
        reader = NsightReader()
        reader.setFilePath("/test/report.nsys-rep")
        assert reader.getFilePath() == "/test/report.nsys-rep"
    
    def test_nsight_reader_load(self):
        """Test loading trace"""
        reader = NsightReader("/test/report.nsys-rep")
        trace = reader.load()
        
        assert trace is not None
        assert isinstance(trace, Trace)
        assert reader.getTrace() == trace
    
    def test_nsight_reader_run(self):
        """Test running NsightReader"""
        reader = NsightReader("/test/report.nsys-rep")
        reader.run()
        
        outputs = reader.get_outputs().get_data()
        assert len(outputs) == 1
        
        output_trace = list(outputs)[0]
        assert isinstance(output_trace, Trace)


class TestHpctoolkitReader:
    """Test HpctoolkitReader class"""
    
    def test_hpctoolkit_reader_creation(self):
        """Test creating HpctoolkitReader"""
        reader = HpctoolkitReader()
        assert reader.getFilePath() is None
        assert reader.getTrace() is None
    
    def test_hpctoolkit_reader_creation_with_path(self):
        """Test creating HpctoolkitReader with file path"""
        reader = HpctoolkitReader("/path/to/hpctoolkit-db")
        assert reader.getFilePath() == "/path/to/hpctoolkit-db"
    
    def test_hpctoolkit_reader_inherits_from_flownode(self):
        """Test that HpctoolkitReader inherits from FlowNode"""
        reader = HpctoolkitReader()
        assert isinstance(reader, FlowNode)
    
    def test_hpctoolkit_reader_set_file_path(self):
        """Test setting file path"""
        reader = HpctoolkitReader()
        reader.setFilePath("/test/hpctoolkit-db")
        assert reader.getFilePath() == "/test/hpctoolkit-db"
    
    def test_hpctoolkit_reader_load(self):
        """Test loading trace"""
        reader = HpctoolkitReader("/test/hpctoolkit-db")
        trace = reader.load()
        
        assert trace is not None
        assert isinstance(trace, Trace)
        assert reader.getTrace() == trace
    
    def test_hpctoolkit_reader_run(self):
        """Test running HpctoolkitReader"""
        reader = HpctoolkitReader("/test/hpctoolkit-db")
        reader.run()
        
        outputs = reader.get_outputs().get_data()
        assert len(outputs) == 1
        
        output_trace = list(outputs)[0]
        assert isinstance(output_trace, Trace)


class TestCTFReader:
    """Test CTFReader class"""
    
    def test_ctf_reader_creation(self):
        """Test creating CTFReader"""
        reader = CTFReader()
        assert reader.getFilePath() is None
        assert reader.getTrace() is None
    
    def test_ctf_reader_creation_with_path(self):
        """Test creating CTFReader with file path"""
        reader = CTFReader("/path/to/ctf_trace")
        assert reader.getFilePath() == "/path/to/ctf_trace"
    
    def test_ctf_reader_inherits_from_flownode(self):
        """Test that CTFReader inherits from FlowNode"""
        reader = CTFReader()
        assert isinstance(reader, FlowNode)
    
    def test_ctf_reader_set_file_path(self):
        """Test setting file path"""
        reader = CTFReader()
        reader.setFilePath("/test/ctf_trace")
        assert reader.getFilePath() == "/test/ctf_trace"
    
    def test_ctf_reader_load(self):
        """Test loading trace"""
        reader = CTFReader("/test/ctf_trace")
        trace = reader.load()
        
        assert trace is not None
        assert isinstance(trace, Trace)
        assert reader.getTrace() == trace
    
    def test_ctf_reader_run(self):
        """Test running CTFReader"""
        reader = CTFReader("/test/ctf_trace")
        reader.run()
        
        outputs = reader.get_outputs().get_data()
        assert len(outputs) == 1
        
        output_trace = list(outputs)[0]
        assert isinstance(output_trace, Trace)


class TestScalatraceReader:
    """Test ScalatraceReader class"""
    
    def test_scalatrace_reader_creation(self):
        """Test creating ScalatraceReader"""
        reader = ScalatraceReader()
        assert reader.getFilePath() is None
        assert reader.getTrace() is None
    
    def test_scalatrace_reader_creation_with_path(self):
        """Test creating ScalatraceReader with file path"""
        reader = ScalatraceReader("/path/to/scalatrace.sct")
        assert reader.getFilePath() == "/path/to/scalatrace.sct"
    
    def test_scalatrace_reader_inherits_from_flownode(self):
        """Test that ScalatraceReader inherits from FlowNode"""
        reader = ScalatraceReader()
        assert isinstance(reader, FlowNode)
    
    def test_scalatrace_reader_set_file_path(self):
        """Test setting file path"""
        reader = ScalatraceReader()
        reader.setFilePath("/test/scalatrace.sct")
        assert reader.getFilePath() == "/test/scalatrace.sct"
    
    def test_scalatrace_reader_load(self):
        """Test loading trace"""
        reader = ScalatraceReader("/test/scalatrace.sct")
        trace = reader.load()
        
        assert trace is not None
        assert isinstance(trace, Trace)
        assert reader.getTrace() == trace
    
    def test_scalatrace_reader_run(self):
        """Test running ScalatraceReader"""
        reader = ScalatraceReader("/test/scalatrace.sct")
        reader.run()
        
        outputs = reader.get_outputs().get_data()
        assert len(outputs) == 1
        
        output_trace = list(outputs)[0]
        assert isinstance(output_trace, Trace)
