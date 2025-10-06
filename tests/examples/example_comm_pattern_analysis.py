"""
Example: Communication Pattern Analysis

This example demonstrates how to analyze communication patterns in traces,
including message volumes, communication frequency, and process interactions.
"""

from collections import defaultdict
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.task.trace_analysis.low_level.trace_replayer import TraceReplayer


def create_communication_trace(num_processes=4, comm_rounds=3):
    """
    Create a trace with various communication patterns.
    
    Args:
        num_processes: Number of processes
        comm_rounds: Number of communication rounds
        
    Returns:
        Trace object with communication events
    """
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0)
    trace.setTraceInfo(trace_info)
    
    event_id = 0
    timestamp = 0.0
    
    for round_num in range(comm_rounds):
        # Each process sends to the next process (ring pattern)
        for src_pid in range(num_processes):
            dest_pid = (src_pid + 1) % num_processes
            
            send = MpiSendEvent(
                event_type=EventType.SEND,
                idx=event_id,
                name=f"MPI_Send_R{round_num}",
                pid=src_pid,
                tid=0,
                timestamp=timestamp,
                communicator=1,
                tag=round_num * 100 + src_pid,
                dest_pid=dest_pid
            )
            event_id += 1
            timestamp += 0.1
            
            recv = MpiRecvEvent(
                event_type=EventType.RECV,
                idx=event_id,
                name=f"MPI_Recv_R{round_num}",
                pid=dest_pid,
                tid=0,
                timestamp=timestamp,
                communicator=1,
                tag=round_num * 100 + src_pid,
                src_pid=src_pid
            )
            event_id += 1
            timestamp += 0.1
            
            send.setRecvEvent(recv)
            recv.setSendEvent(send)
            
            trace.addEvent(send)
            trace.addEvent(recv)
    
    return trace


class CommunicationPatternAnalyzer:
    """Analyzer for communication patterns"""
    
    def __init__(self):
        self.send_counts = defaultdict(int)  # Per-process send count
        self.recv_counts = defaultdict(int)  # Per-process receive count
        self.comm_matrix = defaultdict(lambda: defaultdict(int))  # src -> dest counts
        self.message_sizes = []  # Message sizes (if available)
        self.total_messages = 0
    
    def analyze_event(self, event):
        """Callback to analyze communication events"""
        if isinstance(event, MpiSendEvent):
            src_pid = event.getPid()
            dest_pid = event.getDestPid()
            
            self.send_counts[src_pid] += 1
            self.comm_matrix[src_pid][dest_pid] += 1
            self.total_messages += 1
        
        elif isinstance(event, MpiRecvEvent):
            dest_pid = event.getPid()
            self.recv_counts[dest_pid] += 1
    
    def get_results(self):
        """Get analysis results"""
        return {
            "total_messages": self.total_messages,
            "send_counts": dict(self.send_counts),
            "recv_counts": dict(self.recv_counts),
            "comm_matrix": {src: dict(dests) for src, dests in self.comm_matrix.items()}
        }


def analyze_communication_patterns(trace):
    """
    Analyze communication patterns in trace.
    
    Args:
        trace: Trace object to analyze
        
    Returns:
        Dictionary with communication pattern analysis results
    """
    analyzer = CommunicationPatternAnalyzer()
    
    replayer = TraceReplayer(trace)
    replayer.registerCallback("comm_analyzer", analyzer.analyze_event)
    replayer.forwardReplay()
    
    return analyzer.get_results()


def print_communication_analysis(results):
    """
    Print communication pattern analysis results.
    
    Args:
        results: Dictionary with analysis results
    """
    print("=" * 70)
    print("COMMUNICATION PATTERN ANALYSIS")
    print("=" * 70)
    
    print(f"\nTotal Messages: {results['total_messages']}")
    
    print("\nPer-Process Send Counts:")
    print("-" * 40)
    for pid, count in sorted(results['send_counts'].items()):
        print(f"  Process {pid}: {count} sends")
    
    print("\nPer-Process Receive Counts:")
    print("-" * 40)
    for pid, count in sorted(results['recv_counts'].items()):
        print(f"  Process {pid}: {count} receives")
    
    print("\nCommunication Matrix (src -> dest):")
    print("-" * 40)
    for src_pid, dests in sorted(results['comm_matrix'].items()):
        for dest_pid, count in sorted(dests.items()):
            print(f"  Process {src_pid} -> Process {dest_pid}: {count} messages")
    
    # Detect patterns
    print("\nDetected Communication Patterns:")
    print("-" * 40)
    
    # Check if it's a ring pattern
    is_ring = all(
        len(dests) == 1 and list(dests.keys())[0] == (src + 1) % len(results['send_counts'])
        for src, dests in results['comm_matrix'].items()
    )
    
    if is_ring:
        print("  ✓ Ring communication pattern detected")
    
    # Check for all-to-all
    num_procs = len(results['send_counts'])
    is_all_to_all = all(
        len(dests) == num_procs - 1
        for dests in results['comm_matrix'].values()
    )
    
    if is_all_to_all:
        print("  ✓ All-to-all communication pattern detected")
    
    print("\n" + "=" * 70)


def main():
    """
    Main function demonstrating communication pattern analysis.
    """
    print("\n" + "=" * 70)
    print("PERFLOW EXAMPLE: COMMUNICATION PATTERN ANALYSIS")
    print("=" * 70)
    print("\nThis example analyzes communication patterns in MPI traces.")
    
    # Create trace with ring communication pattern
    print("\nCreating trace with ring communication pattern...")
    trace = create_communication_trace(num_processes=4, comm_rounds=3)
    print(f"  Created trace with {trace.getEventCount()} events")
    print(f"  4 processes, 3 communication rounds")
    
    # Analyze communication patterns
    print("\nAnalyzing communication patterns...")
    results = analyze_communication_patterns(trace)
    
    # Display results
    print_communication_analysis(results)
    
    print("\nExample complete!")


if __name__ == "__main__":
    main()
