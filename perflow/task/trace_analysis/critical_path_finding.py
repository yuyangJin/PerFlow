'''
module critical path finding
'''

from typing import Dict, List, Optional, Set, Tuple
from .low_level.trace_replayer import TraceReplayer, ReplayDirection
from ...perf_data_struct.dynamic.trace.trace import Trace
from ...perf_data_struct.dynamic.trace.event import Event, EventType
from ...perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent


'''
@class CriticalPathFinding
Critical path analysis identifies the longest chain of dependent events in a trace.
This is the sequence of events that determines the minimum execution time.
'''


class CriticalPathFinding(TraceReplayer):
    """
    CriticalPathFinding performs critical path analysis on traces.
    
    This class extends TraceReplayer to identify the critical path - the longest
    chain of dependent events from start to finish. The critical path represents
    the minimum possible execution time and helps identify performance bottlenecks.
    
    The algorithm considers:
    - Sequential dependencies within a process (one event follows another)
    - Communication dependencies (send-receive pairs)
    - Computation time of each event
    
    Attributes:
        m_event_times: Dictionary mapping event IDs to their start times
        m_event_costs: Dictionary mapping event IDs to their execution costs
        m_critical_path: List of events on the critical path
        m_critical_path_length: Total length (time) of the critical path
        m_predecessors: Dictionary tracking predecessor events for path reconstruction
    """
    
    def __init__(self, trace: Optional[Trace] = None) -> None:
        """
        Initialize a CriticalPathFinding analyzer.
        
        Args:
            trace: Optional trace to analyze
        """
        super().__init__(trace)
        self.m_event_times: Dict[int, float] = {}
        self.m_event_costs: Dict[int, float] = {}
        self.m_critical_path: List[Event] = []
        self.m_critical_path_length: float = 0.0
        self.m_predecessors: Dict[int, Optional[int]] = {}
        self.m_earliest_finish_times: Dict[int, float] = {}
        
        # Track process-local previous events
        self.m_process_last_event: Dict[int, Event] = {}
        
        # Register callback for critical path computation
        self.registerCallback("compute_critical_path", 
                            self._compute_earliest_times, 
                            ReplayDirection.FWD)
    
    def _compute_earliest_times(self, event: Event) -> None:
        """
        Compute earliest finish time for each event during forward replay.
        
        This implements a dynamic programming approach where each event's
        earliest finish time is computed based on its dependencies.
        
        Args:
            event: Event being processed during replay
        """
        event_idx = event.getIdx()
        if event_idx is None:
            return
        
        # Get event timestamp and duration
        timestamp = event.getTimestamp()
        if timestamp is None:
            timestamp = 0.0
        
        # Estimate event cost (for simplicity, use a small default)
        # In real scenarios, this would be the actual computation time
        event_cost = 0.1
        self.m_event_costs[event_idx] = event_cost
        
        # Initialize earliest finish time
        earliest_start = 0.0
        predecessor = None
        
        # Check process-local dependency (sequential execution)
        pid = event.getPid()
        if pid is not None and pid in self.m_process_last_event:
            prev_event = self.m_process_last_event[pid]
            prev_idx = prev_event.getIdx()
            if prev_idx is not None and prev_idx in self.m_earliest_finish_times:
                local_dep_time = self.m_earliest_finish_times[prev_idx]
                if local_dep_time > earliest_start:
                    earliest_start = local_dep_time
                    predecessor = prev_idx
        
        # Check communication dependency (for receive events)
        if isinstance(event, MpiRecvEvent):
            send_event = event.getSendEvent()
            if send_event is not None:
                send_idx = send_event.getIdx()
                if send_idx is not None and send_idx in self.m_earliest_finish_times:
                    comm_dep_time = self.m_earliest_finish_times[send_idx]
                    if comm_dep_time > earliest_start:
                        earliest_start = comm_dep_time
                        predecessor = send_idx
        
        # Compute earliest finish time for this event
        earliest_finish = earliest_start + event_cost
        self.m_earliest_finish_times[event_idx] = earliest_finish
        self.m_predecessors[event_idx] = predecessor
        
        # Update process-local last event
        if pid is not None:
            self.m_process_last_event[pid] = event
    
    def _reconstruct_critical_path(self) -> None:
        """
        Reconstruct the critical path by backtracking from the last event.
        
        This method finds the event with the latest finish time and traces
        back through predecessors to build the critical path.
        """
        if not self.m_earliest_finish_times:
            return
        
        # Find the event with the maximum earliest finish time
        last_event_idx = max(self.m_earliest_finish_times.items(), 
                            key=lambda x: x[1])[0]
        self.m_critical_path_length = self.m_earliest_finish_times[last_event_idx]
        
        # Backtrack to build the critical path
        path_indices = []
        current_idx: Optional[int] = last_event_idx
        
        while current_idx is not None:
            path_indices.append(current_idx)
            current_idx = self.m_predecessors.get(current_idx)
        
        # Reverse to get path from start to end
        path_indices.reverse()
        
        # Convert indices to events
        if self.m_trace:
            idx_to_event = {e.getIdx(): e for e in self.m_trace.getEvents() 
                          if e.getIdx() is not None}
            self.m_critical_path = [idx_to_event[idx] for idx in path_indices 
                                  if idx in idx_to_event]
    
    def getCriticalPath(self) -> List[Event]:
        """
        Get the critical path events.
        
        Returns:
            List of events on the critical path, ordered from start to end
        """
        return self.m_critical_path
    
    def getCriticalPathLength(self) -> float:
        """
        Get the length (total time) of the critical path.
        
        Returns:
            Total time of the critical path
        """
        return self.m_critical_path_length
    
    def getEventEarliestFinishTime(self, event_idx: int) -> Optional[float]:
        """
        Get the earliest finish time for a specific event.
        
        Args:
            event_idx: Event index
            
        Returns:
            Earliest finish time, or None if not computed
        """
        return self.m_earliest_finish_times.get(event_idx)
    
    def isCriticalEvent(self, event: Event) -> bool:
        """
        Check if an event is on the critical path.
        
        Args:
            event: Event to check
            
        Returns:
            True if the event is on the critical path
        """
        return event in self.m_critical_path
    
    def getCriticalProcesses(self) -> Set[int]:
        """
        Get the set of processes that have events on the critical path.
        
        Returns:
            Set of process IDs on the critical path
        """
        processes = set()
        for event in self.m_critical_path:
            pid = event.getPid()
            if pid is not None:
                processes.add(pid)
        return processes
    
    def getBottleneckEvents(self, top_n: int = 5) -> List[Tuple[Event, float]]:
        """
        Get the top N events with the longest execution times on critical path.
        
        Args:
            top_n: Number of top bottleneck events to return
            
        Returns:
            List of (event, cost) tuples sorted by cost (descending)
        """
        critical_events_with_cost = []
        for event in self.m_critical_path:
            event_idx = event.getIdx()
            if event_idx is not None and event_idx in self.m_event_costs:
                cost = self.m_event_costs[event_idx]
                critical_events_with_cost.append((event, cost))
        
        # Sort by cost descending
        critical_events_with_cost.sort(key=lambda x: x[1], reverse=True)
        
        return critical_events_with_cost[:top_n]
    
    def getCriticalPathStatistics(self) -> Dict[str, any]:
        """
        Get comprehensive statistics about the critical path.
        
        Returns:
            Dictionary with statistics including:
            - total_length: Total critical path length
            - num_events: Number of events on critical path
            - num_processes: Number of processes involved
            - num_communications: Number of communication events
            - computation_time: Total computation time
            - communication_time: Total communication time
        """
        num_send = 0
        num_recv = 0
        comp_time = 0.0
        comm_time = 0.0
        
        for event in self.m_critical_path:
            event_idx = event.getIdx()
            if event_idx is not None and event_idx in self.m_event_costs:
                cost = self.m_event_costs[event_idx]
                
                if isinstance(event, (MpiSendEvent, MpiRecvEvent)):
                    comm_time += cost
                    if isinstance(event, MpiSendEvent):
                        num_send += 1
                    else:
                        num_recv += 1
                else:
                    comp_time += cost
        
        return {
            "total_length": self.m_critical_path_length,
            "num_events": len(self.m_critical_path),
            "num_processes": len(self.getCriticalProcesses()),
            "num_send_events": num_send,
            "num_recv_events": num_recv,
            "computation_time": comp_time,
            "communication_time": comm_time
        }
    
    def clear(self) -> None:
        """Clear all analysis results."""
        self.m_event_times.clear()
        self.m_event_costs.clear()
        self.m_critical_path.clear()
        self.m_critical_path_length = 0.0
        self.m_predecessors.clear()
        self.m_earliest_finish_times.clear()
        self.m_process_last_event.clear()
    
    def run(self) -> None:
        """
        Execute critical path finding analysis.
        
        Processes input traces and identifies the critical path.
        Results are stored in m_critical_path and m_critical_path_length.
        """
        # Clear previous results
        self.clear()
        
        # Process traces from input data
        for data in self.m_inputs.get_data():
            if isinstance(data, Trace):
                self.setTrace(data)
                
                # Compute earliest finish times
                self.forwardReplay()
                
                # Reconstruct the critical path
                self._reconstruct_critical_path()
                
                # Add analysis results to outputs
                stats = self.getCriticalPathStatistics()
                output = (
                    "CriticalPathAnalysis",
                    data,
                    tuple(self.m_critical_path),
                    self.m_critical_path_length,
                    tuple(stats.items())  # Convert dict to tuple of tuples for hashability
                )
                self.m_outputs.add_data(output)
