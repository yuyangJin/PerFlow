'''
module trace replayer
'''

from typing import Any, Callable, Optional
from ....flow.flow import FlowNode
from ....perf_data_struct.dynamic.trace.trace import Trace
from ....perf_data_struct.dynamic.trace.event import Event

'''
@class TraceReplayer
Replay the trace. The callback functions are designed for implementing different tasks.
'''


class TraceReplayer(FlowNode):
    """
    TraceReplayer replays trace events with callback support.
    
    This class provides functionality to replay trace events in forward or
    backward order. Callback functions can be registered to process events
    during replay, enabling various trace analysis tasks.
    
    Attributes:
        m_trace: The trace to be replayed
        m_callbacks: Dictionary of registered callback functions
    """
    
    def __init__(self, trace: Optional[Trace] = None) -> None:
        """
        Initialize a TraceReplayer.
        
        Args:
            trace: Optional trace to replay
        """
        super().__init__()
        self.m_trace: Optional[Trace] = trace
        self.m_callbacks: dict[str, Callable[[Event], None]] = {}
    
    def setTrace(self, trace: Trace) -> None:
        """
        Set the trace to be replayed.
        
        Args:
            trace: Trace object to replay
        """
        self.m_trace = trace
    
    def getTrace(self) -> Optional[Trace]:
        """
        Get the trace being replayed.
        
        Returns:
            The trace object
        """
        return self.m_trace
    
    def registerCallback(self, name: str, callback: Callable[[Event], None]) -> None:
        """
        Register a callback function for event processing.
        
        Callbacks are invoked during replay for each event, allowing
        custom analysis logic to be executed.
        
        Args:
            name: Name identifier for the callback
            callback: Function that takes an Event and returns None
        """
        self.m_callbacks[name] = callback
    
    def unregisterCallback(self, name: str) -> None:
        """
        Unregister a callback function.
        
        Args:
            name: Name identifier of the callback to remove
        """
        if name in self.m_callbacks:
            del self.m_callbacks[name]
    
    def forwardReplay(self) -> None:
        """
        Replay the trace in forward (chronological) order.
        
        Events are processed from earliest to latest timestamp.
        All registered callbacks are invoked for each event.
        """
        if self.m_trace is None:
            return
        
        events = self.m_trace.getEvents()
        for event in events:
            # Invoke all registered callbacks
            for callback in self.m_callbacks.values():
                callback(event)
    
    def backwardReplay(self) -> None:
        """
        Replay the trace in backward (reverse chronological) order.
        
        Events are processed from latest to earliest timestamp.
        All registered callbacks are invoked for each event.
        """
        if self.m_trace is None:
            return
        
        events = self.m_trace.getEvents()
        for event in reversed(events):
            # Invoke all registered callbacks
            for callback in self.m_callbacks.values():
                callback(event)
    
    def run(self) -> None:
        """
        Execute the trace replay task.
        
        Default implementation performs a forward replay.
        Subclasses can override this to implement specific analysis logic.
        """
        # Extract traces from input data
        for data in self.m_inputs.get_data():
            if isinstance(data, Trace):
                self.setTrace(data)
                self.forwardReplay()
                # Add trace to outputs
                self.m_outputs.add_data(data)