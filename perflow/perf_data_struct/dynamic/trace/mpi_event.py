'''
module mpi event
'''

from typing import Optional
from .event import Event, EventType

'''
@class MpiEvent
An mpi event is an event related to MPI communications.
'''


class MpiEvent(Event):
    """
    MpiEvent represents an MPI communication event.
    
    This class extends Event to include MPI-specific information such as
    the communicator used for the operation.
    
    Attributes:
        m_communicator: MPI communicator for this event
    """
    
    def __init__(
        self,
        event_type: Optional[EventType] = None,
        idx: Optional[int] = None,
        name: Optional[str] = None,
        pid: Optional[int] = None,
        tid: Optional[int] = None,
        timestamp: Optional[float] = None,
        replay_pid: Optional[int] = None,
        communicator: Optional[int] = None
    ) -> None:
        """
        Initialize an MpiEvent object.
        
        Args:
            event_type: Type of the event
            idx: Unique identifier for this event
            name: Human-readable name of the event
            pid: Process ID where this event occurred
            tid: Thread ID where this event occurred
            timestamp: Timestamp when this event occurred
            replay_pid: Process ID for replay purposes
            communicator: MPI communicator identifier
        """
        super().__init__(event_type, idx, name, pid, tid, timestamp, replay_pid)
        self.m_communicator: Optional[int] = communicator
    
    def getCommunicator(self) -> Optional[int]:
        """Get the MPI communicator for this event."""
        return self.m_communicator
    
    def setCommunicator(self, communicator: int) -> None:
        """Set the MPI communicator for this event."""
        self.m_communicator = communicator


'''
@class MpiP2PEvent
Point-to-point MPI communication event
'''


class MpiP2PEvent(MpiEvent):
    """
    MpiP2PEvent represents a point-to-point MPI communication event.
    
    This class extends MpiEvent to include information specific to
    point-to-point communications such as message tags.
    
    Attributes:
        m_tag: Message tag for point-to-point communication
    """
    
    def __init__(
        self,
        event_type: Optional[EventType] = None,
        idx: Optional[int] = None,
        name: Optional[str] = None,
        pid: Optional[int] = None,
        tid: Optional[int] = None,
        timestamp: Optional[float] = None,
        replay_pid: Optional[int] = None,
        communicator: Optional[int] = None,
        tag: Optional[int] = None
    ) -> None:
        """
        Initialize an MpiP2PEvent object.
        
        Args:
            event_type: Type of the event
            idx: Unique identifier for this event
            name: Human-readable name of the event
            pid: Process ID where this event occurred
            tid: Thread ID where this event occurred
            timestamp: Timestamp when this event occurred
            replay_pid: Process ID for replay purposes
            communicator: MPI communicator identifier
            tag: Message tag for point-to-point communication
        """
        super().__init__(event_type, idx, name, pid, tid, timestamp, replay_pid, communicator)
        self.m_tag: Optional[int] = tag
    
    def getTag(self) -> Optional[int]:
        """Get the message tag."""
        return self.m_tag
    
    def setTag(self, tag: int) -> None:
        """Set the message tag."""
        self.m_tag = tag


'''
@class MpiSendEvent
MPI send event
'''


class MpiSendEvent(MpiP2PEvent):
    """
    MpiSendEvent represents an MPI send operation.
    
    This class extends MpiP2PEvent to include send-specific information
    such as the destination process and matching receive event.
    
    Attributes:
        m_dest_pid: Destination process ID
        m_recv_event: Corresponding receive event (if matched)
    """
    
    def __init__(
        self,
        idx: Optional[int] = None,
        name: Optional[str] = None,
        pid: Optional[int] = None,
        tid: Optional[int] = None,
        timestamp: Optional[float] = None,
        replay_pid: Optional[int] = None,
        communicator: Optional[int] = None,
        tag: Optional[int] = None,
        dest_pid: Optional[int] = None
    ) -> None:
        """
        Initialize an MpiSendEvent object.
        
        The event_type is automatically set to EventType.SEND.
        
        Args:
            idx: Unique identifier for this event
            name: Human-readable name of the event
            pid: Process ID where this event occurred
            tid: Thread ID where this event occurred
            timestamp: Timestamp when this event occurred
            replay_pid: Process ID for replay purposes
            communicator: MPI communicator identifier
            tag: Message tag
            dest_pid: Destination process ID
        """
        super().__init__(EventType.SEND, idx, name, pid, tid, timestamp, replay_pid, communicator, tag)
        self.m_dest_pid: Optional[int] = dest_pid
        self.m_recv_event: Optional[Event] = None
    
    def getDestPid(self) -> Optional[int]:
        """Get the destination process ID."""
        return self.m_dest_pid
    
    def setDestPid(self, dest_pid: int) -> None:
        """Set the destination process ID."""
        self.m_dest_pid = dest_pid
    
    def getRecvEvent(self) -> Optional[Event]:
        """Get the corresponding receive event."""
        return self.m_recv_event
    
    def setRecvEvent(self, recv_event: Event) -> None:
        """Set the corresponding receive event."""
        self.m_recv_event = recv_event


'''
@class MpiRecvEvent
MPI receive event
'''


class MpiRecvEvent(MpiP2PEvent):
    """
    MpiRecvEvent represents an MPI receive operation.
    
    This class extends MpiP2PEvent to include receive-specific information
    such as the source process and matching send event.
    
    Attributes:
        m_src_pid: Source process ID
        m_send_event: Corresponding send event (if matched)
    """
    
    def __init__(
        self,
        idx: Optional[int] = None,
        name: Optional[str] = None,
        pid: Optional[int] = None,
        tid: Optional[int] = None,
        timestamp: Optional[float] = None,
        replay_pid: Optional[int] = None,
        communicator: Optional[int] = None,
        tag: Optional[int] = None,
        src_pid: Optional[int] = None
    ) -> None:
        """
        Initialize an MpiRecvEvent object.
        
        The event_type is automatically set to EventType.RECV.
        
        Args:
            idx: Unique identifier for this event
            name: Human-readable name of the event
            pid: Process ID where this event occurred
            tid: Thread ID where this event occurred
            timestamp: Timestamp when this event occurred
            replay_pid: Process ID for replay purposes
            communicator: MPI communicator identifier
            tag: Message tag
            src_pid: Source process ID
        """
        super().__init__(EventType.RECV, idx, name, pid, tid, timestamp, replay_pid, communicator, tag)
        self.m_src_pid: Optional[int] = src_pid
        self.m_send_event: Optional[Event] = None
    
    def getSrcPid(self) -> Optional[int]:
        """Get the source process ID."""
        return self.m_src_pid
    
    def setSrcPid(self, src_pid: int) -> None:
        """Set the source process ID."""
        self.m_src_pid = src_pid
    
    def getSendEvent(self) -> Optional[Event]:
        """Get the corresponding send event."""
        return self.m_send_event
    
    def setSendEvent(self, send_event: Event) -> None:
        """Set the corresponding send event."""
        self.m_send_event = send_event