"""
GPU Late Receiver Analyzer - GPU-accelerated late receiver detection.

This module provides GPU-accelerated detection of late receiver patterns
in MPI communication traces using callback registration.
"""

from typing import List, Dict, Optional
import numpy as np

from ....perf_data_struct.dynamic.trace.trace import Trace
from ....perf_data_struct.dynamic.trace.event import Event, EventType
from ....perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from .gpu_trace_replayer import GPUTraceReplayer, GPUAvailable, DataDependence
from ..low_level.trace_replayer import ReplayDirection


class GPULateReceiver(GPUTraceReplayer):
    """
    GPU-accelerated late receiver analyzer.
    
    This class extends GPUTraceReplayer and registers a callback function
    to detect late receiver patterns using GPU parallel processing. It identifies
    receive operations that arrive late relative to their corresponding send
    operations.
    
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
        
        if self.use_gpu:
            # Register GPU callback for late receiver detection
            # Data dependence: NO_DEPS because each send event can be checked independently
            self.registerGPUCallback(
                "late_receiver_detector",
                self._late_receiver_callback,
                DataDependence.NO_DEPS
            )
        else:
            # Register CPU callback as fallback
            self.registerCallback("late_receiver_detector", 
                                 self._late_receiver_callback, 
                                 ReplayDirection.FWD)
    
    def _late_receiver_callback(self, event: Event) -> None:
        """
        Callback function for late receiver detection.
        
        This callback is invoked for each event during trace replay.
        It checks if the current event is a send event with a late receiver.
        
        Args:
            event: The current event being processed during replay
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
    
    def _analyze_cpu(self) -> None:
        """
        Perform late receiver analysis on CPU (fallback).
        """
        if self.m_trace is None:
            return
        
        # Clear and replay
        events = self.m_trace.getEvents()
        for event in events:
            self._late_receiver_callback(event)
    
    def forwardReplay(self) -> None:
        """
        Execute late receiver analysis.
        
        Replays the trace in forward order, invoking the registered callback
        for each event. Uses GPU acceleration if available, otherwise falls
        back to CPU.
        """
        # Clear previous results
        self.clear()
        
        if self.use_gpu:
            # Execute GPU callbacks through parent class
            super().forwardReplay()
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
