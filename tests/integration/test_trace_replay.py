"""
Integration test: Trace replay and analysis
"""
import pytest
from perflow.task.trace_analysis.low_level.trace_replayer import TraceReplayer
from perflow.task.trace_analysis.late_sender import LateSender
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.flow.flow import FlowGraph


class TestTraceReplayIntegration:
    """Integration tests for trace replay"""
    
    def test_simple_trace_replay(self):
        """Test replaying a simple trace"""
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        # Add events
        events = [
            Event(EventType.ENTER, 1, "main", 0, 0, 0.0, 0),
            Event(EventType.ENTER, 2, "func_a", 0, 0, 1.0, 0),
            Event(EventType.LEAVE, 3, "func_a", 0, 0, 3.0, 0),
            Event(EventType.ENTER, 4, "func_b", 0, 0, 4.0, 0),
            Event(EventType.LEAVE, 5, "func_b", 0, 0, 6.0, 0),
            Event(EventType.LEAVE, 6, "main", 0, 0, 7.0, 0),
        ]
        
        for event in events:
            trace.addEvent(event)
        
        # Replay and collect events
        replayer = TraceReplayer(trace)
        replayed = []
        replayer.registerCallback("collector", lambda e: replayed.append(e.getName()))
        replayer.forwardReplay()
        
        # Verify all events were replayed
        assert len(replayed) == 6
        assert replayed == ["main", "func_a", "func_a", "func_b", "func_b", "main"]
    
    def test_trace_replay_with_analysis(self):
        """Test replay with analysis callbacks"""
        trace = Trace()
        
        # Add compute events
        trace.addEvent(Event(EventType.COMPUTE, 1, "compute1", 0, 0, 1.0, 0))
        trace.addEvent(Event(EventType.COMPUTE, 2, "compute2", 0, 0, 2.0, 0))
        trace.addEvent(Event(EventType.COMPUTE, 3, "compute3", 0, 0, 3.0, 0))
        
        replayer = TraceReplayer(trace)
        
        # Track compute events
        compute_count = [0]
        def count_compute(event):
            if event.getType() == EventType.COMPUTE:
                compute_count[0] += 1
        
        replayer.registerCallback("compute_counter", count_compute)
        replayer.forwardReplay()
        
        assert compute_count[0] == 3
    
    def test_backward_replay_integration(self):
        """Test backward replay for debugging"""
        trace = Trace()
        
        # Add events with increasing timestamps
        for i in range(5):
            trace.addEvent(Event(EventType.COMPUTE, i, f"event_{i}", 0, 0, float(i), 0))
        
        replayer = TraceReplayer(trace)
        
        # Collect in reverse order
        replayed_indices = []
        replayer.registerCallback("collector", lambda e: replayed_indices.append(e.getIdx()))
        replayer.backwardReplay()
        
        # Should be in reverse order
        assert replayed_indices == [4, 3, 2, 1, 0]


class TestLateSenderIntegration:
    """Integration tests for late sender analysis"""
    
    def test_late_sender_detection_simple(self):
        """Test detecting a single late sender"""
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        # Process 0 sends at time 5.0
        # Process 1 is ready to receive at time 2.0
        # This is a late sender (wait time = 3.0)
        send = MpiSendEvent(EventType.SEND, 1, "send", 0, 0, 5.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(EventType.RECV, 2, "recv", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        
        # Match events
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(recv)
        trace.addEvent(send)
        
        # Analyze
        analyzer = LateSender(trace)
        analyzer.forwardReplay()
        
        # Verify results
        late_sends = analyzer.getLateSends()
        assert len(late_sends) == 1
        assert late_sends[0] == send
        assert analyzer.getTotalWaitTime() == 3.0
    
    def test_late_sender_analysis_workflow(self):
        """Test late sender analysis in a workflow"""
        # Create trace with multiple communications
        trace = Trace()
        
        # Communication 1: on time
        send1 = MpiSendEvent(EventType.SEND, 1, "send1", 0, 0, 1.0, 0, 1, 100, dest_pid=1)
        recv1 = MpiRecvEvent(EventType.RECV, 2, "recv1", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        send1.setRecvEvent(recv1)
        recv1.setSendEvent(send1)
        
        # Communication 2: late (wait = 2.0)
        send2 = MpiSendEvent(EventType.SEND, 3, "send2", 0, 0, 7.0, 0, 1, 200, dest_pid=1)
        recv2 = MpiRecvEvent(EventType.RECV, 4, "recv2", 1, 0, 5.0, 1, 1, 200, src_pid=0)
        send2.setRecvEvent(recv2)
        recv2.setSendEvent(send2)
        
        # Communication 3: late (wait = 1.0)
        send3 = MpiSendEvent(EventType.SEND, 5, "send3", 0, 0, 11.0, 0, 1, 300, dest_pid=1)
        recv3 = MpiRecvEvent(EventType.RECV, 6, "recv3", 1, 0, 10.0, 1, 1, 300, src_pid=0)
        send3.setRecvEvent(recv3)
        recv3.setSendEvent(send3)
        
        for event in [send1, recv1, send2, recv2, send3, recv3]:
            trace.addEvent(event)
        
        # Build workflow
        workflow = FlowGraph()
        analyzer = LateSender()
        
        # Add trace to input
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis
        analyzer.run()
        
        # Verify results
        assert len(analyzer.getLateSends()) == 2  # send2 and send3 are late
        assert analyzer.getTotalWaitTime() == 3.0  # 2.0 + 1.0
    
    def test_no_late_senders(self):
        """Test trace with no late senders"""
        trace = Trace()
        
        # All sends arrive before receives are ready
        for i in range(5):
            send = MpiSendEvent(EventType.SEND, i*2, f"send{i}", 0, 0, float(i), 0, 1, i*100, dest_pid=1)
            recv = MpiRecvEvent(EventType.RECV, i*2+1, f"recv{i}", 1, 0, float(i+1), 1, 1, i*100, src_pid=0)
            send.setRecvEvent(recv)
            recv.setSendEvent(send)
            trace.addEvent(send)
            trace.addEvent(recv)
        
        analyzer = LateSender(trace)
        analyzer.forwardReplay()
        
        # Should find no late senders
        assert len(analyzer.getLateSends()) == 0
        assert analyzer.getTotalWaitTime() == 0.0
    
    def test_late_sender_with_multiple_processes(self):
        """Test late sender detection across multiple process pairs"""
        trace = Trace()
        
        # Process 0 -> Process 1: late
        send_01 = MpiSendEvent(EventType.SEND, 1, "send_01", 0, 0, 5.0, 0, 1, 100, dest_pid=1)
        recv_01 = MpiRecvEvent(EventType.RECV, 2, "recv_01", 1, 0, 3.0, 1, 1, 100, src_pid=0)
        send_01.setRecvEvent(recv_01)
        recv_01.setSendEvent(send_01)
        
        # Process 1 -> Process 2: on time
        send_12 = MpiSendEvent(EventType.SEND, 3, "send_12", 1, 0, 6.0, 1, 1, 200, dest_pid=2)
        recv_12 = MpiRecvEvent(EventType.RECV, 4, "recv_12", 2, 0, 7.0, 2, 1, 200, src_pid=1)
        send_12.setRecvEvent(recv_12)
        recv_12.setSendEvent(send_12)
        
        # Process 2 -> Process 0: late
        send_20 = MpiSendEvent(EventType.SEND, 5, "send_20", 2, 0, 12.0, 2, 1, 300, dest_pid=0)
        recv_20 = MpiRecvEvent(EventType.RECV, 6, "recv_20", 0, 0, 10.0, 0, 1, 300, src_pid=2)
        send_20.setRecvEvent(recv_20)
        recv_20.setSendEvent(send_20)
        
        for event in [send_01, recv_01, send_12, recv_12, send_20, recv_20]:
            trace.addEvent(event)
        
        analyzer = LateSender(trace)
        analyzer.forwardReplay()
        
        # Should detect 2 late senders
        late_sends = analyzer.getLateSends()
        assert len(late_sends) == 2
        assert send_01 in late_sends
        assert send_20 in late_sends
        
        # Total wait time = 2.0 + 2.0 = 4.0
        assert analyzer.getTotalWaitTime() == 4.0
