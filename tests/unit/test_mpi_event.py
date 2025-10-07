"""
Unit tests for MPI Event classes
"""
import pytest
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import (
    MpiEvent, MpiP2PEvent, MpiSendEvent, MpiRecvEvent
)


class TestMpiEvent:
    """Test MpiEvent class"""
    
    def test_mpi_event_creation(self):
        """Test creating MPI event"""
        event = MpiEvent(
            event_type=EventType.SEND,
            idx=1,
            name="mpi_send",
            pid=0,
            tid=0,
            timestamp=1.0,
            replay_pid=0,
            communicator=1
        )
        assert event.getType() == EventType.SEND
        assert event.getCommunicator() == 1
    
    def test_mpi_event_inherits_from_event(self):
        """Test that MpiEvent inherits from Event"""
        event = MpiEvent()
        assert isinstance(event, Event)
    
    def test_mpi_event_set_communicator(self):
        """Test setting communicator"""
        event = MpiEvent()
        event.setCommunicator(5)
        assert event.getCommunicator() == 5


class TestMpiP2PEvent:
    """Test MpiP2PEvent class"""
    
    def test_p2p_event_creation(self):
        """Test creating point-to-point event"""
        event = MpiP2PEvent(
            event_type=EventType.SEND,
            idx=1,
            name="p2p_send",
            pid=0,
            tid=0,
            timestamp=1.0,
            replay_pid=0,
            communicator=1,
            tag=100
        )
        assert event.getTag() == 100
        assert event.getCommunicator() == 1
    
    def test_p2p_event_inherits_from_mpi_event(self):
        """Test that MpiP2PEvent inherits from MpiEvent"""
        event = MpiP2PEvent()
        assert isinstance(event, MpiEvent)
        assert isinstance(event, Event)
    
    def test_p2p_event_set_tag(self):
        """Test setting tag"""
        event = MpiP2PEvent()
        event.setTag(200)
        assert event.getTag() == 200


class TestMpiSendEvent:
    """Test MpiSendEvent class"""
    
    def test_send_event_creation(self):
        """Test creating send event"""
        event = MpiSendEvent(
            idx=1,
            name="send",
            pid=0,
            tid=0,
            timestamp=1.0,
            replay_pid=0,
            communicator=1,
            tag=100,
            dest_pid=1
        )
        assert event.getType() == EventType.SEND
        assert event.getDestPid() == 1
        assert event.getTag() == 100
        assert event.getCommunicator() == 1
    
    def test_send_event_inherits_from_p2p_event(self):
        """Test that MpiSendEvent inherits from MpiP2PEvent"""
        event = MpiSendEvent()
        assert isinstance(event, MpiP2PEvent)
        assert isinstance(event, MpiEvent)
        assert isinstance(event, Event)
    
    def test_send_event_set_dest_pid(self):
        """Test setting destination process ID"""
        event = MpiSendEvent()
        event.setDestPid(5)
        assert event.getDestPid() == 5
    
    def test_send_event_recv_event_matching(self):
        """Test send-recv event matching"""
        send_event = MpiSendEvent()
        recv_event = MpiRecvEvent()
        
        send_event.setRecvEvent(recv_event)
        assert send_event.getRecvEvent() == recv_event


class TestMpiRecvEvent:
    """Test MpiRecvEvent class"""
    
    def test_recv_event_creation(self):
        """Test creating receive event"""
        event = MpiRecvEvent(
            idx=2,
            name="recv",
            pid=1,
            tid=0,
            timestamp=2.0,
            replay_pid=1,
            communicator=1,
            tag=100,
            src_pid=0
        )
        assert event.getType() == EventType.RECV
        assert event.getSrcPid() == 0
        assert event.getTag() == 100
        assert event.getCommunicator() == 1
    
    def test_recv_event_inherits_from_p2p_event(self):
        """Test that MpiRecvEvent inherits from MpiP2PEvent"""
        event = MpiRecvEvent()
        assert isinstance(event, MpiP2PEvent)
        assert isinstance(event, MpiEvent)
        assert isinstance(event, Event)
    
    def test_recv_event_set_src_pid(self):
        """Test setting source process ID"""
        event = MpiRecvEvent()
        event.setSrcPid(3)
        assert event.getSrcPid() == 3
    
    def test_recv_event_send_event_matching(self):
        """Test recv-send event matching"""
        send_event = MpiSendEvent()
        recv_event = MpiRecvEvent()
        
        recv_event.setSendEvent(send_event)
        assert recv_event.getSendEvent() == send_event


class TestMpiEventMatching:
    """Test MPI event matching between send and receive"""
    
    def test_bidirectional_matching(self):
        """Test bidirectional send-recv matching"""
        send_event = MpiSendEvent(
            idx=1,
            name="send",
            pid=0,
            tid=0,
            timestamp=1.0,
            communicator=1,
            tag=100,
            dest_pid=1
        )
        
        recv_event = MpiRecvEvent(
            idx=2,
            name="recv",
            pid=1,
            tid=0,
            timestamp=2.0,
            communicator=1,
            tag=100,
            src_pid=0
        )
        
        # Match the events
        send_event.setRecvEvent(recv_event)
        recv_event.setSendEvent(send_event)
        
        # Verify matching
        assert send_event.getRecvEvent() == recv_event
        assert recv_event.getSendEvent() == send_event
        assert send_event.getDestPid() == recv_event.getPid()
        assert recv_event.getSrcPid() == send_event.getPid()
