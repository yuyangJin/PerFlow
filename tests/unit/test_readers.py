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
        import tempfile
        import os
        
        # Create a temporary JSON file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            f.write('{"events": [{"name": "test", "pid": 0, "tid": 0, "timestamp": 1.0}]}')
            temp_file = f.name
        
        try:
            reader = VtuneReader(temp_file)
            trace = reader.load()
            
            assert trace is not None
            assert isinstance(trace, Trace)
            assert reader.getTrace() == trace
        finally:
            os.unlink(temp_file)
    
    def test_vtune_reader_run(self):
        """Test running VtuneReader"""
        import tempfile
        import os
        
        # Create a temporary JSON file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            f.write('{"events": [{"name": "test", "pid": 0, "tid": 0, "timestamp": 1.0}]}')
            temp_file = f.name
        
        try:
            reader = VtuneReader(temp_file)
            reader.run()
            
            outputs = reader.get_outputs().get_data()
            assert len(outputs) == 1
            
            output_trace = list(outputs)[0]
            assert isinstance(output_trace, Trace)
        finally:
            os.unlink(temp_file)


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
        import tempfile
        import os
        
        # Create a temporary JSON file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            f.write('{"traceEvents": [{"name": "kernel", "ph": "B", "pid": 0, "tid": 0, "ts": 1000000}]}')
            temp_file = f.name
        
        try:
            reader = NsightReader(temp_file)
            trace = reader.load()
            
            assert trace is not None
            assert isinstance(trace, Trace)
            assert reader.getTrace() == trace
        finally:
            os.unlink(temp_file)
    
    def test_nsight_reader_run(self):
        """Test running NsightReader"""
        import tempfile
        import os
        
        # Create a temporary JSON file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            f.write('{"traceEvents": [{"name": "kernel", "ph": "B", "pid": 0, "tid": 0, "ts": 1000000}]}')
            temp_file = f.name
        
        try:
            reader = NsightReader(temp_file)
            reader.run()
            
            outputs = reader.get_outputs().get_data()
            assert len(outputs) == 1
            
            output_trace = list(outputs)[0]
            assert isinstance(output_trace, Trace)
        finally:
            os.unlink(temp_file)


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
        import tempfile
        import os
        
        # Create a temporary CSV file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
            f.write("function,file,line,exclusive_time,inclusive_time\n")
            f.write("main,main.c,10,0.5,1.0\n")
            temp_file = f.name
        
        try:
            reader = HpctoolkitReader(temp_file)
            trace = reader.load()
            
            assert trace is not None
            assert isinstance(trace, Trace)
            assert reader.getTrace() == trace
        finally:
            os.unlink(temp_file)
    
    def test_hpctoolkit_reader_run(self):
        """Test running HpctoolkitReader"""
        import tempfile
        import os
        
        # Create a temporary CSV file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
            f.write("function,file,line,exclusive_time,inclusive_time\n")
            f.write("main,main.c,10,0.5,1.0\n")
            temp_file = f.name
        
        try:
            reader = HpctoolkitReader(temp_file)
            reader.run()
            
            outputs = reader.get_outputs().get_data()
            assert len(outputs) == 1
            
            output_trace = list(outputs)[0]
            assert isinstance(output_trace, Trace)
        finally:
            os.unlink(temp_file)


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
        import tempfile
        import os
        
        # Create a temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("[1.5] event_enter: name=main\n")
            f.write("[2.5] event_leave: name=main\n")
            temp_file = f.name
        
        try:
            reader = CTFReader(temp_file)
            trace = reader.load()
            
            assert trace is not None
            assert isinstance(trace, Trace)
            assert reader.getTrace() == trace
        finally:
            os.unlink(temp_file)
    
    def test_ctf_reader_run(self):
        """Test running CTFReader"""
        import tempfile
        import os
        
        # Create a temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("[1.5] event_enter: name=main\n")
            f.write("[2.5] event_leave: name=main\n")
            temp_file = f.name
        
        try:
            reader = CTFReader(temp_file)
            reader.run()
            
            outputs = reader.get_outputs().get_data()
            assert len(outputs) == 1
            
            output_trace = list(outputs)[0]
            assert isinstance(output_trace, Trace)
        finally:
            os.unlink(temp_file)


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
        import tempfile
        import os
        
        # Create a temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("SEND,1,MPI_Send,0,0,1.0,0\n")
            f.write("RECV,2,MPI_Recv,1,0,1.5,1\n")
            temp_file = f.name
        
        try:
            reader = ScalatraceReader(temp_file)
            trace = reader.load()
            
            assert trace is not None
            assert isinstance(trace, Trace)
            assert reader.getTrace() == trace
        finally:
            os.unlink(temp_file)
    
    def test_scalatrace_reader_run(self):
        """Test running ScalatraceReader"""
        import tempfile
        import os
        
        # Create a temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("SEND,1,MPI_Send,0,0,1.0,0\n")
            f.write("RECV,2,MPI_Recv,1,0,1.5,1\n")
            temp_file = f.name
        
        try:
            reader = ScalatraceReader(temp_file)
            reader.run()
            
            outputs = reader.get_outputs().get_data()
            assert len(outputs) == 1
            
            output_trace = list(outputs)[0]
            assert isinstance(output_trace, Trace)
        finally:
            os.unlink(temp_file)
