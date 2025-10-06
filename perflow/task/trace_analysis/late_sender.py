'''
module late sender analysis
'''

from typing import Dict, List, Optional
from .low_level.trace_replayer import TraceReplayer
from ...perf_data_struct.dynamic.trace.trace import Trace
from ...perf_data_struct.dynamic.trace.event import Event
from ...perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent

'''
@class LateSender
Late sender analysis identifies send operations that arrive late relative to
their corresponding receive operations, causing waiting time.
'''


class LateSender(TraceReplayer):
    """
    LateSender performs late sender analysis on MPI traces.
    
    This class extends TraceReplayer to identify MPI send operations that
    cause receive operations to wait. A "late sender" occurs when a receive
    operation is ready to execute but must wait for the corresponding send
    to arrive.
    
    Attributes:
        m_late_sends: List of send events identified as late
        m_wait_times: Dictionary mapping event IDs to wait times
    """
    
    def __init__(self, trace: Optional[Trace] = None) -> None:
        """
        Initialize a LateSender analyzer.
        
        Args:
            trace: Optional trace to analyze
        """
        super().__init__(trace)
        self.m_late_sends: List[MpiSendEvent] = []
        self.m_wait_times: Dict[int, float] = {}
        
        # Register callback for late sender detection
        self.registerCallback("late_sender_detector", self._detect_late_sender)
    
    def _detect_late_sender(self, event: Event) -> None:
        """
        Callback to detect late senders during trace replay.
        
        Args:
            event: Event being processed during replay
        """
        # Check if this is a receive event
        if isinstance(event, MpiRecvEvent):
            send_event = event.getSendEvent()
            
            # If send event is matched, check if it's late
            if send_event and isinstance(send_event, MpiSendEvent):
                recv_timestamp = event.getTimestamp()
                send_timestamp = send_event.getTimestamp()
                
                # If send happens after receive is ready, it's a late sender
                if send_timestamp and recv_timestamp and send_timestamp > recv_timestamp:
                    wait_time = send_timestamp - recv_timestamp
                    
                    # Record the late send
                    self.m_late_sends.append(send_event)
                    
                    # Record wait time
                    event_idx = event.getIdx()
                    if event_idx is not None:
                        self.m_wait_times[event_idx] = wait_time
    
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
                
                # Add analysis results to outputs
                self.m_outputs.add_data({
                    "trace": data,
                    "late_sends": self.m_late_sends.copy(),
                    "wait_times": self.m_wait_times.copy(),
                    "total_wait_time": self.getTotalWaitTime()
                })
