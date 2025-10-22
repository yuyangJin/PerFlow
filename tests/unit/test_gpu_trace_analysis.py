"""
Unit tests for GPU trace analysis components.

Tests cover GPU data structure conversion, GPU late sender analysis,
and GPU late receiver analysis with CPU fallback.
"""
import pytest
from perflow.task.trace_analysis.gpu import (
    GPUTraceReplayer, GPULateSender, GPULateReceiver, GPUAvailable, DataDependence
)
from perflow.task.trace_analysis.gpu.gpu_trace_replayer import GPUEventData
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent


class TestGPUEventData:
    """Test GPU event data structure conversion"""
    
    def test_empty_trace_conversion(self):
        """Test converting empty trace to GPU format"""
        trace = Trace()
        gpu_data = GPUEventData(trace)
        
        assert gpu_data.get_event_count() == 0
        assert gpu_data.num_events == 0
    
    def test_basic_event_conversion(self):
        """Test converting basic events to GPU format"""
        trace = Trace()
        
        # Add some basic events
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.COMPUTE, 2, "compute", 0, 0, 2.0, 0)
        event3 = Event(EventType.LEAVE, 3, "func1", 0, 0, 3.0, 0)
        
        trace.addEvent(event1)
        trace.addEvent(event2)
        trace.addEvent(event3)
        
        # Convert to GPU format
        gpu_data = GPUEventData(trace)
        
        assert gpu_data.num_events == 3
        assert gpu_data.types[0] == EventType.ENTER.value
        assert gpu_data.types[1] == EventType.COMPUTE.value
        assert gpu_data.types[2] == EventType.LEAVE.value
        assert gpu_data.timestamps[0] == 1.0
        assert gpu_data.timestamps[1] == 2.0
        assert gpu_data.timestamps[2] == 3.0
    
    def test_mpi_event_conversion(self):
        """Test converting MPI events to GPU format"""
        trace = Trace()
        
        # Create MPI send/recv pair
        send = MpiSendEvent(1, "send", 0, 0, 2.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(2, "recv", 1, 0, 1.0, 1, 1, 100, src_pid=0)
        
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(send)
        trace.addEvent(recv)
        
        # Convert to GPU format
        gpu_data = GPUEventData(trace)
        
        assert gpu_data.num_events == 2
        assert gpu_data.types[0] == EventType.SEND.value
        assert gpu_data.types[1] == EventType.RECV.value
        assert gpu_data.tags[0] == 100
        assert gpu_data.tags[1] == 100
        assert gpu_data.partner_pids[0] == 1  # send dest
        assert gpu_data.partner_pids[1] == 0  # recv src
        assert gpu_data.partner_indices[0] == 1  # send -> recv index
        assert gpu_data.partner_indices[1] == 0  # recv -> send index


class TestGPUTraceReplayer:
    """Test GPU trace replayer base class"""
    
    def test_creation(self):
        """Test creating GPU trace replayer"""
        replayer = GPUTraceReplayer()
        assert replayer.getTrace() is None
    
    def test_creation_with_trace(self):
        """Test creating GPU trace replayer with trace"""
        trace = Trace()
        replayer = GPUTraceReplayer(trace)
        assert replayer.getTrace() == trace
    
    def test_gpu_availability_check(self):
        """Test GPU availability detection"""
        replayer = GPUTraceReplayer()
        # GPU may or may not be available - just check the property exists
        gpu_enabled = replayer.isGPUEnabled()
        assert isinstance(gpu_enabled, bool)
    
    def test_set_trace_converts_to_gpu_format(self):
        """Test that setting trace converts to GPU format"""
        trace = Trace()
        event = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        trace.addEvent(event)
        
        replayer = GPUTraceReplayer()
        replayer.setTrace(trace)
        
        if replayer.isGPUEnabled():
            gpu_data = replayer.getGPUData()
            assert gpu_data is not None
            assert gpu_data.num_events == 1
    
    def test_shared_memory_toggle(self):
        """Test enabling/disabling shared memory optimization"""
        replayer = GPUTraceReplayer()
        
        replayer.enableSharedMemory(True)
        assert replayer.use_shared_mem is True
        
        replayer.enableSharedMemory(False)
        assert replayer.use_shared_mem is False
    
    def test_register_gpu_callback(self):
        """Test registering GPU callback"""
        replayer = GPUTraceReplayer()
        
        def test_callback(types, timestamps, partner_indices):
            return {"test": "result"}
        
        replayer.registerGPUCallback("test", test_callback, DataDependence.NO_DEPS)
        
        assert "test" in replayer.gpu_callbacks
        assert replayer.callback_data_deps["test"] == DataDependence.NO_DEPS
    
    def test_unregister_gpu_callback(self):
        """Test unregistering GPU callback"""
        replayer = GPUTraceReplayer()
        
        def test_callback(types, timestamps, partner_indices):
            return {"test": "result"}
        
        replayer.registerGPUCallback("test", test_callback, DataDependence.NO_DEPS)
        assert "test" in replayer.gpu_callbacks
        
        replayer.unregisterGPUCallback("test")
        assert "test" not in replayer.gpu_callbacks
    
    def test_data_dependence_types(self):
        """Test all data dependence types"""
        replayer = GPUTraceReplayer()
        
        def callback1(types, timestamps, partner_indices):
            return {}
        
        # Test all dependence types
        replayer.registerGPUCallback("no_deps", callback1, DataDependence.NO_DEPS)
        replayer.registerGPUCallback("intra_proc", callback1, DataDependence.INTRA_PROCS_DEPS)
        replayer.registerGPUCallback("inter_proc", callback1, DataDependence.INTER_PROCS_DEPS)
        replayer.registerGPUCallback("full_deps", callback1, DataDependence.FULL_DEPS)
        
        assert replayer.callback_data_deps["no_deps"] == DataDependence.NO_DEPS
        assert replayer.callback_data_deps["intra_proc"] == DataDependence.INTRA_PROCS_DEPS
        assert replayer.callback_data_deps["inter_proc"] == DataDependence.INTER_PROCS_DEPS
        assert replayer.callback_data_deps["full_deps"] == DataDependence.FULL_DEPS


class TestGPULateSender:
    """Test GPU late sender analysis"""
    
    def test_creation(self):
        """Test creating GPU late sender analyzer"""
        analyzer = GPULateSender()
        assert analyzer.getTrace() is None
        assert len(analyzer.getLateSends()) == 0
        assert len(analyzer.getWaitTimes()) == 0
    
    def test_no_late_sends(self):
        """Test when there are no late sends"""
        trace = Trace()
        
        # Send happens before receive (not late)
        send = MpiSendEvent(1, "send", 0, 0, 1.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(2, "recv", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(send)
        trace.addEvent(recv)
        
        analyzer = GPULateSender(trace)
        analyzer.forwardReplay()
        
        assert len(analyzer.getLateSends()) == 0
        assert analyzer.getTotalWaitTime() == 0.0
    
    def test_with_late_send(self):
        """Test detecting late send"""
        trace = Trace()
        
        # Receive is ready at 1.0, but send doesn't arrive until 2.0 (late!)
        send = MpiSendEvent(1, "send", 0, 0, 2.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(2, "recv", 1, 0, 1.0, 1, 1, 100, src_pid=0)
        
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(recv)
        trace.addEvent(send)
        
        analyzer = GPULateSender(trace)
        analyzer.forwardReplay()
        
        # Should detect one late send
        late_sends = analyzer.getLateSends()
        assert len(late_sends) == 1
        assert late_sends[0] == send
        
        # Wait time should be 1.0 (2.0 - 1.0)
        assert analyzer.getTotalWaitTime() == 1.0
    
    def test_multiple_late_sends(self):
        """Test detecting multiple late sends"""
        trace = Trace()
        
        # Late send 1: wait time = 1.0
        send1 = MpiSendEvent(1, "send1", 0, 0, 3.0, 0, 1, 100, dest_pid=1)
        recv1 = MpiRecvEvent(2, "recv1", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        send1.setRecvEvent(recv1)
        recv1.setSendEvent(send1)
        
        # Late send 2: wait time = 2.0
        send2 = MpiSendEvent(3, "send2", 0, 0, 7.0, 0, 1, 200, dest_pid=1)
        recv2 = MpiRecvEvent(4, "recv2", 1, 0, 5.0, 1, 1, 200, src_pid=0)
        send2.setRecvEvent(recv2)
        recv2.setSendEvent(send2)
        
        trace.addEvent(recv1)
        trace.addEvent(send1)
        trace.addEvent(recv2)
        trace.addEvent(send2)
        
        analyzer = GPULateSender(trace)
        analyzer.forwardReplay()
        
        # Should detect two late sends
        assert len(analyzer.getLateSends()) == 2
        
        # Total wait time should be 3.0 (1.0 + 2.0)
        assert analyzer.getTotalWaitTime() == 3.0
    
    def test_clear(self):
        """Test clearing late sender results"""
        trace = Trace()
        
        send = MpiSendEvent(1, "send", 0, 0, 2.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(2, "recv", 1, 0, 1.0, 1, 1, 100, src_pid=0)
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(recv)
        trace.addEvent(send)
        
        analyzer = GPULateSender(trace)
        analyzer.forwardReplay()
        
        assert len(analyzer.getLateSends()) > 0
        
        analyzer.clear()
        
        assert len(analyzer.getLateSends()) == 0
        assert len(analyzer.getWaitTimes()) == 0
        assert analyzer.getTotalWaitTime() == 0.0


class TestGPULateReceiver:
    """Test GPU late receiver analysis"""
    
    def test_creation(self):
        """Test creating GPU late receiver analyzer"""
        analyzer = GPULateReceiver()
        assert analyzer.getTrace() is None
        assert len(analyzer.getLateRecvs()) == 0
        assert len(analyzer.getWaitTimes()) == 0
    
    def test_no_late_recvs(self):
        """Test when there are no late receives"""
        trace = Trace()
        
        # Receive happens before send (not late)
        send = MpiSendEvent(1, "send", 0, 0, 2.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(2, "recv", 1, 0, 1.0, 1, 1, 100, src_pid=0)
        
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(send)
        trace.addEvent(recv)
        
        analyzer = GPULateReceiver(trace)
        analyzer.forwardReplay()
        
        assert len(analyzer.getLateRecvs()) == 0
        assert analyzer.getTotalWaitTime() == 0.0
    
    def test_with_late_recv(self):
        """Test detecting late receive"""
        trace = Trace()
        
        # Send is ready at 1.0, but receive doesn't happen until 2.0 (late!)
        send = MpiSendEvent(1, "send", 0, 0, 1.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(2, "recv", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(send)
        trace.addEvent(recv)
        
        analyzer = GPULateReceiver(trace)
        analyzer.forwardReplay()
        
        # Should detect one late receive
        late_recvs = analyzer.getLateRecvs()
        assert len(late_recvs) == 1
        assert late_recvs[0] == recv
        
        # Wait time should be 1.0 (2.0 - 1.0)
        assert analyzer.getTotalWaitTime() == 1.0
    
    def test_multiple_late_recvs(self):
        """Test detecting multiple late receives"""
        trace = Trace()
        
        # Late recv 1: wait time = 1.0
        send1 = MpiSendEvent(1, "send1", 0, 0, 2.0, 0, 1, 100, dest_pid=1)
        recv1 = MpiRecvEvent(2, "recv1", 1, 0, 3.0, 1, 1, 100, src_pid=0)
        send1.setRecvEvent(recv1)
        recv1.setSendEvent(send1)
        
        # Late recv 2: wait time = 2.0
        send2 = MpiSendEvent(3, "send2", 0, 0, 5.0, 0, 1, 200, dest_pid=1)
        recv2 = MpiRecvEvent(4, "recv2", 1, 0, 7.0, 1, 1, 200, src_pid=0)
        send2.setRecvEvent(recv2)
        recv2.setSendEvent(send2)
        
        trace.addEvent(send1)
        trace.addEvent(recv1)
        trace.addEvent(send2)
        trace.addEvent(recv2)
        
        analyzer = GPULateReceiver(trace)
        analyzer.forwardReplay()
        
        # Should detect two late receives
        assert len(analyzer.getLateRecvs()) == 2
        
        # Total wait time should be 3.0 (1.0 + 2.0)
        assert analyzer.getTotalWaitTime() == 3.0
    
    def test_clear(self):
        """Test clearing late receiver results"""
        trace = Trace()
        
        send = MpiSendEvent(1, "send", 0, 0, 1.0, 0, 1, 100, dest_pid=1)
        recv = MpiRecvEvent(2, "recv", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
        
        trace.addEvent(send)
        trace.addEvent(recv)
        
        analyzer = GPULateReceiver(trace)
        analyzer.forwardReplay()
        
        assert len(analyzer.getLateRecvs()) > 0
        
        analyzer.clear()
        
        assert len(analyzer.getLateRecvs()) == 0
        assert len(analyzer.getWaitTimes()) == 0
        assert analyzer.getTotalWaitTime() == 0.0


class TestGPUCPUConsistency:
    """Test that GPU and CPU implementations produce consistent results"""
    
    def test_late_sender_consistency(self):
        """Test GPU and CPU late sender produce same results"""
        from perflow.task.trace_analysis.late_sender import LateSender
        
        trace = Trace()
        
        # Create test scenario
        send1 = MpiSendEvent(1, "send1", 0, 0, 3.0, 0, 1, 100, dest_pid=1)
        recv1 = MpiRecvEvent(2, "recv1", 1, 0, 2.0, 1, 1, 100, src_pid=0)
        send1.setRecvEvent(recv1)
        recv1.setSendEvent(send1)
        
        send2 = MpiSendEvent(3, "send2", 0, 0, 7.0, 0, 1, 200, dest_pid=1)
        recv2 = MpiRecvEvent(4, "recv2", 1, 0, 8.0, 1, 1, 200, src_pid=0)
        send2.setRecvEvent(recv2)
        recv2.setSendEvent(send2)
        
        trace.addEvent(recv1)
        trace.addEvent(send1)
        trace.addEvent(recv2)
        trace.addEvent(send2)
        
        # CPU analysis
        cpu_analyzer = LateSender(trace)
        cpu_analyzer.forwardReplay()
        cpu_count = len(cpu_analyzer.getLateSends())
        cpu_total_wait = cpu_analyzer.getTotalWaitTime()
        
        # GPU analysis (will use CPU fallback if GPU not available)
        gpu_analyzer = GPULateSender(trace)
        gpu_analyzer.forwardReplay()
        gpu_count = len(gpu_analyzer.getLateSends())
        gpu_total_wait = gpu_analyzer.getTotalWaitTime()
        
        # Results should be identical
        assert cpu_count == gpu_count
        assert abs(cpu_total_wait - gpu_total_wait) < 1e-9
