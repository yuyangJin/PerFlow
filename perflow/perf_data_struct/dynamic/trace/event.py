'''
@module event
'''

from enum import Enum
from typing import Optional

'''
@Enum EventType
Event types for different kinds of trace events
'''


class EventType(Enum):
    """Enumeration of event types in a trace."""
    UNKNOWN = 0
    ENTER = 1
    LEAVE = 2
    SEND = 3
    RECV = 4
    COLLECTIVE = 5
    BARRIER = 6
    COMPUTE = 7


'''
@class Event
An event is a basic unit in the trace. Represents an execution event with
timing and context information.
'''


class Event:
    """
    Event represents an execution event in a trace.
    
    This is the base class for all event types in the trace analysis framework.
    Each event captures timing information, process/thread context, and event-specific
    attributes.
    
    Attributes:
        m_type: Type of the event (EventType)
        m_idx: Unique identifier for this event
        m_name: Human-readable name of the event
        m_pid: Process ID where this event occurred
        m_tid: Thread ID where this event occurred
        m_timestamp: Timestamp when this event occurred
        m_replay_pid: Process ID for replay purposes
    """
    
    def __init__(
        self,
        event_type: Optional[EventType] = None,
        idx: Optional[int] = None,
        name: Optional[str] = None,
        pid: Optional[int] = None,
        tid: Optional[int] = None,
        timestamp: Optional[float] = None,
        replay_pid: Optional[int] = None
    ) -> None:
        """
        Initialize an Event object.
        
        Args:
            event_type: Type of the event
            idx: Unique identifier for this event
            name: Human-readable name of the event
            pid: Process ID where this event occurred
            tid: Thread ID where this event occurred
            timestamp: Timestamp when this event occurred
            replay_pid: Process ID for replay purposes
        """
        self.m_type: Optional[EventType] = event_type
        self.m_idx: Optional[int] = idx
        self.m_name: Optional[str] = name
        self.m_pid: Optional[int] = pid
        self.m_tid: Optional[int] = tid
        self.m_timestamp: Optional[float] = timestamp
        self.m_replay_pid: Optional[int] = replay_pid
    
    def getType(self) -> Optional[EventType]:
        """Get the type of this event."""
        return self.m_type
    
    def getIdx(self) -> Optional[int]:
        """Get the unique identifier of this event."""
        return self.m_idx
    
    def getName(self) -> Optional[str]:
        """Get the name of this event."""
        return self.m_name
    
    def getPid(self) -> Optional[int]:
        """Get the process ID where this event occurred."""
        return self.m_pid
    
    def getTid(self) -> Optional[int]:
        """Get the thread ID where this event occurred."""
        return self.m_tid
    
    def getTimestamp(self) -> Optional[float]:
        """Get the timestamp when this event occurred."""
        return self.m_timestamp
    
    def getReplayPid(self) -> Optional[int]:
        """Get the process ID for replay purposes."""
        return self.m_replay_pid