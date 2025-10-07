'''
module communication pattern analyzer
'''

from typing import Dict, List, Optional, Set, Tuple
from ..trace_analysis.low_level.trace_replayer import TraceReplayer, ReplayDirection
from ...perf_data_struct.dynamic.trace.trace import Trace
from ...perf_data_struct.dynamic.trace.event import Event
from ...perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent


'''
@class CommPatternAnalyzer
Analysis pass for identifying communication patterns in parallel applications
'''


class CommPatternAnalyzer(TraceReplayer):
    """
    CommPatternAnalyzer identifies and classifies communication patterns.
    
    Detects common patterns like:
    - Point-to-point patterns (nearest neighbor, all-to-all)
    - Collective patterns
    - Communication volume and frequency
    
    Attributes:
        m_comm_pairs: Set of (source, dest) communication pairs
        m_message_counts: Dictionary counting messages between process pairs
        m_message_sizes: Dictionary tracking message sizes
    """
    
    def __init__(self, trace: Optional[Trace] = None) -> None:
        """
        Initialize a CommPatternAnalyzer.
        
        Args:
            trace: Optional trace to analyze
        """
        super().__init__(trace)
        self.m_comm_pairs: Set[Tuple[int, int]] = set()
        self.m_message_counts: Dict[Tuple[int, int], int] = {}
        self.m_message_sizes: Dict[Tuple[int, int], List[int]] = {}
        self.m_total_messages: int = 0
        
        # Register callback
        self.registerCallback("detect_pattern", self._detect_pattern, ReplayDirection.FWD)
    
    def _detect_pattern(self, event: Event) -> None:
        """Detect communication patterns from events."""
        if isinstance(event, MpiSendEvent):
            dest = event.getDestPid()
            src = event.getPid()
            
            if src is not None and dest is not None:
                pair = (src, dest)
                self.m_comm_pairs.add(pair)
                self.m_message_counts[pair] = self.m_message_counts.get(pair, 0) + 1
                self.m_total_messages += 1
                
                # Track message size if available
                # (This would need to be extracted from event attributes)
    
    def identifyPattern(self) -> str:
        """
        Identify the dominant communication pattern.
        
        Returns:
            String describing the pattern (e.g., "nearest_neighbor", "all_to_all")
        """
        if not self.m_comm_pairs:
            return "no_communication"
        
        # Get unique processes
        processes = set()
        for src, dest in self.m_comm_pairs:
            processes.add(src)
            processes.add(dest)
        
        num_processes = len(processes)
        num_pairs = len(self.m_comm_pairs)
        
        # Check for all-to-all pattern
        expected_all_to_all = num_processes * (num_processes - 1)
        if num_pairs >= expected_all_to_all * 0.8:  # 80% threshold
            return "all_to_all"
        
        # Check for nearest neighbor
        nearest_neighbor_count = 0
        sorted_procs = sorted(processes)
        for i in range(len(sorted_procs) - 1):
            if (sorted_procs[i], sorted_procs[i+1]) in self.m_comm_pairs:
                nearest_neighbor_count += 1
            if (sorted_procs[i+1], sorted_procs[i]) in self.m_comm_pairs:
                nearest_neighbor_count += 1
        
        if nearest_neighbor_count >= len(sorted_procs) * 0.8:
            return "nearest_neighbor"
        
        # Check for hub pattern (one process communicates with many)
        max_connections = 0
        for proc in processes:
            connections = sum(1 for src, dest in self.m_comm_pairs if src == proc or dest == proc)
            max_connections = max(max_connections, connections)
        
        if max_connections >= num_processes * 0.7:
            return "hub"
        
        return "irregular"
    
    def getCommPairs(self) -> Set[Tuple[int, int]]:
        """Get all communication pairs."""
        return self.m_comm_pairs
    
    def getMessageCounts(self) -> Dict[Tuple[int, int], int]:
        """Get message counts for each pair."""
        return self.m_message_counts
    
    def getTotalMessages(self) -> int:
        """Get total number of messages."""
        return self.m_total_messages
    
    def getHeaviestCommPair(self) -> Optional[Tuple[int, int]]:
        """
        Get the communication pair with most messages.
        
        Returns:
            (source, dest) tuple
        """
        if not self.m_message_counts:
            return None
        
        return max(self.m_message_counts.items(), key=lambda x: x[1])[0]
    
    def getCommunicationMatrix(self) -> Dict[int, Dict[int, int]]:
        """
        Get communication matrix showing message counts.
        
        Returns:
            Nested dictionary: matrix[src][dest] = message_count
        """
        matrix: Dict[int, Dict[int, int]] = {}
        
        for (src, dest), count in self.m_message_counts.items():
            if src not in matrix:
                matrix[src] = {}
            matrix[src][dest] = count
        
        return matrix
    
    def clear(self) -> None:
        """Clear analysis results."""
        self.m_comm_pairs.clear()
        self.m_message_counts.clear()
        self.m_message_sizes.clear()
        self.m_total_messages = 0
    
    def run(self) -> None:
        """Execute communication pattern analysis."""
        self.clear()
        
        for data in self.m_inputs.get_data():
            if isinstance(data, Trace):
                self.setTrace(data)
                self.forwardReplay()
                
                pattern = self.identifyPattern()
                
                output = (
                    "CommPatternAnalysis",
                    data,
                    pattern,
                    len(self.m_comm_pairs),
                    self.m_total_messages
                )
                self.m_outputs.add_data(output)
