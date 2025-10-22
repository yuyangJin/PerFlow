"""
Unit tests for MPITraceReplayer class
"""
import pytest
from perflow.task.trace_analysis.low_level.mpi_trace_replayer import MPITraceReplayer
from perflow.task.trace_analysis.low_level.trace_replayer import TraceReplayer, ReplayDirection
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType


class TestMPITraceReplayer:
    """Test MPITraceReplayer class"""
    
    def test_mpitracereplayer_creation(self):
        """Test creating MPITraceReplayer"""
        replayer = MPITraceReplayer()
        assert replayer.getTrace() is None
        assert replayer.is_mpi_enabled() is False
        assert len(replayer.get_traces()) == 0
    
    def test_mpitracereplayer_creation_with_trace(self):
        """Test creating MPITraceReplayer with trace"""
        trace = Trace()
        replayer = MPITraceReplayer(trace=trace)
        assert replayer.getTrace() == trace
        assert replayer.is_mpi_enabled() is False
    
    def test_mpitracereplayer_inherits_from_tracereplayer(self):
        """Test that MPITraceReplayer inherits from TraceReplayer"""
        replayer = MPITraceReplayer()
        assert isinstance(replayer, TraceReplayer)
    
    def test_mpitracereplayer_disable_mpi(self):
        """Test disabling MPI"""
        replayer = MPITraceReplayer()
        replayer.disable_mpi()
        assert replayer.is_mpi_enabled() is False
    
    def test_mpitracereplayer_set_traces(self):
        """Test setting multiple traces"""
        replayer = MPITraceReplayer()
        
        traces = []
        for i in range(3):
            trace = Trace()
            trace_info = TraceInfo(pid=i)
            trace.setTraceInfo(trace_info)
            traces.append(trace)
        
        replayer.set_traces(traces)
        assert len(replayer.get_traces()) == 3
    
    def test_mpitracereplayer_distribute_traces_without_mpi(self):
        """Test distributing traces without MPI enabled"""
        replayer = MPITraceReplayer()
        
        # Create traces
        traces = []
        for i in range(4):
            trace = Trace()
            trace_info = TraceInfo(pid=i)
            trace.setTraceInfo(trace_info)
            traces.append(trace)
        
        # Distribute without MPI
        distributed = replayer.distribute_traces(traces, num_execution_processes=4)
        
        # Without MPI, should get all traces
        assert len(distributed) == 4
    
    def test_mpitracereplayer_is_root_without_mpi(self):
        """Test is_root without MPI enabled"""
        replayer = MPITraceReplayer()
        # Without MPI, should return True
        assert replayer.is_root() is True
    
    def test_mpitracereplayer_get_rank_without_mpi(self):
        """Test get_rank without MPI enabled"""
        replayer = MPITraceReplayer()
        assert replayer.get_rank() is None
    
    def test_mpitracereplayer_get_size_without_mpi(self):
        """Test get_size without MPI enabled"""
        replayer = MPITraceReplayer()
        assert replayer.get_size() is None
    
    def test_mpitracereplayer_barrier_without_mpi(self):
        """Test barrier without MPI enabled (should not raise)"""
        replayer = MPITraceReplayer()
        # Should not raise an exception
        replayer.barrier()
    
    def test_mpitracereplayer_forward_replay_single_trace(self):
        """Test forward replay with single trace (non-MPI mode)"""
        replayer = MPITraceReplayer()
        
        trace = Trace()
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.LEAVE, 2, "func1", 0, 0, 2.0, 0)
        trace.addEvent(event1)
        trace.addEvent(event2)
        
        replayer.setTrace(trace)
        
        replayed_events = []
        replayer.registerCallback("recorder", lambda e: replayed_events.append(e), ReplayDirection.FWD)
        
        replayer.forwardReplay()
        
        assert len(replayed_events) == 2
        assert replayed_events[0] == event1
        assert replayed_events[1] == event2
    
    def test_mpitracereplayer_forward_replay_multiple_traces(self):
        """Test forward replay with multiple traces (simulated MPI mode)"""
        replayer = MPITraceReplayer()
        
        # Create two traces
        traces = []
        for i in range(2):
            trace = Trace()
            trace_info = TraceInfo(pid=i)
            trace.setTraceInfo(trace_info)
            
            event = Event(EventType.ENTER, i+1, f"func{i}", i, 0, 1.0, 0)
            trace.addEvent(event)
            traces.append(trace)
        
        replayer.set_traces(traces)
        
        # Manually enable MPI mode for testing
        replayer.m_mpi_enabled = True
        
        replayed_events = []
        replayer.registerCallback("recorder", lambda e: replayed_events.append(e), ReplayDirection.FWD)
        
        replayer.forwardReplay()
        
        # Should replay events from both traces
        assert len(replayed_events) == 2
    
    def test_mpitracereplayer_backward_replay_single_trace(self):
        """Test backward replay with single trace"""
        replayer = MPITraceReplayer()
        
        trace = Trace()
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.LEAVE, 2, "func1", 0, 0, 2.0, 0)
        trace.addEvent(event1)
        trace.addEvent(event2)
        
        replayer.setTrace(trace)
        
        replayed_events = []
        replayer.registerCallback("recorder", lambda e: replayed_events.append(e), ReplayDirection.BWD)
        
        replayer.backwardReplay()
        
        assert len(replayed_events) == 2
        assert replayed_events[0] == event2  # Backward order
        assert replayed_events[1] == event1
    
    def test_mpitracereplayer_get_event_data(self):
        """Test getting event data via event fetcher"""
        replayer = MPITraceReplayer()
        
        # Create trace with event
        trace = Trace()
        trace_info = TraceInfo(pid=0)
        trace.setTraceInfo(trace_info)
        
        event = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        trace.addEvent(event)
        
        # Register traces
        replayer.m_event_fetcher.register_traces([trace], {0: 0})
        
        # Get event data
        event_data = replayer.get_event_data(0, 1)
        
        assert event_data is not None
        assert event_data['idx'] == 1
        assert event_data['name'] == "func1"
    
    def test_mpitracereplayer_get_load_balance_info(self):
        """Test getting load balance information"""
        replayer = MPITraceReplayer()
        
        # Set up distribution
        replayer.m_distributor.set_num_execution_processes(8)
        replayer.m_distributor.set_num_replay_processes(4)
        replayer.m_distributor.compute_distribution()
        
        load_info = replayer.get_load_balance_info()
        
        assert len(load_info) == 4
        # Each RP should have 2 EPs
        for rp_id in range(4):
            assert load_info[rp_id] == 2
