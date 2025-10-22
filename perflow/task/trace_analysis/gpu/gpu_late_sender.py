"""
GPU Late Sender Analyzer - GPU-accelerated late sender detection.

This module provides GPU-accelerated detection of late sender patterns
in MPI communication traces using callback registration.
"""

from typing import List, Dict, Optional
import numpy as np

from ....perf_data_struct.dynamic.trace.trace import Trace
from ....perf_data_struct.dynamic.trace.event import Event, EventType
from ....perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from .gpu_trace_replayer import GPUTraceReplayer, GPUAvailable, DataDependence
from ..late_sender import LateSender


class GPULateSender(GPUTraceReplayer):
    """
    GPU-accelerated late sender analyzer.
    
    This class extends GPUTraceReplayer and registers a callback function
    to detect late sender patterns using GPU parallel processing. It identifies
    send operations that arrive late relative to their corresponding receive
    operations.
    
    When GPU is not available, automatically falls back to CPU implementation.
    
    Attributes:
        m_late_sends: List of send events identified as late
        m_wait_times: Dictionary mapping event IDs to wait times
        cpu_analyzer: Fallback CPU-based analyzer
    """
    
    def __init__(self, trace: Optional[Trace] = None, use_gpu: bool = True):
        """
        Initialize GPU late sender analyzer.
        
        Args:
            trace: Optional trace to analyze
            use_gpu: Whether to enable GPU acceleration (default: True)
        """
        super().__init__(trace, use_gpu)
        self.m_late_sends: List[MpiSendEvent] = []
        self.m_wait_times: Dict[int, float] = {}
        
        # Create CPU fallback analyzer
        self.cpu_analyzer: Optional[LateSender] = None
        if not self.use_gpu:
            self.cpu_analyzer = LateSender(trace)
        else:
            # Register GPU callback for late sender detection
            # Data dependence: NO_DEPS because each receive event can be checked independently
            self.registerGPUCallback(
                "late_sender_detector",
                self._late_sender_callback,
                DataDependence.NO_DEPS
            )
    
    def _late_sender_callback(self, event: Event) -> None:
        """
        Callback function for late sender detection.
        
        This callback is invoked for each event during trace replay.
        It checks if the current event is a receive event with a late sender.
        
        Args:
            event: The current event being processed during replay
        """
        # Check if this is a receive event
        if isinstance(event, MpiRecvEvent):
            send_event = event.getSendEvent()
            
            # Check if send event is matched
            if send_event and isinstance(send_event, MpiSendEvent):
                recv_time = event.getTimestamp()
                send_time = send_event.getTimestamp()
                
                # Late sender: send happens after receive is ready
                if recv_time is not None and send_time is not None and send_time > recv_time:
                    wait_time = send_time - recv_time
                    
                    # Record the late send
                    self.m_late_sends.append(send_event)
                    
                    # Record wait time
                    event_idx = event.getIdx()
                    if event_idx is not None:
                        self.m_wait_times[event_idx] = wait_time
    
    def _analyze_cpu(self) -> None:
        """
        Perform late sender analysis on CPU (fallback).
        
        Uses the CPU-based LateSender implementation.
        """
        if self.m_trace is None:
            return
        
        if self.cpu_analyzer is None:
            self.cpu_analyzer = LateSender(self.m_trace)
        else:
            self.cpu_analyzer.setTrace(self.m_trace)
        
        self.cpu_analyzer.forwardReplay()
        
        # Copy results from CPU analyzer
        self.m_late_sends = self.cpu_analyzer.getLateSends().copy()
        self.m_wait_times = self.cpu_analyzer.getWaitTimes().copy()
    
    def forwardReplay(self) -> None:
        """
        Execute late sender analysis.
        
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
    
    def getLateSends(self) -> List[MpiSendEvent]:
        """
        Get the list of identified late send events.
        
        Returns:
            List of MpiSendEvent objects identified as late
        """
        return self.m_late_sends
    
    def getWaitTimes(self) -> Dict[int, float]:
        """
        Get wait times for events.
        
        Returns:
            Dictionary mapping event IDs to their wait times
        """
        return self.m_wait_times
    
    def getTotalWaitTime(self) -> float:
        """
        Calculate the total wait time caused by late senders.
        
        Returns:
            Sum of all wait times
        """
        return sum(self.m_wait_times.values())
    
    def clear(self) -> None:
        """Clear all analysis results."""
        self.m_late_sends.clear()
        self.m_wait_times.clear()
    
    def run(self) -> None:
        """
        Execute late sender analysis.
        
        Processes input traces and identifies late sender patterns.
        Results are stored in m_late_sends and m_wait_times.
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
                    "GPULateSenderAnalysis",
                    data,
                    tuple(self.m_late_sends.copy()),
                    tuple(self.m_wait_times.items()),
                    self.getTotalWaitTime()
                )
                self.m_outputs.add_data(output)
