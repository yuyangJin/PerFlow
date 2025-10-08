'''
module critical path finding
'''

from typing import Dict, List, Optional, Set, Tuple, Any
import sys
import psutil
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
        m_enable_memory_tracking: Flag to enable/disable memory consumption tracking
        m_enable_detailed_memory_tracking: Flag to enable tracking of individual data structures
        m_memory_stats: Dictionary storing memory consumption statistics
        m_memory_timeline: List of (phase, event_count, memory_bytes) tuples tracking memory over time
        m_detailed_memory_timeline: List of (phase, event_count, {var_name: memory_bytes}) tracking individual variables
        m_memory_sample_interval: Interval for sampling memory during replay (events)
    """
    
    def __init__(self, trace: Optional[Trace] = None, enable_memory_tracking: bool = False, 
                 enable_detailed_memory_tracking: bool = False,
                 recomputation_mode: str = 'none', recomputation_threshold: int = 5,
                 checkpoint_interval: int = 500) -> None:
        """
        Initialize a CriticalPathFinding analyzer.
        
        Args:
            trace: Optional trace to analyze
            enable_memory_tracking: If True, track memory consumption during analysis
            enable_detailed_memory_tracking: If True, track memory of individual data structures
            recomputation_mode: Memory optimization mode - 'none', 'full', or 'partial'
                - 'none': Store all earliest times (Step 1 optimization only)
                - 'full': Store no earliest times, recompute all during backward pass
                - 'partial': Store only for events with >= threshold predecessors
            recomputation_threshold: Number of predecessors threshold for partial mode
            checkpoint_interval: Store checkpoints every N events to limit recursion depth
                (used in full recomputation mode, default: 500, based on Python's recursion limit)
        """
        super().__init__(trace)
        self.m_event_costs: Dict[int, float] = {}
        self.m_critical_path: List[Event] = []
        self.m_critical_path_length: float = 0.0
        self.m_max_earliest_finish: float = 0.0  # Track max even in recomputation mode
        
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
        
        # Memory tracking
        self.m_enable_memory_tracking: bool = enable_memory_tracking
        self.m_enable_detailed_memory_tracking: bool = enable_detailed_memory_tracking
        self.m_memory_stats: Dict[str, Any] = {}
        self.m_memory_timeline: List[Tuple[str, int, float]] = []  # (phase, event_count, memory_bytes)
        self.m_memory_sample_interval: int = 1000  # Sample memory every N events
        
        # Detailed memory tracking for individual data structures
        self.m_detailed_memory_timeline: List[Tuple[str, int, Dict[str, float]]] = []  # (phase, event_count, {var_name: memory_bytes})
        
        # Step 2: Recomputation strategy
        self.m_recomputation_mode: str = recomputation_mode
        self.m_recomputation_threshold: int = recomputation_threshold
        self.m_checkpoint_interval: int = checkpoint_interval
        self.m_event_counter: int = 0  # Track event count for checkpointing
        self.m_recomputation_stats: Dict[str, int] = {
            'stored_events': 0,
            'recomputed_events': 0,
            'checkpoint_events': 0
        }
        
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
        
        Step 2 optimization: Based on recomputation mode, may skip storing earliest times
        to save memory. These will be recomputed during backward pass if needed.
        
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
        prev_finish_time = None
        if pid is not None and pid in self.m_process_last_event:
            prev_event = self.m_process_last_event[pid]
            prev_idx = prev_event.getIdx()
            if prev_idx is not None:
                # Always add dependency edge (needed for backward pass)
                self.m_predecessors[event_idx].append(prev_idx)
                if prev_idx not in self.m_successors:
                    self.m_successors[prev_idx] = []
                self.m_successors[prev_idx].append(event_idx)
                
                # Get predecessor's finish time (from storage or tracking)
                if prev_idx in self.m_earliest_finish_times:
                    prev_finish_time = self.m_earliest_finish_times[prev_idx]
                # If not stored, we need to track it somehow - use process tracking
                # Store previous event's computed time temporarily
                if not hasattr(self, 'm_temp_finish_times'):
                    self.m_temp_finish_times = {}
                if prev_idx in self.m_temp_finish_times:
                    prev_finish_time = self.m_temp_finish_times[prev_idx]
                
                if prev_finish_time is not None:
                    earliest_start = max(earliest_start, prev_finish_time)
        
        # Check communication dependency (for receive events)
        if isinstance(event, MpiRecvEvent):
            send_event = event.getSendEvent()
            if send_event is not None:
                send_idx = send_event.getIdx()
                if send_idx is not None:
                    # Always add dependency edge
                    self.m_predecessors[event_idx].append(send_idx)
                    if send_idx not in self.m_successors:
                        self.m_successors[send_idx] = []
                    self.m_successors[send_idx].append(event_idx)
                    
                    # Get send event's finish time
                    send_finish_time = None
                    if send_idx in self.m_earliest_finish_times:
                        send_finish_time = self.m_earliest_finish_times[send_idx]
                    elif hasattr(self, 'm_temp_finish_times') and send_idx in self.m_temp_finish_times:
                        send_finish_time = self.m_temp_finish_times[send_idx]
                    
                    if send_finish_time is not None:
                        earliest_start = max(earliest_start, send_finish_time)
        
        # Compute earliest start and finish times for this event
        earliest_finish = earliest_start + event_cost
        
        # Always track maximum earliest finish time for critical path length
        self.m_max_earliest_finish = max(self.m_max_earliest_finish, earliest_finish)
        
        # Temporarily store finish time for dependency tracking (used by next events)
        if not hasattr(self, 'm_temp_finish_times'):
            self.m_temp_finish_times = {}
        self.m_temp_finish_times[event_idx] = earliest_finish
        
        # Increment event counter for checkpointing
        self.m_event_counter += 1
        
        # Step 2: Decide whether to store earliest times based on recomputation mode
        should_store = True
        is_checkpoint = False
        
        if self.m_recomputation_mode == 'full':
            # Full recomputation with checkpoints: store every M events to limit recursion
            # This prevents RecursionError for long dependency chains
            is_checkpoint = (self.m_event_counter % self.m_checkpoint_interval == 0)
            should_store = is_checkpoint
            if is_checkpoint:
                self.m_recomputation_stats['checkpoint_events'] += 1
            else:
                self.m_recomputation_stats['recomputed_events'] += 1
        elif self.m_recomputation_mode == 'partial':
            # Partial recomputation: store only if predecessors >= threshold
            num_predecessors = len(self.m_predecessors[event_idx])
            should_store = num_predecessors >= self.m_recomputation_threshold
            if should_store:
                self.m_recomputation_stats['stored_events'] += 1
            else:
                self.m_recomputation_stats['recomputed_events'] += 1
        else:
            # Mode 'none': always store (default behavior)
            self.m_recomputation_stats['stored_events'] += 1
        
        if should_store:
            self.m_earliest_start_times[event_idx] = earliest_start
            self.m_earliest_finish_times[event_idx] = earliest_finish
        
        # Update process-local last event
        if pid is not None:
            self.m_process_last_event[pid] = event
    
    def _recompute_earliest_times(self, event_idx: int) -> Tuple[float, float]:
        """
        Recompute earliest start and finish times for an event during backward pass.
        
        This recursively recomputes the earliest times by traversing predecessors.
        Used when earliest times were not stored during forward pass (Step 2 optimization).
        
        Args:
            event_idx: Event index to recompute
            
        Returns:
            Tuple of (earliest_start_time, earliest_finish_time)
        """
        # Check if already computed and stored
        if event_idx in self.m_earliest_start_times:
            return (self.m_earliest_start_times[event_idx], 
                    self.m_earliest_finish_times[event_idx])
        
        # Get event cost
        event_cost = self.m_event_costs.get(event_idx, 0.1)
        
        # Compute earliest start time from predecessors
        earliest_start = 0.0
        if event_idx in self.m_predecessors:
            for pred_idx in self.m_predecessors[event_idx]:
                # Recursively recompute predecessor if needed
                pred_start, pred_finish = self._recompute_earliest_times(pred_idx)
                earliest_start = max(earliest_start, pred_finish)
        
        earliest_finish = earliest_start + event_cost
        return (earliest_start, earliest_finish)
    
    def _compute_latest_times(self, event: Event) -> None:
        """
        Compute latest start and finish times for each event during backward replay.
        
        This implements the backward pass of CPM where each event's latest
        finish time is the minimum of all its successors' latest start times.
        
        Memory optimization: After computing latest times for an event, we delete
        the entries for m_successors, m_earliest_start_times, and m_earliest_finish_times
        since they are no longer needed. This progressively reduces memory consumption
        during backward replay.
        
        Step 2 optimization: If earliest times were not stored (recomputation mode),
        recompute them on-the-fly during backward pass.
        
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
        # Step 2: Recompute earliest times if not stored
        if event_idx in self.m_earliest_start_times:
            earliest_start = self.m_earliest_start_times[event_idx]
        else:
            # Recompute on-the-fly
            earliest_start, _ = self._recompute_earliest_times(event_idx)
        
        latest_start = self.m_latest_start_times[event_idx]
        self.m_slack_times[event_idx] = latest_start - earliest_start
        
        # Memory optimization: Remove entries that are no longer needed after backward processing
        # These dictionaries were built during forward pass and are read-only during backward pass
        # Once we've computed the latest times for this event, we can safely delete these entries
        if event_idx in self.m_successors:
            del self.m_successors[event_idx]
        if event_idx in self.m_earliest_start_times:
            del self.m_earliest_start_times[event_idx]
        if event_idx in self.m_earliest_finish_times:
            del self.m_earliest_finish_times[event_idx]
    
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
        self.m_max_earliest_finish = 0.0
        self.m_earliest_start_times.clear()
        self.m_earliest_finish_times.clear()
        self.m_latest_start_times.clear()
        self.m_latest_finish_times.clear()
        self.m_slack_times.clear()
        self.m_predecessors.clear()
        self.m_successors.clear()
        self.m_process_last_event.clear()
        self.m_memory_stats.clear()
        self.m_memory_timeline.clear()
        self.m_detailed_memory_timeline.clear()
        # Step 2: Clear recomputation stats and temp data
        self.m_event_counter = 0
        self.m_recomputation_stats = {
            'stored_events': 0,
            'recomputed_events': 0,
            'checkpoint_events': 0
        }
        if hasattr(self, 'm_temp_finish_times'):
            self.m_temp_finish_times.clear()
    
    def getRecomputationStats(self) -> Dict[str, int]:
        """
        Get recomputation statistics (Step 2).
        
        Returns:
            Dictionary with counts of stored vs recomputed events
        """
        return self.m_recomputation_stats.copy()
    
    def _measure_memory_usage(self) -> float:
        """
        Measure current memory usage in bytes.
        
        Returns:
            Current memory usage in bytes (RSS - Resident Set Size)
        """
        process = psutil.Process()
        return process.memory_info().rss
    
    def _measure_trace_memory(self, trace: Trace) -> int:
        """
        Measure the memory consumption of a trace object.
        
        This estimates the memory size by measuring the trace object itself
        and all its events.
        
        Args:
            trace: Trace object to measure
            
        Returns:
            Estimated memory size in bytes
        """
        total_size = sys.getsizeof(trace)
        
        # Add memory for all events
        events = trace.getEvents()
        for event in events:
            total_size += sys.getsizeof(event)
            # Add size of event attributes (rough estimation)
            try:
                total_size += sys.getsizeof(event.__dict__)
            except (AttributeError, TypeError):
                pass
        
        return total_size
    
    def _measure_data_structure_memory(self, obj: Any) -> int:
        """
        Measure the memory consumption of a data structure (dict, list, etc.).
        
        This provides a more accurate measurement than sys.getsizeof alone by
        recursively measuring container contents.
        
        Args:
            obj: Object to measure (dict, list, set, etc.)
            
        Returns:
            Estimated memory size in bytes
        """
        total_size = sys.getsizeof(obj)
        
        # Add memory for dictionary contents
        if isinstance(obj, dict):
            for key, value in obj.items():
                total_size += sys.getsizeof(key)
                total_size += sys.getsizeof(value)
                # For nested structures, add their size too
                if isinstance(value, (list, dict, set)):
                    try:
                        total_size += sum(sys.getsizeof(item) for item in value) if isinstance(value, (list, set)) else 0
                    except:
                        pass
        # Add memory for list/set contents
        elif isinstance(obj, (list, set, tuple)):
            try:
                for item in obj:
                    total_size += sys.getsizeof(item)
            except:
                pass
        
        return total_size
    
    def _collect_detailed_memory_snapshot(self) -> Dict[str, float]:
        """
        Collect memory usage of individual data structures.
        
        Returns:
            Dictionary mapping variable name to memory size in bytes
        """
        snapshot = {}
        
        # Measure key data structures
        snapshot['m_event_costs'] = self._measure_data_structure_memory(self.m_event_costs)
        snapshot['m_critical_path'] = self._measure_data_structure_memory(self.m_critical_path)
        snapshot['m_earliest_start_times'] = self._measure_data_structure_memory(self.m_earliest_start_times)
        snapshot['m_earliest_finish_times'] = self._measure_data_structure_memory(self.m_earliest_finish_times)
        snapshot['m_latest_start_times'] = self._measure_data_structure_memory(self.m_latest_start_times)
        snapshot['m_latest_finish_times'] = self._measure_data_structure_memory(self.m_latest_finish_times)
        snapshot['m_slack_times'] = self._measure_data_structure_memory(self.m_slack_times)
        snapshot['m_predecessors'] = self._measure_data_structure_memory(self.m_predecessors)
        snapshot['m_successors'] = self._measure_data_structure_memory(self.m_successors)
        snapshot['m_process_last_event'] = self._measure_data_structure_memory(self.m_process_last_event)
        
        return snapshot
    
    def setEnableMemoryTracking(self, enable: bool) -> None:
        """
        Enable or disable memory tracking.
        
        Args:
            enable: True to enable memory tracking, False to disable
        """
        self.m_enable_memory_tracking = enable
    
    def setMemorySampleInterval(self, interval: int) -> None:
        """
        Set the memory sampling interval during replay.
        
        Args:
            interval: Number of events between memory samples (default: 1000)
        """
        self.m_memory_sample_interval = max(1, interval)
    
    def isMemoryTrackingEnabled(self) -> bool:
        """
        Check if memory tracking is enabled.
        
        Returns:
            True if memory tracking is enabled
        """
        return self.m_enable_memory_tracking
    
    def getMemoryStatistics(self) -> Dict[str, Any]:
        """
        Get memory consumption statistics.
        
        Returns:
            Dictionary containing memory statistics:
            - trace_memory_bytes: Memory consumed by input trace
            - forward_replay_start_memory_bytes: Memory at start of forward replay
            - forward_replay_end_memory_bytes: Memory at end of forward replay
            - forward_replay_delta_bytes: Memory change during forward replay
            - backward_replay_start_memory_bytes: Memory at start of backward replay
            - backward_replay_end_memory_bytes: Memory at end of backward replay
            - backward_replay_delta_bytes: Memory change during backward replay
            - peak_memory_bytes: Peak memory usage during analysis
        """
        return self.m_memory_stats.copy()
    
    def getMemoryTimeline(self) -> List[Tuple[str, int, float]]:
        """
        Get the memory usage timeline during replay.
        
        Returns:
            List of tuples (phase, event_count, memory_bytes) showing memory
            consumption at different points during forward and backward replay.
            Phase is either 'forward' or 'backward'.
        """
        return self.m_memory_timeline.copy()
    
    def getDetailedMemoryTimeline(self) -> List[Tuple[str, int, Dict[str, float]]]:
        """
        Get the detailed memory usage timeline for individual data structures.
        
        Returns:
            List of tuples (phase, event_count, {var_name: memory_bytes}) showing
            memory consumption of individual variables at different points during
            forward and backward replay.
        """
        return self.m_detailed_memory_timeline.copy()
    
    def plotMemoryUsage(self, output_file: str = "memory_usage.png", 
                        plot_detailed: bool = False) -> None:
        """
        Generate a plot showing memory usage during forward and backward replay.
        
        Args:
            output_file: Path to save the plot image
            plot_detailed: If True and detailed tracking enabled, plot individual variables
            
        Raises:
            ImportError: If matplotlib is not installed
        """
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            raise ImportError(
                "matplotlib is required for plotting. Install it with: pip install matplotlib"
            )
        
        if not self.m_memory_timeline and not self.m_detailed_memory_timeline:
            print("No memory timeline data available. Enable memory tracking and run analysis first.")
            return
        
        # Check if we should plot detailed view
        if plot_detailed and self.m_detailed_memory_timeline:
            self._plot_detailed_memory_usage(output_file)
        else:
            self._plot_summary_memory_usage(output_file)
    
    def _plot_summary_memory_usage(self, output_file: str) -> None:
        """Plot summary memory usage (overall process memory)."""
        import matplotlib.pyplot as plt
        
        # Separate forward and backward phases
        forward_events = []
        forward_memory = []
        backward_events = []
        backward_memory = []
        
        for phase, event_count, memory_bytes in self.m_memory_timeline:
            memory_mb = memory_bytes / (1024 * 1024)  # Convert to MB
            if phase == 'forward':
                forward_events.append(event_count)
                forward_memory.append(memory_mb)
            elif phase == 'backward':
                backward_events.append(event_count)
                backward_memory.append(memory_mb)
        
        # Create the plot
        fig, ax = plt.subplots(figsize=(12, 6))
        
        if forward_events:
            ax.plot(forward_events, forward_memory, 'b-o', label='Forward Replay', 
                   linewidth=2, markersize=4)
        
        if backward_events:
            ax.plot(backward_events, backward_memory, 'r-s', label='Backward Replay', 
                   linewidth=2, markersize=4)
        
        ax.set_xlabel('Event Count', fontsize=12)
        ax.set_ylabel('Memory Usage (MB)', fontsize=12)
        ax.set_title('Memory Consumption During Critical Path Analysis', fontsize=14, fontweight='bold')
        ax.legend(fontsize=10)
        ax.grid(True, alpha=0.3)
        
        # Add annotations for peak memory
        if forward_memory:
            peak_fwd_idx = forward_memory.index(max(forward_memory))
            ax.annotate(f'Peak: {max(forward_memory):.1f} MB',
                       xy=(forward_events[peak_fwd_idx], forward_memory[peak_fwd_idx]),
                       xytext=(10, 10), textcoords='offset points',
                       bbox=dict(boxstyle='round,pad=0.5', fc='yellow', alpha=0.7),
                       arrowprops=dict(arrowstyle='->', connectionstyle='arc3,rad=0'))
        
        plt.tight_layout()
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Memory usage plot saved to: {output_file}")
        plt.close()
    
    def _plot_detailed_memory_usage(self, output_file: str) -> None:
        """Plot detailed memory usage showing individual data structures."""
        import matplotlib.pyplot as plt
        
        if not self.m_detailed_memory_timeline:
            print("No detailed memory timeline data available.")
            return
        
        # Organize data by variable
        variables = {}
        forward_events = []
        backward_events = []
        
        for phase, event_count, mem_dict in self.m_detailed_memory_timeline:
            if phase == 'forward':
                forward_events.append(event_count)
            elif phase == 'backward':
                backward_events.append(event_count)
            
            for var_name, mem_bytes in mem_dict.items():
                if var_name not in variables:
                    variables[var_name] = {'forward_events': [], 'forward_memory': [],
                                          'backward_events': [], 'backward_memory': []}
                
                mem_mb = mem_bytes / (1024 * 1024)  # Convert to MB
                if phase == 'forward':
                    variables[var_name]['forward_events'].append(event_count)
                    variables[var_name]['forward_memory'].append(mem_mb)
                elif phase == 'backward':
                    variables[var_name]['backward_events'].append(event_count)
                    variables[var_name]['backward_memory'].append(mem_mb)
        
        # Create the plot with subplots
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
        
        # Color palette for different variables
        colors = plt.cm.tab10.colors
        markers = ['o', 's', '^', 'v', 'D', 'p', '*', 'h', 'x', '+']
        
        # Plot forward replay
        for idx, (var_name, data) in enumerate(sorted(variables.items())):
            if data['forward_events']:
                color = colors[idx % len(colors)]
                marker = markers[idx % len(markers)]
                ax1.plot(data['forward_events'], data['forward_memory'], 
                        marker=marker, color=color, label=var_name,
                        linewidth=1.5, markersize=4, alpha=0.8)
        
        ax1.set_xlabel('Event Count', fontsize=11)
        ax1.set_ylabel('Memory Usage (MB)', fontsize=11)
        ax1.set_title('Forward Replay - Memory Consumption by Data Structure', 
                     fontsize=12, fontweight='bold')
        ax1.legend(fontsize=8, loc='best', ncol=2)
        ax1.grid(True, alpha=0.3)
        
        # Plot backward replay
        for idx, (var_name, data) in enumerate(sorted(variables.items())):
            if data['backward_events']:
                color = colors[idx % len(colors)]
                marker = markers[idx % len(markers)]
                ax2.plot(data['backward_events'], data['backward_memory'], 
                        marker=marker, color=color, label=var_name,
                        linewidth=1.5, markersize=4, alpha=0.8)
        
        ax2.set_xlabel('Event Count', fontsize=11)
        ax2.set_ylabel('Memory Usage (MB)', fontsize=11)
        ax2.set_title('Backward Replay - Memory Consumption by Data Structure', 
                     fontsize=12, fontweight='bold')
        ax2.legend(fontsize=8, loc='best', ncol=2)
        ax2.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Detailed memory usage plot saved to: {output_file}")
        plt.close()
    
    def forwardReplay(self) -> None:
        """
        Replay the trace in forward order with memory tracking.
        
        If memory tracking is enabled, this method samples memory usage
        at regular intervals during replay.
        """
        if self.m_trace is None:
            return
        
        events = self.m_trace.getEvents()
        total_events = len(events)
        
        for idx, event in enumerate(events):
            # Invoke all registered forward callbacks
            for callback in self.m_forward_callbacks.values():
                callback(event)
            
            # Sample memory at intervals if tracking is enabled
            if self.m_enable_memory_tracking:
                if idx == 0 or (idx + 1) % self.m_memory_sample_interval == 0 or idx == total_events - 1:
                    memory_bytes = self._measure_memory_usage()
                    self.m_memory_timeline.append(('forward', idx + 1, memory_bytes))
                    
                    # Collect detailed memory snapshot if enabled
                    if self.m_enable_detailed_memory_tracking:
                        detailed_snapshot = self._collect_detailed_memory_snapshot()
                        self.m_detailed_memory_timeline.append(('forward', idx + 1, detailed_snapshot))
    
    def backwardReplay(self) -> None:
        """
        Replay the trace in backward order with memory tracking.
        
        If memory tracking is enabled, this method samples memory usage
        at regular intervals during replay.
        """
        if self.m_trace is None:
            return
        
        events = self.m_trace.getEvents()
        total_events = len(events)
        
        for idx, event in enumerate(reversed(events)):
            # Invoke all registered backward callbacks
            for callback in self.m_backward_callbacks.values():
                callback(event)
            
            # Sample memory at intervals if tracking is enabled
            if self.m_enable_memory_tracking:
                if idx == 0 or (idx + 1) % self.m_memory_sample_interval == 0 or idx == total_events - 1:
                    memory_bytes = self._measure_memory_usage()
                    self.m_memory_timeline.append(('backward', idx + 1, memory_bytes))
                    
                    # Collect detailed memory snapshot if enabled
                    if self.m_enable_detailed_memory_tracking:
                        detailed_snapshot = self._collect_detailed_memory_snapshot()
                        self.m_detailed_memory_timeline.append(('backward', idx + 1, detailed_snapshot))
    
    def run(self) -> None:
        """
        Execute critical path finding analysis using the Critical Path Method.
        
        This performs:
        1. Forward pass to compute earliest start/finish times
        2. Backward pass to compute latest start/finish times
        3. Slack computation and critical path identification
        4. Optional memory consumption tracking
        5. Step 2: Timing measurements for forward and backward replay
        
        Results are stored in m_critical_path and related attributes.
        """
        import time
        
        # Clear previous results
        self.clear()
        
        # Process traces from input data
        for data in self.m_inputs.get_data():
            if isinstance(data, Trace):
                self.setTrace(data)
                
                # Track memory if enabled
                if self.m_enable_memory_tracking:
                    # Measure input trace memory
                    self.m_memory_stats['trace_memory_bytes'] = self._measure_trace_memory(data)
                    
                    # Measure memory before forward replay
                    self.m_memory_stats['forward_replay_start_memory_bytes'] = self._measure_memory_usage()
                
                # Step 2: Timing - Forward pass
                forward_start_time = time.time()
                
                # Forward pass: compute earliest times and build dependency graph
                self.forwardReplay()
                
                forward_end_time = time.time()
                forward_duration = forward_end_time - forward_start_time
                self.m_memory_stats['forward_replay_time_seconds'] = forward_duration
                
                # Track memory after forward replay
                if self.m_enable_memory_tracking:
                    self.m_memory_stats['forward_replay_end_memory_bytes'] = self._measure_memory_usage()
                    self.m_memory_stats['forward_replay_delta_bytes'] = (
                        self.m_memory_stats['forward_replay_end_memory_bytes'] - 
                        self.m_memory_stats['forward_replay_start_memory_bytes']
                    )
                
                # Set critical path length (max earliest finish time)
                # Use tracked max even if we didn't store all earliest times
                if self.m_max_earliest_finish > 0:
                    self.m_critical_path_length = self.m_max_earliest_finish
                elif self.m_earliest_finish_times:
                    self.m_critical_path_length = max(self.m_earliest_finish_times.values())
                
                # Track memory before backward replay
                if self.m_enable_memory_tracking:
                    self.m_memory_stats['backward_replay_start_memory_bytes'] = self._measure_memory_usage()
                
                # Step 2: Timing - Backward pass
                backward_start_time = time.time()
                
                # Backward pass: compute latest times and slack
                self.backwardReplay()
                
                backward_end_time = time.time()
                backward_duration = backward_end_time - backward_start_time
                self.m_memory_stats['backward_replay_time_seconds'] = backward_duration
                self.m_memory_stats['total_time_seconds'] = forward_duration + backward_duration
                
                # Track memory after backward replay
                if self.m_enable_memory_tracking:
                    self.m_memory_stats['backward_replay_end_memory_bytes'] = self._measure_memory_usage()
                    self.m_memory_stats['backward_replay_delta_bytes'] = (
                        self.m_memory_stats['backward_replay_end_memory_bytes'] - 
                        self.m_memory_stats['backward_replay_start_memory_bytes']
                    )
                    
                    # Compute peak memory from timeline if available, otherwise use sampled values
                    if self.m_memory_timeline:
                        peak_memory = max(mem for _, _, mem in self.m_memory_timeline)
                        self.m_memory_stats['peak_memory_bytes'] = peak_memory
                    else:
                        memory_values = [
                            self.m_memory_stats.get('forward_replay_start_memory_bytes', 0),
                            self.m_memory_stats.get('forward_replay_end_memory_bytes', 0),
                            self.m_memory_stats.get('backward_replay_start_memory_bytes', 0),
                            self.m_memory_stats.get('backward_replay_end_memory_bytes', 0)
                        ]
                        self.m_memory_stats['peak_memory_bytes'] = max(memory_values)
                
                # Step 2: Add recomputation statistics
                self.m_memory_stats['recomputation_stats'] = self.m_recomputation_stats.copy()
                
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
