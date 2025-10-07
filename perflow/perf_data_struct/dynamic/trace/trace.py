'''
module trace
'''

from typing import List, Optional, Dict
from .event import Event

'''
@class TraceInfo
Basic information of the collected trace
'''


class TraceInfo:
    """
    TraceInfo contains metadata about a trace collection.
    
    This class stores comprehensive information about trace collection from
    large-scale parallel applications. It includes both local trace metadata
    (process/thread IDs) and global metadata (total execution processes,
    replay processes, and distribution mapping).
    
    Attributes:
        m_pid: Process ID that this trace belongs to (local)
        m_tid: Thread ID that this trace belongs to (local)
        m_num_execution_processes: Total number of execution processes (EP) in the trace collection
        m_num_replay_processes: Total number of replay processes (RP) for distributed trace processing
        m_ep_to_rp_mapping: Mapping from execution process IDs to replay process IDs (Dict[int, int])
        m_trace_format: Format of the trace (e.g., "OTF2", "Scalatrace", "Nsight")
        m_trace_start_time: Global start time of the trace collection
        m_trace_end_time: Global end time of the trace collection
        m_application_name: Name of the application that generated the trace
    """
    
    def __init__(
        self,
        pid: Optional[int] = None,
        tid: Optional[int] = None,
        num_execution_processes: Optional[int] = None,
        num_replay_processes: Optional[int] = None,
        ep_to_rp_mapping: Optional[Dict[int, int]] = None,
        trace_format: Optional[str] = None,
        trace_start_time: Optional[float] = None,
        trace_end_time: Optional[float] = None,
        application_name: Optional[str] = None
    ) -> None:
        """
        Initialize TraceInfo object.
        
        Args:
            pid: Process ID that this trace belongs to (local)
            tid: Thread ID that this trace belongs to (local)
            num_execution_processes: Total number of execution processes (EP)
            num_replay_processes: Total number of replay processes (RP)
            ep_to_rp_mapping: Mapping from EP IDs to RP IDs for distributed trace processing
            trace_format: Format of the trace file (e.g., "OTF2", "Scalatrace")
            trace_start_time: Global start timestamp of the trace collection
            trace_end_time: Global end timestamp of the trace collection
            application_name: Name of the application that generated the trace
        """
        # Local trace metadata
        self.m_pid: Optional[int] = pid
        self.m_tid: Optional[int] = tid
        
        # Global trace collection metadata
        self.m_num_execution_processes: Optional[int] = num_execution_processes
        self.m_num_replay_processes: Optional[int] = num_replay_processes
        self.m_ep_to_rp_mapping: Dict[int, int] = ep_to_rp_mapping if ep_to_rp_mapping is not None else {}
        
        # Additional trace metadata
        self.m_trace_format: Optional[str] = trace_format
        self.m_trace_start_time: Optional[float] = trace_start_time
        self.m_trace_end_time: Optional[float] = trace_end_time
        self.m_application_name: Optional[str] = application_name
    
    def getPid(self) -> Optional[int]:
        """Get the process ID."""
        return self.m_pid
    
    def getTid(self) -> Optional[int]:
        """Get the thread ID."""
        return self.m_tid
    
    def setPid(self, pid: int) -> None:
        """Set the process ID."""
        self.m_pid = pid
    
    def setTid(self, tid: int) -> None:
        """Set the thread ID."""
        self.m_tid = tid
    
    def getNumExecutionProcesses(self) -> Optional[int]:
        """Get the total number of execution processes."""
        return self.m_num_execution_processes
    
    def setNumExecutionProcesses(self, num_ep: int) -> None:
        """Set the total number of execution processes."""
        self.m_num_execution_processes = num_ep
    
    def getNumReplayProcesses(self) -> Optional[int]:
        """Get the total number of replay processes."""
        return self.m_num_replay_processes
    
    def setNumReplayProcesses(self, num_rp: int) -> None:
        """Set the total number of replay processes."""
        self.m_num_replay_processes = num_rp
    
    def getEpToRpMapping(self) -> Dict[int, int]:
        """Get the mapping from execution process IDs to replay process IDs."""
        return self.m_ep_to_rp_mapping
    
    def setEpToRpMapping(self, mapping: Dict[int, int]) -> None:
        """Set the mapping from execution process IDs to replay process IDs."""
        self.m_ep_to_rp_mapping = mapping
    
    def addEpToRpMapping(self, ep_id: int, rp_id: int) -> None:
        """
        Add a mapping from an execution process ID to a replay process ID.
        
        Args:
            ep_id: Execution process ID
            rp_id: Replay process ID
        """
        self.m_ep_to_rp_mapping[ep_id] = rp_id
    
    def getReplayProcessForEp(self, ep_id: int) -> Optional[int]:
        """
        Get the replay process ID for a given execution process ID.
        
        Args:
            ep_id: Execution process ID
            
        Returns:
            Replay process ID, or None if not found
        """
        return self.m_ep_to_rp_mapping.get(ep_id)
    
    def getTraceFormat(self) -> Optional[str]:
        """Get the trace format."""
        return self.m_trace_format
    
    def setTraceFormat(self, trace_format: str) -> None:
        """Set the trace format."""
        self.m_trace_format = trace_format
    
    def getTraceStartTime(self) -> Optional[float]:
        """Get the global start time of the trace collection."""
        return self.m_trace_start_time
    
    def setTraceStartTime(self, start_time: float) -> None:
        """Set the global start time of the trace collection."""
        self.m_trace_start_time = start_time
    
    def getTraceEndTime(self) -> Optional[float]:
        """Get the global end time of the trace collection."""
        return self.m_trace_end_time
    
    def setTraceEndTime(self, end_time: float) -> None:
        """Set the global end time of the trace collection."""
        self.m_trace_end_time = end_time
    
    def getTraceDuration(self) -> Optional[float]:
        """
        Calculate the total duration of the trace collection.
        
        Returns:
            Duration in seconds, or None if start/end times are not set
        """
        if self.m_trace_start_time is not None and self.m_trace_end_time is not None:
            return self.m_trace_end_time - self.m_trace_start_time
        return None
    
    def getApplicationName(self) -> Optional[str]:
        """Get the application name."""
        return self.m_application_name
    
    def setApplicationName(self, app_name: str) -> None:
        """Set the application name."""
        self.m_application_name = app_name


'''
@class Trace
A trace is a collection of events.
'''


class Trace:
    """
    Trace represents a collection of events from program execution.
    
    This class serves as a container for events and their associated metadata.
    It provides methods to add, access, and manage events within the trace.
    
    Attributes:
        m_events: List of events in chronological order
        m_traceinfo: Metadata about this trace (process/thread info)
    """
    
    def __init__(self) -> None:
        """
        Initialize a Trace object.
        
        Creates an empty trace with no events and default trace info.
        """
        self.m_events: List[Event] = []
        self.m_traceinfo: TraceInfo = TraceInfo()
    
    def addEvent(self, event: Event) -> None:
        """
        Add an event to the trace.
        
        Args:
            event: The event to add to this trace
        """
        self.m_events.append(event)
    
    def getEvents(self) -> List[Event]:
        """
        Get all events in the trace.
        
        Returns:
            List of all events in this trace
        """
        return self.m_events
    
    def getEvent(self, index: int) -> Optional[Event]:
        """
        Get a specific event by index.
        
        Args:
            index: Index of the event to retrieve
            
        Returns:
            The event at the specified index, or None if out of bounds
        """
        if 0 <= index < len(self.m_events):
            return self.m_events[index]
        return None
    
    def getEventCount(self) -> int:
        """
        Get the total number of events in the trace.
        
        Returns:
            Number of events
        """
        return len(self.m_events)
    
    def getTraceInfo(self) -> TraceInfo:
        """
        Get the trace metadata.
        
        Returns:
            TraceInfo object containing metadata about this trace
        """
        return self.m_traceinfo
    
    def setTraceInfo(self, trace_info: TraceInfo) -> None:
        """
        Set the trace metadata.
        
        Args:
            trace_info: TraceInfo object to set for this trace
        """
        self.m_traceinfo = trace_info
    
    def clear(self) -> None:
        """Clear all events from the trace."""
        self.m_events.clear()