"""
Integration tests for MPI-based trace analysis
"""
import pytest
from perflow.task.trace_analysis.low_level.mpi_trace_replayer import MPITraceReplayer
from perflow.task.trace_analysis.low_level.trace_distributor import TraceDistributor
from perflow.task.trace_analysis.low_level.event_data_fetcher import EventDataFetcher
from perflow.task.trace_analysis.low_level.trace_replayer import ReplayDirection
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent


class TestMPITraceAnalysisIntegration:
    """Integration tests for MPI-based trace analysis"""
    
    def test_full_workflow_without_mpi(self):
        """Test complete workflow without MPI (sequential mode)"""
        # Create traces for 4 execution processes
        traces = []
        for ep_id in range(4):
            trace = Trace()
            trace_info = TraceInfo(pid=ep_id)
            trace.setTraceInfo(trace_info)
            
            # Add some events
            event1 = Event(EventType.ENTER, ep_id * 10 + 1, f"func{ep_id}", ep_id, 0, 1.0 + ep_id, 0)
            event2 = Event(EventType.LEAVE, ep_id * 10 + 2, f"func{ep_id}", ep_id, 0, 2.0 + ep_id, 0)
            trace.addEvent(event1)
            trace.addEvent(event2)
            
            traces.append(trace)
        
        # Create replayer (MPI disabled)
        replayer = MPITraceReplayer()
        
        # Distribute traces
        distributed_traces = replayer.distribute_traces(
            traces, 
            num_execution_processes=4,
            num_replay_processes=2
        )
        
        # Without MPI, should get all traces
        assert len(distributed_traces) == 4
        
        # Register callback to count replayed events
        event_count = [0]
        def count_events(event):
            event_count[0] += 1
        
        replayer.registerCallback("counter", count_events, ReplayDirection.FWD)
        
        # Replay
        replayer.forwardReplay()
        
        # Should have replayed 8 events (2 per trace, 4 traces)
        assert event_count[0] == 8
    
    def test_trace_distribution_and_metadata(self):
        """Test trace distribution updates metadata correctly"""
        # Create traces
        traces = []
        for ep_id in range(8):
            trace = Trace()
            trace_info = TraceInfo(pid=ep_id)
            trace.setTraceInfo(trace_info)
            traces.append(trace)
        
        # Create distributor
        distributor = TraceDistributor(num_execution_processes=8, num_replay_processes=4)
        mapping = distributor.compute_distribution()
        
        # Update trace info
        distributor.update_trace_info(traces)
        
        # Verify metadata was updated
        for trace in traces:
            trace_info = trace.getTraceInfo()
            assert trace_info.getNumExecutionProcesses() == 8
            assert trace_info.getNumReplayProcesses() == 4
            assert trace_info.getEpToRpMapping() == mapping
    
    def test_event_data_fetcher_workflow(self):
        """Test event data fetcher in a complete workflow"""
        # Create traces with events
        traces = []
        for ep_id in range(3):
            trace = Trace()
            trace_info = TraceInfo(pid=ep_id)
            trace.setTraceInfo(trace_info)
            
            # Add unique event for each EP
            event = Event(EventType.ENTER, ep_id + 1, f"func{ep_id}", ep_id, 0, 1.0 + ep_id, 0)
            trace.addEvent(event)
            
            traces.append(trace)
        
        # Create mapping
        ep_to_rp_mapping = {0: 0, 1: 0, 2: 1}
        
        # Create fetcher and register traces
        fetcher = EventDataFetcher()
        fetcher.register_traces(traces, ep_to_rp_mapping)
        
        # Fetch local events
        event_data_0 = fetcher.fetch_event_data(0, 1)
        event_data_1 = fetcher.fetch_event_data(1, 2)
        event_data_2 = fetcher.fetch_event_data(2, 3)
        
        # All should be found (local)
        assert event_data_0 is not None
        assert event_data_0['name'] == "func0"
        
        assert event_data_1 is not None
        assert event_data_1['name'] == "func1"
        
        assert event_data_2 is not None
        assert event_data_2['name'] == "func2"
        
        # Verify caching
        assert fetcher.get_cache_size() == 3
    
    def test_mpi_replayer_with_callbacks(self):
        """Test MPI replayer with analysis callbacks"""
        # Create traces
        traces = []
        for ep_id in range(4):
            trace = Trace()
            trace_info = TraceInfo(pid=ep_id)
            trace.setTraceInfo(trace_info)
            
            # Add events with different types
            event1 = Event(EventType.ENTER, ep_id * 10 + 1, f"func{ep_id}", ep_id, 0, 1.0, 0)
            event2 = Event(EventType.COMPUTE, ep_id * 10 + 2, "compute", ep_id, 0, 2.0, 0)
            event3 = Event(EventType.LEAVE, ep_id * 10 + 3, f"func{ep_id}", ep_id, 0, 3.0, 0)
            
            trace.addEvent(event1)
            trace.addEvent(event2)
            trace.addEvent(event3)
            
            traces.append(trace)
        
        # Create replayer
        replayer = MPITraceReplayer()
        replayer.set_traces(traces)
        
        # Manually set MPI mode for testing
        replayer.m_mpi_enabled = True
        
        # Register different callbacks
        enter_count = [0]
        compute_count = [0]
        leave_count = [0]
        
        def count_enters(event):
            if event.getType() == EventType.ENTER:
                enter_count[0] += 1
        
        def count_computes(event):
            if event.getType() == EventType.COMPUTE:
                compute_count[0] += 1
        
        def count_leaves(event):
            if event.getType() == EventType.LEAVE:
                leave_count[0] += 1
        
        replayer.registerCallback("enter_counter", count_enters, ReplayDirection.FWD)
        replayer.registerCallback("compute_counter", count_computes, ReplayDirection.FWD)
        replayer.registerCallback("leave_counter", count_leaves, ReplayDirection.FWD)
        
        # Replay
        replayer.forwardReplay()
        
        # Verify counts
        assert enter_count[0] == 4
        assert compute_count[0] == 4
        assert leave_count[0] == 4
    
    def test_late_sender_detection_workflow(self):
        """Test late sender detection in distributed environment"""
        # Create traces for 2 processes with late sender scenario
        traces = []
        
        # Process 0 trace
        trace0 = Trace()
        trace_info0 = TraceInfo(pid=0)
        trace0.setTraceInfo(trace_info0)
        
        # Late sender: send at t=5.0
        send1 = MpiSendEvent(
            idx=1,
            name="MPI_Send",
            pid=0,
            tid=0,
            timestamp=5.0,
            replay_pid=0,
            communicator=1,
            tag=100,
            dest_pid=1
        )
        trace0.addEvent(send1)
        traces.append(trace0)
        
        # Process 1 trace
        trace1 = Trace()
        trace_info1 = TraceInfo(pid=1)
        trace1.setTraceInfo(trace_info1)
        
        # Receive ready at t=2.0 (late sender scenario)
        recv1 = MpiRecvEvent(
            idx=2,
            name="MPI_Recv",
            pid=1,
            tid=0,
            timestamp=2.0,
            replay_pid=1,
            communicator=1,
            tag=100,
            src_pid=0
        )
        trace1.addEvent(recv1)
        traces.append(trace1)
        
        # Match events
        send1.setRecvEvent(recv1)
        recv1.setSendEvent(send1)
        
        # Create replayer
        replayer = MPITraceReplayer()
        replayer.set_traces(traces)
        
        # Manually enable MPI mode
        replayer.m_mpi_enabled = True
        
        # Detect late senders
        late_senders = []
        
        def detect_late_sender(event):
            if isinstance(event, MpiRecvEvent):
                send_event = event.getSendEvent()
                if send_event and isinstance(send_event, MpiSendEvent):
                    recv_time = event.getTimestamp()
                    send_time = send_event.getTimestamp()
                    
                    if send_time and recv_time and send_time > recv_time:
                        late_senders.append(send_event)
        
        replayer.registerCallback("late_sender_detector", detect_late_sender, ReplayDirection.FWD)
        
        # Replay
        replayer.forwardReplay()
        
        # Should detect 1 late sender
        assert len(late_senders) == 1
        assert late_senders[0].getIdx() == 1
    
    def test_load_balancing(self):
        """Test that load balancing works correctly"""
        distributor = TraceDistributor(num_execution_processes=10, num_replay_processes=3)
        distributor.compute_distribution()
        
        load_info = distributor.get_load_balance_info()
        
        # Total EPs should match
        total_eps = sum(load_info.values())
        assert total_eps == 10
        
        # Load should be relatively balanced
        # RP 0 gets 4 EPs (0, 3, 6, 9)
        # RP 1 gets 3 EPs (1, 4, 7)
        # RP 2 gets 3 EPs (2, 5, 8)
        assert load_info[0] == 4
        assert load_info[1] == 3
        assert load_info[2] == 3
    
    def test_backward_replay_workflow(self):
        """Test backward replay in MPI context"""
        # Create traces
        traces = []
        for ep_id in range(2):
            trace = Trace()
            trace_info = TraceInfo(pid=ep_id)
            trace.setTraceInfo(trace_info)
            
            # Add events with increasing timestamps
            for i in range(3):
                event = Event(
                    EventType.COMPUTE, 
                    ep_id * 10 + i, 
                    f"event{i}", 
                    ep_id, 
                    0, 
                    float(i), 
                    0
                )
                trace.addEvent(event)
            
            traces.append(trace)
        
        # Create replayer
        replayer = MPITraceReplayer()
        replayer.set_traces(traces)
        replayer.m_mpi_enabled = True
        
        # Track replay order
        replay_order = []
        
        def record_order(event):
            replay_order.append((event.getPid(), event.getIdx()))
        
        replayer.registerCallback("order_recorder", record_order, ReplayDirection.BWD)
        
        # Backward replay
        replayer.backwardReplay()
        
        # Should replay 6 events total (3 per trace, 2 traces)
        assert len(replay_order) == 6
        
        # For each trace, events should be in reverse order
        # Trace 0: events 2, 1, 0
        # Trace 1: events 12, 11, 10
        trace0_events = [idx for pid, idx in replay_order if pid == 0]
        trace1_events = [idx for pid, idx in replay_order if pid == 1]
        
        # Events within each trace should be descending
        assert trace0_events == [2, 1, 0]
        assert trace1_events == [12, 11, 10]
