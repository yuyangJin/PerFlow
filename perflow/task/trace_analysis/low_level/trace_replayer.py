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
    backward order. Callback functions can be registered separately for
    forward and backward replay, enabling various trace analysis tasks.
    
    Attributes:
        m_trace: The trace to be replayed
        m_forward_callbacks: Dictionary of callback functions for forward replay
        m_backward_callbacks: Dictionary of callback functions for backward replay
    """
    
    def __init__(self, trace: Optional[Trace] = None) -> None:
        """
        Initialize a TraceReplayer.
        
        Args:
            trace: Optional trace to replay
        """
        super().__init__()
        self.m_trace: Optional[Trace] = trace
        self.m_forward_callbacks: dict[str, Callable[[Event], None]] = {}
        self.m_backward_callbacks: dict[str, Callable[[Event], None]] = {}
    
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
    
    def registerForwardCallback(self, name: str, callback: Callable[[Event], None]) -> None:
        """
        Register a callback function for forward replay event processing.
        
        Callbacks are invoked during forward replay for each event, allowing
        custom analysis logic to be executed.
        
        Args:
            name: Name identifier for the callback
            callback: Function that takes an Event and returns None
        """
        self.m_forward_callbacks[name] = callback
    
    def registerBackwardCallback(self, name: str, callback: Callable[[Event], None]) -> None:
        """
        Register a callback function for backward replay event processing.
        
        Callbacks are invoked during backward replay for each event, allowing
        custom analysis logic to be executed.
        
        Args:
            name: Name identifier for the callback
            callback: Function that takes an Event and returns None
        """
        self.m_backward_callbacks[name] = callback
    
    def registerCallback(self, name: str, callback: Callable[[Event], None]) -> None:
        """
        Register a callback function for both forward and backward replay.
        
        This is a convenience method that registers the same callback for
        both forward and backward replay directions.
        
        Args:
            name: Name identifier for the callback
            callback: Function that takes an Event and returns None
        """
        self.m_forward_callbacks[name] = callback
        self.m_backward_callbacks[name] = callback
    
    def unregisterForwardCallback(self, name: str) -> None:
        """
        Unregister a forward replay callback function.
        
        Args:
            name: Name identifier of the callback to remove
        """
        if name in self.m_forward_callbacks:
            del self.m_forward_callbacks[name]
    
    def unregisterBackwardCallback(self, name: str) -> None:
        """
        Unregister a backward replay callback function.
        
        Args:
            name: Name identifier of the callback to remove
        """
        if name in self.m_backward_callbacks:
            del self.m_backward_callbacks[name]
    
    def unregisterCallback(self, name: str) -> None:
        """
        Unregister a callback function from both forward and backward replay.
        
        Args:
            name: Name identifier of the callback to remove
        """
        if name in self.m_forward_callbacks:
            del self.m_forward_callbacks[name]
        if name in self.m_backward_callbacks:
            del self.m_backward_callbacks[name]
    
    def forwardReplay(self) -> None:
        """
        Replay the trace in forward (chronological) order.
        
        Events are processed from earliest to latest timestamp.
        All registered forward callbacks are invoked for each event.
        """
        if self.m_trace is None:
            return
        
        events = self.m_trace.getEvents()
        for event in events:
            # Invoke all registered forward callbacks
            for callback in self.m_forward_callbacks.values():
                callback(event)
    
    def backwardReplay(self) -> None:
        """
        Replay the trace in backward (reverse chronological) order.
        
        Events are processed from latest to earliest timestamp.
        All registered backward callbacks are invoked for each event.
        """
        if self.m_trace is None:
            return
        
        events = self.m_trace.getEvents()
        for event in reversed(events):
            # Invoke all registered backward callbacks
            for callback in self.m_backward_callbacks.values():
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