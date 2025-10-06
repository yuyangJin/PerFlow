"""
Unit tests for Event classes
"""
import pytest
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType


class TestEventType:
    """Test EventType enum"""
    
    def test_event_types_exist(self):
        """Test that all event types are defined"""
        assert EventType.UNKNOWN
        assert EventType.ENTER
        assert EventType.LEAVE
        assert EventType.SEND
        assert EventType.RECV
        assert EventType.COLLECTIVE
        assert EventType.BARRIER
        assert EventType.COMPUTE
    
    def test_event_type_values(self):
        """Test event type values are unique"""
        types = [EventType.UNKNOWN, EventType.ENTER, EventType.LEAVE,
                 EventType.SEND, EventType.RECV, EventType.COLLECTIVE,
                 EventType.BARRIER, EventType.COMPUTE]
        values = [t.value for t in types]
        assert len(values) == len(set(values))


class TestEvent:
    """Test Event class"""
    
    def test_event_creation_default(self):
        """Test creating event with default parameters"""
        event = Event()
        assert event.m_type is None
        assert event.m_idx is None
        assert event.m_name is None
        assert event.m_pid is None
        assert event.m_tid is None
        assert event.m_timestamp is None
        assert event.m_replay_pid is None
    
    def test_event_creation_with_parameters(self):
        """Test creating event with all parameters"""
        event = Event(
            event_type=EventType.SEND,
            idx=1,
            name="test_send",
            pid=0,
            tid=0,
            timestamp=1.5,
            replay_pid=0
        )
        assert event.m_type == EventType.SEND
        assert event.m_idx == 1
        assert event.m_name == "test_send"
        assert event.m_pid == 0
        assert event.m_tid == 0
        assert event.m_timestamp == 1.5
        assert event.m_replay_pid == 0
    
    def test_event_getters(self):
        """Test event getter methods"""
        event = Event(
            event_type=EventType.RECV,
            idx=2,
            name="test_recv",
            pid=1,
            tid=1,
            timestamp=2.5,
            replay_pid=1
        )
        assert event.getType() == EventType.RECV
        assert event.getIdx() == 2
        assert event.getName() == "test_recv"
        assert event.getPid() == 1
        assert event.getTid() == 1
        assert event.getTimestamp() == 2.5
        assert event.getReplayPid() == 1
    
    def test_event_partial_initialization(self):
        """Test creating event with partial parameters"""
        event = Event(event_type=EventType.ENTER, name="func1")
        assert event.getType() == EventType.ENTER
        assert event.getName() == "func1"
        assert event.getIdx() is None
        assert event.getPid() is None
