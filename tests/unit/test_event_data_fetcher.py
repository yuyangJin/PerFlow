"""
Unit tests for EventDataFetcher class
"""
import pytest
from perflow.task.trace_analysis.low_level.event_data_fetcher import EventDataFetcher
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType


class TestEventDataFetcher:
    """Test EventDataFetcher class"""
    
    def test_eventdatafetcher_creation(self):
        """Test creating EventDataFetcher"""
        fetcher = EventDataFetcher()
        assert len(fetcher.m_local_traces) == 0
        assert len(fetcher.m_ep_to_rp_mapping) == 0
        assert len(fetcher.m_event_cache) == 0
    
    def test_eventdatafetcher_register_traces(self):
        """Test registering traces"""
        fetcher = EventDataFetcher()
        
        # Create traces
        traces = []
        for i in range(3):
            trace = Trace()
            trace_info = TraceInfo(pid=i)
            trace.setTraceInfo(trace_info)
            traces.append(trace)
        
        mapping = {0: 0, 1: 0, 2: 1}
        fetcher.register_traces(traces, mapping)
        
        assert len(fetcher.m_local_traces) == 3
        assert fetcher.m_ep_to_rp_mapping == mapping
    
    def test_eventdatafetcher_is_local_event(self):
        """Test checking if event is local"""
        fetcher = EventDataFetcher()
        
        # Create and register traces
        traces = []
        for i in range(2):
            trace = Trace()
            trace_info = TraceInfo(pid=i)
            trace.setTraceInfo(trace_info)
            traces.append(trace)
        
        mapping = {0: 0, 1: 1}
        fetcher.register_traces(traces, mapping)
        
        assert fetcher.is_local_event(0) is True
        assert fetcher.is_local_event(1) is True
        assert fetcher.is_local_event(2) is False
    
    def test_eventdatafetcher_get_local_event(self):
        """Test getting local event"""
        fetcher = EventDataFetcher()
        
        # Create trace with events
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.LEAVE, 2, "func1", 0, 0, 2.0, 0)
        trace.addEvent(event1)
        trace.addEvent(event2)
        
        mapping = {0: 0}
        fetcher.register_traces([trace], mapping)
        
        # Get events
        retrieved_event1 = fetcher.get_local_event(0, 1)
        assert retrieved_event1 is not None
        assert retrieved_event1.getIdx() == 1
        assert retrieved_event1.getName() == "func1"
        
        retrieved_event2 = fetcher.get_local_event(0, 2)
        assert retrieved_event2 is not None
        assert retrieved_event2.getIdx() == 2
    
    def test_eventdatafetcher_get_local_event_not_found(self):
        """Test getting local event that doesn't exist"""
        fetcher = EventDataFetcher()
        
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        mapping = {0: 0}
        fetcher.register_traces([trace], mapping)
        
        # Try to get non-existent event
        event = fetcher.get_local_event(0, 999)
        assert event is None
    
    def test_eventdatafetcher_get_local_event_wrong_ep(self):
        """Test getting event from non-local EP"""
        fetcher = EventDataFetcher()
        
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        mapping = {0: 0}
        fetcher.register_traces([trace], mapping)
        
        # Try to get event from EP that's not local
        event = fetcher.get_local_event(1, 1)
        assert event is None
    
    def test_eventdatafetcher_create_cache_key(self):
        """Test cache key creation"""
        fetcher = EventDataFetcher()
        
        key1 = fetcher._create_cache_key(0, 1)
        key2 = fetcher._create_cache_key(0, 2)
        key3 = fetcher._create_cache_key(1, 1)
        
        assert key1 == "0:1"
        assert key2 == "0:2"
        assert key3 == "1:1"
        assert key1 != key2
        assert key1 != key3
    
    def test_eventdatafetcher_get_event_data_dict(self):
        """Test converting event to dictionary"""
        fetcher = EventDataFetcher()
        
        event = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event_dict = fetcher._get_event_data_dict(event)
        
        assert event_dict['type'] == EventType.ENTER.value
        assert event_dict['idx'] == 1
        assert event_dict['name'] == "func1"
        assert event_dict['pid'] == 0
        assert event_dict['tid'] == 0
        assert event_dict['timestamp'] == 1.0
        assert event_dict['replay_pid'] == 0
    
    def test_eventdatafetcher_fetch_event_data_local(self):
        """Test fetching local event data"""
        fetcher = EventDataFetcher()
        
        # Create trace with event
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        event = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        trace.addEvent(event)
        
        mapping = {0: 0}
        fetcher.register_traces([trace], mapping)
        
        # Fetch event data
        event_data = fetcher.fetch_event_data(0, 1)
        
        assert event_data is not None
        assert event_data['idx'] == 1
        assert event_data['name'] == "func1"
    
    def test_eventdatafetcher_fetch_event_data_cached(self):
        """Test that fetched event data is cached"""
        fetcher = EventDataFetcher()
        
        # Create trace with event
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        event = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        trace.addEvent(event)
        
        mapping = {0: 0}
        fetcher.register_traces([trace], mapping)
        
        # First fetch
        event_data1 = fetcher.fetch_event_data(0, 1)
        assert event_data1 is not None
        
        # Check cache
        assert fetcher.get_cache_size() == 1
        
        # Second fetch should use cache
        event_data2 = fetcher.fetch_event_data(0, 1)
        assert event_data2 is not None
        assert event_data2 == event_data1
    
    def test_eventdatafetcher_clear_cache(self):
        """Test clearing the cache"""
        fetcher = EventDataFetcher()
        
        # Create trace with event
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        event = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        trace.addEvent(event)
        
        mapping = {0: 0}
        fetcher.register_traces([trace], mapping)
        
        # Fetch to populate cache
        fetcher.fetch_event_data(0, 1)
        assert fetcher.get_cache_size() == 1
        
        # Clear cache
        fetcher.clear_cache()
        assert fetcher.get_cache_size() == 0
    
    def test_eventdatafetcher_fetch_event_data_not_found(self):
        """Test fetching event data that doesn't exist"""
        fetcher = EventDataFetcher()
        
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        mapping = {0: 0}
        fetcher.register_traces([trace], mapping)
        
        # Try to fetch non-existent event
        event_data = fetcher.fetch_event_data(0, 999)
        assert event_data is None
    
    def test_eventdatafetcher_fetch_remote_without_mpi(self):
        """Test fetching remote event data without MPI enabled"""
        fetcher = EventDataFetcher()
        
        # Register local traces
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        mapping = {0: 0, 1: 1}
        fetcher.register_traces([trace], mapping)
        
        # Try to fetch from remote EP (without MPI, should return None)
        event_data = fetcher.fetch_event_data(1, 1)
        assert event_data is None
