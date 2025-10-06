"""
Unit tests for TraceReplayer and LateSender classes
"""
import pytest
from perflow.task.trace_analysis.low_level.trace_replayer import TraceReplayer
from perflow.task.trace_analysis.late_sender import LateSender
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.flow.flow import FlowNode


class TestTraceReplayer:
    """Test TraceReplayer class"""
    
    def test_tracereplayer_creation(self):
        """Test creating TraceReplayer"""
        replayer = TraceReplayer()
        assert replayer.getTrace() is None
    
    def test_tracereplayer_creation_with_trace(self):
        """Test creating TraceReplayer with trace"""
        trace = Trace()
        replayer = TraceReplayer(trace)
        assert replayer.getTrace() == trace
    
    def test_tracereplayer_inherits_from_flownode(self):
        """Test that TraceReplayer inherits from FlowNode"""
        replayer = TraceReplayer()
        assert isinstance(replayer, FlowNode)
    
    def test_tracereplayer_set_trace(self):
        """Test setting trace"""
        replayer = TraceReplayer()
        trace = Trace()
        
        replayer.setTrace(trace)
        assert replayer.getTrace() == trace
    
    def test_tracereplayer_register_callback(self):
        """Test registering callback"""
        replayer = TraceReplayer()
        trace = Trace()
        trace.addEvent(Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0))
        replayer.setTrace(trace)
        
        callback_called = []
        
        def test_callback(event):
            callback_called.append(event.getName())
        
        replayer.registerCallback("test", test_callback)
        replayer.forwardReplay()
        
        assert len(callback_called) == 1
        assert "func1" in callback_called
    
    def test_tracereplayer_forward_replay(self):
        """Test forward replay"""
        replayer = TraceReplayer()
        trace = Trace()
        
        # Add events in order
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.COMPUTE, 2, "compute", 0, 0, 2.0, 0)
        event3 = Event(EventType.LEAVE, 3, "func1", 0, 0, 3.0, 0)
        
        trace.addEvent(event1)
        trace.addEvent(event2)
        trace.addEvent(event3)
        replayer.setTrace(trace)
        
        replayed_events = []
        replayer.registerCallback("recorder", lambda e: replayed_events.append(e))
        
        replayer.forwardReplay()
        
        assert len(replayed_events) == 3
        assert replayed_events[0] == event1
        assert replayed_events[1] == event2
        assert replayed_events[2] == event3
    
    def test_tracereplayer_backward_replay(self):
        """Test backward replay"""
        replayer = TraceReplayer()
        trace = Trace()
        
        # Add events in order
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.COMPUTE, 2, "compute", 0, 0, 2.0, 0)
        event3 = Event(EventType.LEAVE, 3, "func1", 0, 0, 3.0, 0)
        
        trace.addEvent(event1)
        trace.addEvent(event2)
        trace.addEvent(event3)
        replayer.setTrace(trace)
        
        replayed_events = []
        replayer.registerCallback("recorder", lambda e: replayed_events.append(e))
        
        replayer.backwardReplay()
        
        # Events should be in reverse order
        assert len(replayed_events) == 3
        assert replayed_events[0] == event3
        assert replayed_events[1] == event2
        assert replayed_events[2] == event1
    
    def test_tracereplayer_multiple_callbacks(self):
        """Test multiple callbacks"""
        replayer = TraceReplayer()
        trace = Trace()
        trace.addEvent(Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0))
        replayer.setTrace(trace)
        
        callback1_count = [0]
        callback2_count = [0]
        
        replayer.registerCallback("cb1", lambda e: callback1_count.__setitem__(0, callback1_count[0] + 1))
        replayer.registerCallback("cb2", lambda e: callback2_count.__setitem__(0, callback2_count[0] + 1))
        
        replayer.forwardReplay()
        
        assert callback1_count[0] == 1
        assert callback2_count[0] == 1
    
    def test_tracereplayer_unregister_callback(self):
        """Test unregistering callback"""
        replayer = TraceReplayer()
        trace = Trace()
        trace.addEvent(Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0))
        replayer.setTrace(trace)
        
        callback_count = [0]
        replayer.registerCallback("test", lambda e: callback_count.__setitem__(0, callback_count[0] + 1))
        
        replayer.forwardReplay()
        assert callback_count[0] == 1
        
        # Unregister and replay again
        replayer.unregisterCallback("test")
        replayer.forwardReplay()
        
        # Count should still be 1 (callback not called second time)
        assert callback_count[0] == 1


class TestLateSender:
    """Test LateSender class"""
    
    def test_latesender_creation(self):
        """Test creating LateSender"""
        analyzer = LateSender()
        assert analyzer.getTrace() is None
        assert len(analyzer.getLateSends()) == 0
        assert len(analyzer.getWaitTimes()) == 0
    
    def test_latesender_inherits_from_tracereplayer(self):
        """Test that LateSender inherits from TraceReplayer"""
        analyzer = LateSender()
        assert isinstance(analyzer, TraceReplayer)
        assert isinstance(analyzer, FlowNode)
    
    def test_latesender_no_late_sends(self):
        """Test when there are no late sends"""
        trace = Trace()
        
        # Send happens before receive (not late)
        send = MpiSendEvent(EventType.SEND, 1, "send", 0, 0, 1.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(EventType.RECV, 2, "recv", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(send)
        trace.addEvent(recv)
        
        analyzer = LateSender(trace)
        analyzer.forwardReplay()
        
        assert len(analyzer.getLateSends()) == 0
        assert analyzer.getTotalWaitTime() == 0.0
    
    def test_latesender_with_late_send(self):
        """Test detecting late send"""
        trace = Trace()
        
        # Receive is ready at 1.0, but send doesn't arrive until 2.0 (late!)
        send = MpiSendEvent(EventType.SEND, 1, "send", 0, 0, 2.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(EventType.RECV, 2, "recv", 1, 0, 1.0, 1, 1, 100, src_pid=0)
        
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(recv)
        trace.addEvent(send)
        
        analyzer = LateSender(trace)
        analyzer.forwardReplay()
        
        # Should detect one late send
        late_sends = analyzer.getLateSends()
        assert len(late_sends) == 1
        assert late_sends[0] == send
        
        # Wait time should be 1.0 (2.0 - 1.0)
        assert analyzer.getTotalWaitTime() == 1.0
    
    def test_latesender_multiple_late_sends(self):
        """Test detecting multiple late sends"""
        trace = Trace()
        
        # Late send 1: wait time = 1.0
        send1 = MpiSendEvent(EventType.SEND, 1, "send1", 0, 0, 3.0, 0, 1, 100, dest_pid=1)
        recv1 = MpiRecvEvent(EventType.RECV, 2, "recv1", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        send1.setRecvEvent(recv1)
        recv1.setSendEvent(send1)
        
        # Late send 2: wait time = 2.0
        send2 = MpiSendEvent(EventType.SEND, 3, "send2", 0, 0, 7.0, 0, 1, 200, dest_pid=1)
        recv2 = MpiRecvEvent(EventType.RECV, 4, "recv2", 1, 0, 5.0, 1, 1, 200, src_pid=0)
        send2.setRecvEvent(recv2)
        recv2.setSendEvent(send2)
        
        trace.addEvent(recv1)
        trace.addEvent(send1)
        trace.addEvent(recv2)
        trace.addEvent(send2)
        
        analyzer = LateSender(trace)
        analyzer.forwardReplay()
        
        # Should detect two late sends
        assert len(analyzer.getLateSends()) == 2
        
        # Total wait time should be 3.0 (1.0 + 2.0)
        assert analyzer.getTotalWaitTime() == 3.0
    
    def test_latesender_clear(self):
        """Test clearing late sender results"""
        trace = Trace()
        
        send = MpiSendEvent(EventType.SEND, 1, "send", 0, 0, 2.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(EventType.RECV, 2, "recv", 1, 0, 1.0, 1, 1, 100, src_pid=0)
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(recv)
        trace.addEvent(send)
        
        analyzer = LateSender(trace)
        analyzer.forwardReplay()
        
        assert len(analyzer.getLateSends()) > 0
        
        analyzer.clear()
        
        assert len(analyzer.getLateSends()) == 0
        assert len(analyzer.getWaitTimes()) == 0
        assert analyzer.getTotalWaitTime() == 0.0
