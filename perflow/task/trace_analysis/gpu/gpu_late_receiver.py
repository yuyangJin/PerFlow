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
                self._gpu_late_receiver_callback,
                DataDependence.NO_DEPS
            )
        else:
            # Register CPU callback as fallback
            self.registerCallback("late_receiver_detector", 
                                 self._detect_late_receiver_cpu, 
                                 ReplayDirection.FWD)
    
    def _gpu_late_receiver_callback(
        self, 
        types: np.ndarray, 
        timestamps: np.ndarray, 
        partner_indices: np.ndarray
    ) -> Dict[str, any]:
        """
        GPU callback function for late receiver detection.
        
        This callback processes event arrays on the GPU to identify late receivers.
        Each send event is checked independently to see if its corresponding
        receive was late.
        
        Args:
            types: Array of event types
            timestamps: Array of event timestamps
            partner_indices: Array mapping events to their partners
            
        Returns:
            Dictionary containing late receiver indices and wait times
        """
        late_recv_indices = []
        wait_times = {}
        
        # Process all events in parallel (simulated for CPU fallback)
        for i in range(len(types)):
            # Check if this is a send event
            if types[i] == EventType.SEND.value:
                recv_idx = partner_indices[i]
                
                # Check if receive event is matched
                if recv_idx >= 0 and recv_idx < len(types):
                    send_time = timestamps[i]
                    recv_time = timestamps[recv_idx]
                    
                    # Late receiver: receive happens after send is ready
                    if recv_time > send_time:
                        wait_time = recv_time - send_time
                        late_recv_indices.append(recv_idx)
                        wait_times[recv_idx] = wait_time
        
        return {
            'late_recv_indices': late_recv_indices,
            'wait_times': wait_times
        }
    
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
    
    def _process_gpu_results(self, results: Dict[str, any]) -> None:
        """
        Process results from GPU callback and populate analysis results.
        
        Args:
            results: Dictionary containing GPU callback results
        """
        if 'late_receiver_detector' not in results:
            return
        
        callback_results = results['late_receiver_detector']
        late_recv_indices = callback_results.get('late_recv_indices', [])
        wait_times_dict = callback_results.get('wait_times', {})
        
        # Convert indices back to event objects
        if self.m_trace is not None:
            events = self.m_trace.getEvents()
            for idx in late_recv_indices:
                if 0 <= idx < len(events):
                    event = events[idx]
                    if isinstance(event, MpiRecvEvent):
                        self.m_late_recvs.append(event)
            
            # Store wait times
            self.m_wait_times = wait_times_dict
    
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
            # Execute GPU callbacks
            results = self._execute_gpu_callbacks(ReplayDirection.FWD)
            self._process_gpu_results(results)
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
