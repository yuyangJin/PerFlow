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
