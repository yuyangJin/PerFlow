"""
Unit tests for Trace and TraceInfo classes
"""
import pytest
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType


class TestTraceInfo:
    """Test TraceInfo class"""
    
    def test_traceinfo_creation_default(self):
        """Test creating TraceInfo with default parameters"""
        trace_info = TraceInfo()
        assert trace_info.m_pid is None
        assert trace_info.m_tid is None
        assert trace_info.m_num_execution_processes is None
        assert trace_info.m_num_replay_processes is None
        assert trace_info.m_ep_to_rp_mapping == {}
    
    def test_traceinfo_creation_with_parameters(self):
        """Test creating TraceInfo with parameters"""
        trace_info = TraceInfo(pid=0, tid=1)
        assert trace_info.m_pid == 0
        assert trace_info.m_tid == 1
    
    def test_traceinfo_getters(self):
        """Test TraceInfo getter methods"""
        trace_info = TraceInfo(pid=5, tid=10)
        assert trace_info.getPid() == 5
        assert trace_info.getTid() == 10
    
    def test_traceinfo_setters(self):
        """Test TraceInfo setter methods"""
        trace_info = TraceInfo()
        trace_info.setPid(3)
        trace_info.setTid(7)
        assert trace_info.getPid() == 3
        assert trace_info.getTid() == 7
    
    def test_traceinfo_execution_processes(self):
        """Test execution process metadata"""
        trace_info = TraceInfo(num_execution_processes=128)
        assert trace_info.getNumExecutionProcesses() == 128
        
        trace_info.setNumExecutionProcesses(256)
        assert trace_info.getNumExecutionProcesses() == 256
    
    def test_traceinfo_replay_processes(self):
        """Test replay process metadata"""
        trace_info = TraceInfo(num_replay_processes=32)
        assert trace_info.getNumReplayProcesses() == 32
        
        trace_info.setNumReplayProcesses(64)
        assert trace_info.getNumReplayProcesses() == 64
    
    def test_traceinfo_ep_to_rp_mapping(self):
        """Test execution to replay process mapping"""
        trace_info = TraceInfo()
        
        # Add individual mappings
        trace_info.addEpToRpMapping(0, 0)
        trace_info.addEpToRpMapping(1, 0)
        trace_info.addEpToRpMapping(2, 1)
        trace_info.addEpToRpMapping(3, 1)
        
        assert trace_info.getReplayProcessForEp(0) == 0
        assert trace_info.getReplayProcessForEp(1) == 0
        assert trace_info.getReplayProcessForEp(2) == 1
        assert trace_info.getReplayProcessForEp(3) == 1
        assert trace_info.getReplayProcessForEp(99) is None
        
        # Set entire mapping
        new_mapping = {10: 5, 11: 5, 12: 6, 13: 6}
        trace_info.setEpToRpMapping(new_mapping)
        assert trace_info.getEpToRpMapping() == new_mapping
        assert trace_info.getReplayProcessForEp(10) == 5
    
    def test_traceinfo_trace_format(self):
        """Test trace format metadata"""
        trace_info = TraceInfo(trace_format="OTF2")
        assert trace_info.getTraceFormat() == "OTF2"
        
        trace_info.setTraceFormat("Scalatrace")
        assert trace_info.getTraceFormat() == "Scalatrace"
    
    def test_traceinfo_trace_times(self):
        """Test trace timing metadata"""
        trace_info = TraceInfo(trace_start_time=0.0, trace_end_time=10.5)
        assert trace_info.getTraceStartTime() == 0.0
        assert trace_info.getTraceEndTime() == 10.5
        assert trace_info.getTraceDuration() == 10.5
        
        trace_info.setTraceStartTime(1.5)
        trace_info.setTraceEndTime(15.0)
        assert trace_info.getTraceDuration() == 13.5
    
    def test_traceinfo_application_name(self):
        """Test application name metadata"""
        trace_info = TraceInfo(application_name="MPI_Test")
        assert trace_info.getApplicationName() == "MPI_Test"
        
        trace_info.setApplicationName("LAMMPS")
        assert trace_info.getApplicationName() == "LAMMPS"
    
    def test_traceinfo_comprehensive(self):
        """Test comprehensive TraceInfo with all metadata"""
        ep_to_rp = {i: i // 4 for i in range(16)}  # 16 EPs mapped to 4 RPs
        
        trace_info = TraceInfo(
            pid=0,
            tid=0,
            num_execution_processes=16,
            num_replay_processes=4,
            ep_to_rp_mapping=ep_to_rp,
            trace_format="OTF2",
            trace_start_time=0.0,
            trace_end_time=100.0,
            application_name="ParallelApp"
        )
        
        # Verify all attributes
        assert trace_info.getPid() == 0
        assert trace_info.getTid() == 0
        assert trace_info.getNumExecutionProcesses() == 16
        assert trace_info.getNumReplayProcesses() == 4
        assert trace_info.getReplayProcessForEp(5) == 1
        assert trace_info.getReplayProcessForEp(15) == 3
        assert trace_info.getTraceFormat() == "OTF2"
        assert trace_info.getTraceDuration() == 100.0
        assert trace_info.getApplicationName() == "ParallelApp"


class TestTrace:
    """Test Trace class"""
    
    def test_trace_creation(self):
        """Test creating empty trace"""
        trace = Trace()
        assert trace.getEventCount() == 0
        assert len(trace.getEvents()) == 0
        assert trace.getTraceInfo() is not None
    
    def test_trace_add_event(self):
        """Test adding events to trace"""
        trace = Trace()
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.LEAVE, 2, "func1", 0, 0, 2.0, 0)
        
        trace.addEvent(event1)
        assert trace.getEventCount() == 1
        
        trace.addEvent(event2)
        assert trace.getEventCount() == 2
    
    def test_trace_get_events(self):
        """Test getting all events from trace"""
        trace = Trace()
        event1 = Event(EventType.SEND, 1, "send", 0, 0, 1.0, 0)
        event2 = Event(EventType.RECV, 2, "recv", 0, 0, 2.0, 0)
        
        trace.addEvent(event1)
        trace.addEvent(event2)
        
        events = trace.getEvents()
        assert len(events) == 2
        assert events[0] == event1
        assert events[1] == event2
    
    def test_trace_get_event_by_index(self):
        """Test getting event by index"""
        trace = Trace()
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.LEAVE, 2, "func1", 0, 0, 2.0, 0)
        
        trace.addEvent(event1)
        trace.addEvent(event2)
        
        assert trace.getEvent(0) == event1
        assert trace.getEvent(1) == event2
        assert trace.getEvent(2) is None
        assert trace.getEvent(-1) is None
    
    def test_trace_set_trace_info(self):
        """Test setting trace info"""
        trace = Trace()
        trace_info = TraceInfo(pid=5, tid=10)
        
        trace.setTraceInfo(trace_info)
        retrieved_info = trace.getTraceInfo()
        
        assert retrieved_info.getPid() == 5
        assert retrieved_info.getTid() == 10
    
    def test_trace_clear(self):
        """Test clearing trace events"""
        trace = Trace()
        event1 = Event(EventType.SEND, 1, "send", 0, 0, 1.0, 0)
        event2 = Event(EventType.RECV, 2, "recv", 0, 0, 2.0, 0)
        
        trace.addEvent(event1)
        trace.addEvent(event2)
        assert trace.getEventCount() == 2
        
        trace.clear()
        assert trace.getEventCount() == 0
        assert len(trace.getEvents()) == 0
    
    def test_trace_multiple_operations(self):
        """Test multiple operations on trace"""
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        # Add events
        for i in range(5):
            event = Event(EventType.COMPUTE, i, f"compute_{i}", 0, 0, float(i), 0)
            trace.addEvent(event)
        
        assert trace.getEventCount() == 5
        
        # Check specific event
        event_2 = trace.getEvent(2)
        assert event_2.getIdx() == 2
        assert event_2.getName() == "compute_2"
        
        # Clear and verify
        trace.clear()
        assert trace.getEventCount() == 0
