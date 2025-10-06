'''
module trace
'''

from typing import List, Optional
from .event import Event

'''
@class TraceInfo
Basic information of the collected trace
'''


class TraceInfo:
    """
    TraceInfo contains metadata about a trace.
    
    This class stores basic information about which process/thread
    a trace belongs to.
    
    Attributes:
        m_pid: Process ID that this trace belongs to
        m_tid: Thread ID that this trace belongs to
    """
    
    def __init__(self, pid: Optional[int] = None, tid: Optional[int] = None) -> None:
        """
        Initialize TraceInfo object.
        
        Args:
            pid: Process ID that this trace belongs to
            tid: Thread ID that this trace belongs to
        """
        self.m_pid: Optional[int] = pid
        self.m_tid: Optional[int] = tid
    
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