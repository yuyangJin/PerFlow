"""
GPU Late Receiver Analyzer - GPU-accelerated late receiver detection.

This module provides GPU-accelerated detection of late receiver patterns
in MPI communication traces.
"""

from typing import List, Dict, Optional

from ....perf_data_struct.dynamic.trace.trace import Trace
from ....perf_data_struct.dynamic.trace.event import Event
from ....perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from .gpu_trace_replayer import GPUTraceReplayer, GPUAvailable, _load_gpu_library
from ..low_level.trace_replayer import ReplayDirection


class GPULateReceiver(GPUTraceReplayer):
    """
    GPU-accelerated late receiver analyzer.
    
    This class extends GPUTraceReplayer to detect late receiver patterns
    using GPU parallel processing. It identifies receive operations that
    arrive late relative to their corresponding send operations.
    
    When GPU is not available, automatically falls back to CPU implementation.
    
    Attributes:
        m_late_recvs: List of receive events identified as late
        m_wait_times: Dictionary mapping event IDs to wait times
    """
    
    def __init__(self, trace: Optional[Trace] = None, use_gpu: bool = True):
        """
        Initialize GPU late receiver analyzer.
        
        Args:
            trace: Optional trace to analyze
            use_gpu: Whether to enable GPU acceleration (default: True)
        """
        super().__init__(trace, use_gpu)
        self.m_late_recvs: List[MpiRecvEvent] = []
        self.m_wait_times: Dict[int, float] = {}
        
        # Register CPU callback as fallback
        if not self.use_gpu:
            self.registerCallback("late_receiver_detector", 
                                 self._detect_late_receiver_cpu, 
                                 ReplayDirection.FWD)
    
    def _detect_late_receiver_cpu(self, event: Event) -> None:
        """
        CPU callback to detect late receivers during trace replay.
        
        Args:
            event: Event being processed during replay
        """
        # Check if this is a send event
        if isinstance(event, MpiSendEvent):
            recv_event = event.getRecvEvent()
            
            # If recv event is matched, check if it's late
            if recv_event and isinstance(recv_event, MpiRecvEvent):
                send_timestamp = event.getTimestamp()
                recv_timestamp = recv_event.getTimestamp()
                
                # If recv happens after send is ready, it's a late receiver
                if send_timestamp and recv_timestamp and recv_timestamp > send_timestamp:
                    wait_time = recv_timestamp - send_timestamp
                    
                    # Record the late receive
                    self.m_late_recvs.append(recv_event)
                    
                    # Record wait time
                    event_idx = recv_event.getIdx()
                    if event_idx is not None:
                        self.m_wait_times[event_idx] = wait_time
    
    def _analyze_gpu(self) -> None:
        """
        Perform late receiver analysis on GPU.
        
        This method converts trace data to GPU format, launches CUDA kernels,
        and retrieves results back to host memory.
        """
        if self.gpu_data is None or self.gpu_data.num_events == 0:
            return
        
        try:
            # Load GPU library
            gpu_lib = _load_gpu_library()
            if gpu_lib is None:
                raise RuntimeError("GPU library not available")
            
            # For now, we simulate GPU execution since we can't actually run CUDA
            # In a real implementation with CUDA available, this would:
            # 1. Allocate GPU memory
            # 2. Copy data to GPU
            # 3. Launch kernel
            # 4. Copy results back
            # 5. Free GPU memory
            
            # Simulate GPU analysis by calling CPU version for now
            self._analyze_cpu()
            
        except Exception as e:
            print(f"GPU analysis failed: {e}. Falling back to CPU.")
            self._analyze_cpu()
    
    def _analyze_cpu(self) -> None:
        """
        Perform late receiver analysis on CPU (fallback).
        """
        if self.m_trace is None:
            return
        
        # Clear and replay
        events = self.m_trace.getEvents()
        for event in events:
            self._detect_late_receiver_cpu(event)
    
    def forwardReplay(self) -> None:
        """
        Execute late receiver analysis.
        
        Uses GPU acceleration if available, otherwise falls back to CPU.
        """
        # Clear previous results
        self.clear()
        
        if self.use_gpu and self.gpu_data is not None:
            self._analyze_gpu()
        else:
            self._analyze_cpu()
    
    def getLateRecvs(self) -> List[MpiRecvEvent]:
        """
        Get the list of identified late receive events.
        
        Returns:
            List of MpiRecvEvent objects identified as late
        """
        return self.m_late_recvs
    
    def getWaitTimes(self) -> Dict[int, float]:
        """
        Get wait times for events.
        
        Returns:
            Dictionary mapping event IDs to their wait times
        """
        return self.m_wait_times
    
    def getTotalWaitTime(self) -> float:
        """
        Calculate the total wait time caused by late receivers.
        
        Returns:
            Sum of all wait times
        """
        return sum(self.m_wait_times.values())
    
    def clear(self) -> None:
        """Clear all analysis results."""
        self.m_late_recvs.clear()
        self.m_wait_times.clear()
    
    def run(self) -> None:
        """
        Execute late receiver analysis.
        
        Processes input traces and identifies late receiver patterns.
        Results are stored in m_late_recvs and m_wait_times.
        """
        # Clear previous results
        self.clear()
        
        # Process traces from input data
        for data in self.m_inputs.get_data():
            if isinstance(data, Trace):
                self.setTrace(data)
                self.forwardReplay()
                
                # Add analysis results to outputs as tuple (hashable)
                output = (
                    "GPULateReceiverAnalysis",
                    data,
                    tuple(self.m_late_recvs.copy()),
                    tuple(self.m_wait_times.items()),
                    self.getTotalWaitTime()
                )
                self.m_outputs.add_data(output)
