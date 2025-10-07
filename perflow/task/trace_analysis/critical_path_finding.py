'''
module critical path finding
'''

from typing import Dict, List, Optional, Set, Tuple, Any
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
    
    The algorithm uses the Critical Path Method (CPM) with two passes:
    1. Forward pass: Compute earliest start/finish times for all events
    2. Backward pass: Compute latest start/finish times for all events
    3. Critical path: Events with zero slack (earliest == latest)
    
    The algorithm considers:
    - Sequential dependencies within a process (one event follows another)
    - Communication dependencies (send-receive pairs)
    - Computation time of each event
    
    Attributes:
        m_event_costs: Dictionary mapping event IDs to their execution costs
        m_critical_path: List of events on the critical path
        m_critical_path_length: Total length (time) of the critical path
        m_earliest_start_times: Dictionary of earliest start times
        m_earliest_finish_times: Dictionary of earliest finish times
        m_latest_start_times: Dictionary of latest start times
        m_latest_finish_times: Dictionary of latest finish times
        m_slack_times: Dictionary of slack times (latest - earliest)
        m_predecessors: Dictionary tracking predecessor events
        m_successors: Dictionary tracking successor events
    """
    
    def __init__(self, trace: Optional[Trace] = None) -> None:
        """
        Initialize a CriticalPathFinding analyzer.
        
        Args:
            trace: Optional trace to analyze
        """
        super().__init__(trace)
        self.m_event_costs: Dict[int, float] = {}
        self.m_critical_path: List[Event] = []
        self.m_critical_path_length: float = 0.0
        
        # Forward pass results
        self.m_earliest_start_times: Dict[int, float] = {}
        self.m_earliest_finish_times: Dict[int, float] = {}
        
        # Backward pass results
        self.m_latest_start_times: Dict[int, float] = {}
        self.m_latest_finish_times: Dict[int, float] = {}
        
        # Slack times
        self.m_slack_times: Dict[int, float] = {}
        
        # Dependency tracking
        self.m_predecessors: Dict[int, List[int]] = {}
        self.m_successors: Dict[int, List[int]] = {}
        
        # Track process-local previous events for dependency construction
        self.m_process_last_event: Dict[int, Event] = {}
        
        # Register callback for forward pass
        self.registerCallback("compute_earliest_times", 
                            self._compute_earliest_times, 
                            ReplayDirection.FWD)
        
        # Register callback for backward pass
        self.registerCallback("compute_latest_times",
                            self._compute_latest_times,
                            ReplayDirection.BWD)
    
    def _compute_earliest_times(self, event: Event) -> None:
        """
        Compute earliest start and finish times for each event during forward replay.
        
        This implements the forward pass of CPM where each event's earliest
        start time is the maximum of all its predecessors' earliest finish times.
        
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
        
        # Initialize predecessor list
        if event_idx not in self.m_predecessors:
            self.m_predecessors[event_idx] = []
        
        # Initialize successor list
        if event_idx not in self.m_successors:
            self.m_successors[event_idx] = []
        
        # Initialize earliest start time
        earliest_start = 0.0
        
        # Check process-local dependency (sequential execution)
        pid = event.getPid()
        if pid is not None and pid in self.m_process_last_event:
            prev_event = self.m_process_last_event[pid]
            prev_idx = prev_event.getIdx()
            if prev_idx is not None and prev_idx in self.m_earliest_finish_times:
                # Add dependency edge
                self.m_predecessors[event_idx].append(prev_idx)
                if prev_idx not in self.m_successors:
                    self.m_successors[prev_idx] = []
                self.m_successors[prev_idx].append(event_idx)
                
                # Update earliest start time
                local_dep_time = self.m_earliest_finish_times[prev_idx]
                earliest_start = max(earliest_start, local_dep_time)
        
        # Check communication dependency (for receive events)
        if isinstance(event, MpiRecvEvent):
            send_event = event.getSendEvent()
            if send_event is not None:
                send_idx = send_event.getIdx()
                if send_idx is not None and send_idx in self.m_earliest_finish_times:
                    # Add dependency edge
                    self.m_predecessors[event_idx].append(send_idx)
                    if send_idx not in self.m_successors:
                        self.m_successors[send_idx] = []
                    self.m_successors[send_idx].append(event_idx)
                    
                    # Update earliest start time
                    comm_dep_time = self.m_earliest_finish_times[send_idx]
                    earliest_start = max(earliest_start, comm_dep_time)
        
        # Compute earliest start and finish times for this event
        self.m_earliest_start_times[event_idx] = earliest_start
        self.m_earliest_finish_times[event_idx] = earliest_start + event_cost
        
        # Update process-local last event
        if pid is not None:
            self.m_process_last_event[pid] = event
    
    def _compute_latest_times(self, event: Event) -> None:
        """
        Compute latest start and finish times for each event during backward replay.
        
        This implements the backward pass of CPM where each event's latest
        finish time is the minimum of all its successors' latest start times.
        
        Args:
            event: Event being processed during backward replay
        """
        event_idx = event.getIdx()
        if event_idx is None:
            return
        
        # Get event cost
        event_cost = self.m_event_costs.get(event_idx, 0.1)
        
        # If this event has no successors, its latest finish time equals
        # the maximum earliest finish time (end of critical path)
        if event_idx not in self.m_successors or not self.m_successors[event_idx]:
            # This is a terminal event
            self.m_latest_finish_times[event_idx] = self.m_critical_path_length
        else:
            # Latest finish time is the minimum of all successors' latest start times
            latest_finish = float('inf')
            for succ_idx in self.m_successors[event_idx]:
                if succ_idx in self.m_latest_start_times:
                    latest_finish = min(latest_finish, self.m_latest_start_times[succ_idx])
            
            if latest_finish == float('inf'):
                # If no valid successor latest start time found, use critical path length
                latest_finish = self.m_critical_path_length
            
            self.m_latest_finish_times[event_idx] = latest_finish
        
        # Latest start time = latest finish time - event cost
        self.m_latest_start_times[event_idx] = self.m_latest_finish_times[event_idx] - event_cost
        
        # Compute slack time (total float)
        earliest_start = self.m_earliest_start_times.get(event_idx, 0.0)
        latest_start = self.m_latest_start_times[event_idx]
        self.m_slack_times[event_idx] = latest_start - earliest_start
    
    def _identify_critical_path(self) -> None:
        """
        Identify the critical path based on zero slack times.
        
        Events with zero (or near-zero) slack are on the critical path.
        This method finds all such events and orders them chronologically.
        """
        if not self.m_slack_times:
            return
        
        # Find events with zero slack (allowing small floating point tolerance)
        tolerance = 1e-9
        critical_event_indices = [
            idx for idx, slack in self.m_slack_times.items()
            if abs(slack) < tolerance
        ]
        
        if not critical_event_indices:
            return
        
        # The critical path length is the maximum earliest finish time
        if self.m_earliest_finish_times:
            self.m_critical_path_length = max(self.m_earliest_finish_times.values())
        
        # Convert indices to events and sort by earliest start time
        if self.m_trace:
            idx_to_event = {e.getIdx(): e for e in self.m_trace.getEvents() 
                          if e.getIdx() is not None}
            
            critical_events = [
                idx_to_event[idx] for idx in critical_event_indices
                if idx in idx_to_event
            ]
            
            # Sort events by their earliest start time
            critical_events.sort(
                key=lambda e: self.m_earliest_start_times.get(e.getIdx() or 0, 0.0)
            )
            
            self.m_critical_path = critical_events
    
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
    
    def getEventEarliestStartTime(self, event_idx: int) -> Optional[float]:
        """
        Get the earliest start time for a specific event.
        
        Args:
            event_idx: Event index
            
        Returns:
            Earliest start time, or None if not computed
        """
        return self.m_earliest_start_times.get(event_idx)
    
    def getEventEarliestFinishTime(self, event_idx: int) -> Optional[float]:
        """
        Get the earliest finish time for a specific event.
        
        Args:
            event_idx: Event index
            
        Returns:
            Earliest finish time, or None if not computed
        """
        return self.m_earliest_finish_times.get(event_idx)
    
    def getEventLatestStartTime(self, event_idx: int) -> Optional[float]:
        """
        Get the latest start time for a specific event.
        
        Args:
            event_idx: Event index
            
        Returns:
            Latest start time, or None if not computed
        """
        return self.m_latest_start_times.get(event_idx)
    
    def getEventLatestFinishTime(self, event_idx: int) -> Optional[float]:
        """
        Get the latest finish time for a specific event.
        
        Args:
            event_idx: Event index
            
        Returns:
            Latest finish time, or None if not computed
        """
        return self.m_latest_finish_times.get(event_idx)
    
    def getEventSlackTime(self, event_idx: int) -> Optional[float]:
        """
        Get the slack time (total float) for a specific event.
        
        Slack time is the amount of time an event can be delayed without
        affecting the total project duration. Events with zero slack are
        on the critical path.
        
        Args:
            event_idx: Event index
            
        Returns:
            Slack time, or None if not computed
        """
        return self.m_slack_times.get(event_idx)
    
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
    
    def getCriticalPathStatistics(self) -> Dict[str, Any]:
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
        self.m_event_costs.clear()
        self.m_critical_path.clear()
        self.m_critical_path_length = 0.0
        self.m_earliest_start_times.clear()
        self.m_earliest_finish_times.clear()
        self.m_latest_start_times.clear()
        self.m_latest_finish_times.clear()
        self.m_slack_times.clear()
        self.m_predecessors.clear()
        self.m_successors.clear()
        self.m_process_last_event.clear()
    
    def run(self) -> None:
        """
        Execute critical path finding analysis using the Critical Path Method.
        
        This performs:
        1. Forward pass to compute earliest start/finish times
        2. Backward pass to compute latest start/finish times
        3. Slack computation and critical path identification
        
        Results are stored in m_critical_path and related attributes.
        """
        # Clear previous results
        self.clear()
        
        # Process traces from input data
        for data in self.m_inputs.get_data():
            if isinstance(data, Trace):
                self.setTrace(data)
                
                # Forward pass: compute earliest times and build dependency graph
                self.forwardReplay()
                
                # Set critical path length (max earliest finish time)
                if self.m_earliest_finish_times:
                    self.m_critical_path_length = max(self.m_earliest_finish_times.values())
                
                # Backward pass: compute latest times and slack
                self.backwardReplay()
                
                # Identify critical path (events with zero slack)
                self._identify_critical_path()
                
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
