"""
Example: Critical Path Finding

This example demonstrates how to use PerFlow to find the critical path in
execution traces. The critical path is the longest chain of dependent events
that determines the minimum execution time.
"""

from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
from perflow.flow.flow import FlowGraph


def create_simple_trace():
    """
    Create a simple trace with a clear critical path.
    
    This trace has 2 processes with a simple send-receive pattern.
    Process 0: compute -> send -> compute
    Process 1: compute -> recv -> compute
    
    Returns:
        Trace object with simple communication pattern
    """
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0)
    trace.setTraceInfo(trace_info)
    
    event_id = 0
    
    # Process 0: Initial computation (0.0 - 0.1)
    comp0_1 = Event(
        event_type=EventType.COMPUTE,
        idx=event_id,
        name="Compute_P0_1",
        pid=0,
        tid=0,
        timestamp=0.0
    )
    event_id += 1
    trace.addEvent(comp0_1)
    
    # Process 1: Initial computation (0.0 - 0.2) - longer
    comp1_1 = Event(
        event_type=EventType.COMPUTE,
        idx=event_id,
        name="Compute_P1_1",
        pid=1,
        tid=0,
        timestamp=0.0
    )
    event_id += 1
    trace.addEvent(comp1_1)
    
    # Process 0: Send to Process 1 (0.1)
    send = MpiSendEvent(
        idx=event_id,
        name="MPI_Send",
        pid=0,
        tid=0,
        timestamp=0.1,
        communicator=1,
        tag=100,
        dest_pid=1
    )
    event_id += 1
    trace.addEvent(send)
    
    # Process 1: Receive from Process 0 (0.2)
    recv = MpiRecvEvent(
        idx=event_id,
        name="MPI_Recv",
        pid=1,
        tid=0,
        timestamp=0.2,
        communicator=1,
        tag=100,
        src_pid=0
    )
    event_id += 1
    trace.addEvent(recv)
    
    # Match send/recv
    send.setRecvEvent(recv)
    recv.setSendEvent(send)
    
    # Process 0: Final computation (0.15)
    comp0_2 = Event(
        event_type=EventType.COMPUTE,
        idx=event_id,
        name="Compute_P0_2",
        pid=0,
        tid=0,
        timestamp=0.15
    )
    event_id += 1
    trace.addEvent(comp0_2)
    
    # Process 1: Final computation (0.25)
    comp1_2 = Event(
        event_type=EventType.COMPUTE,
        idx=event_id,
        name="Compute_P1_2",
        pid=1,
        tid=0,
        timestamp=0.25
    )
    event_id += 1
    trace.addEvent(comp1_2)
    
    return trace


def create_complex_trace():
    """
    Create a complex trace with multiple processes and communications.
    
    This simulates a more realistic parallel application with:
    - 4 processes
    - Multiple computation phases
    - Various communication patterns (pipeline, broadcast-like)
    - Different computation loads per process
    
    The critical path should go through the slowest process chain.
    
    Returns:
        Trace object with complex communication pattern
    """
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=4)
    trace.setTraceInfo(trace_info)
    
    event_id = 0
    timestamp = 0.0
    
    # Phase 1: Initial computation (different durations per process)
    print("Creating Phase 1: Initial computation...")
    comp_durations = [0.5, 1.0, 0.8, 0.6]  # Process 1 is slowest
    for pid in range(4):
        comp = Event(
            event_type=EventType.COMPUTE,
            idx=event_id,
            name=f"Phase1_Compute_P{pid}",
            pid=pid,
            tid=0,
            timestamp=timestamp
        )
        event_id += 1
        trace.addEvent(comp)
    
    # Phase 2: Pipeline communication (0 -> 1 -> 2 -> 3)
    print("Creating Phase 2: Pipeline communication...")
    pipeline_start = max(comp_durations)
    
    for src_pid in range(3):
        dest_pid = src_pid + 1
        
        # Send event
        send = MpiSendEvent(
            idx=event_id,
            name=f"Pipeline_Send_{src_pid}_to_{dest_pid}",
            pid=src_pid,
            tid=0,
            timestamp=pipeline_start + src_pid * 0.2,
            communicator=1,
            tag=100 + src_pid,
            dest_pid=dest_pid
        )
        event_id += 1
        trace.addEvent(send)
        
        # Receive event
        recv = MpiRecvEvent(
            idx=event_id,
            name=f"Pipeline_Recv_{src_pid}_to_{dest_pid}",
            pid=dest_pid,
            tid=0,
            timestamp=pipeline_start + src_pid * 0.2 + 0.1,
            communicator=1,
            tag=100 + src_pid,
            src_pid=src_pid
        )
        event_id += 1
        trace.addEvent(recv)
        
        # Match send/recv
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
    
    # Phase 3: Heavy computation on some processes
    print("Creating Phase 3: Heavy computation phase...")
    heavy_comp_start = pipeline_start + 0.7
    heavy_comp_durations = [0.3, 0.5, 1.2, 0.4]  # Process 2 is slowest
    
    for pid in range(4):
        comp = Event(
            event_type=EventType.COMPUTE,
            idx=event_id,
            name=f"Phase3_Heavy_Compute_P{pid}",
            pid=pid,
            tid=0,
            timestamp=heavy_comp_start + pid * 0.05
        )
        event_id += 1
        trace.addEvent(comp)
    
    # Phase 4: Gather-like communication (all processes send to process 0)
    print("Creating Phase 4: Gather-like communication...")
    gather_start = heavy_comp_start + max(heavy_comp_durations)
    
    for src_pid in range(1, 4):
        # Send to process 0
        send = MpiSendEvent(
            idx=event_id,
            name=f"Gather_Send_P{src_pid}_to_P0",
            pid=src_pid,
            tid=0,
            timestamp=gather_start + src_pid * 0.05,
            communicator=1,
            tag=200 + src_pid,
            dest_pid=0
        )
        event_id += 1
        trace.addEvent(send)
        
        # Receive at process 0
        recv = MpiRecvEvent(
            idx=event_id,
            name=f"Gather_Recv_P{src_pid}_to_P0",
            pid=0,
            tid=0,
            timestamp=gather_start + src_pid * 0.05 + 0.05,
            communicator=1,
            tag=200 + src_pid,
            src_pid=src_pid
        )
        event_id += 1
        trace.addEvent(recv)
        
        # Match send/recv
        send.setRecvEvent(recv)
        recv.setSendEvent(send)
    
    # Phase 5: Final computation
    print("Creating Phase 5: Final computation...")
    final_start = gather_start + 0.4
    
    for pid in range(4):
        comp = Event(
            event_type=EventType.COMPUTE,
            idx=event_id,
            name=f"Phase5_Final_Compute_P{pid}",
            pid=pid,
            tid=0,
            timestamp=final_start + pid * 0.02
        )
        event_id += 1
        trace.addEvent(comp)
    
    print(f"Created complex trace with {trace.getEventCount()} events across 4 processes")
    return trace


def analyze_critical_path(trace):
    """
    Analyze the critical path in a trace.
    
    Args:
        trace: Trace object to analyze
        
    Returns:
        CriticalPathFinding analyzer with results
    """
    analyzer = CriticalPathFinding(trace)
    
    # Forward pass: compute earliest times
    analyzer.forwardReplay()
    
    # Set critical path length
    if analyzer.m_earliest_finish_times:
        analyzer.m_critical_path_length = max(analyzer.m_earliest_finish_times.values())
    
    # Backward pass: compute latest times and slack
    analyzer.backwardReplay()
    
    # Identify critical path
    analyzer._identify_critical_path()
    
    return analyzer


def print_critical_path_summary(analyzer):
    """
    Print a summary of the critical path analysis.
    
    Args:
        analyzer: CriticalPathFinding analyzer with results
    """
    stats = analyzer.getCriticalPathStatistics()
    critical_path = analyzer.getCriticalPath()
    
    print("=" * 80)
    print("CRITICAL PATH ANALYSIS SUMMARY")
    print("=" * 80)
    
    print(f"\nCritical Path Length: {analyzer.getCriticalPathLength():.6f} seconds")
    print(f"Number of Events on Critical Path: {stats['num_events']}")
    print(f"Number of Processes Involved: {stats['num_processes']}")
    print(f"Critical Processes: {sorted(analyzer.getCriticalProcesses())}")
    
    print(f"\nTime Breakdown:")
    print(f"  Computation Time: {stats['computation_time']:.6f} seconds")
    print(f"  Communication Time: {stats['communication_time']:.6f} seconds")
    print(f"  Send Events: {stats['num_send_events']}")
    print(f"  Receive Events: {stats['num_recv_events']}")
    
    if stats['total_length'] > 0:
        comp_pct = (stats['computation_time'] / stats['total_length']) * 100
        comm_pct = (stats['communication_time'] / stats['total_length']) * 100
        print(f"  Computation %: {comp_pct:.2f}%")
        print(f"  Communication %: {comm_pct:.2f}%")
    
    # Print memory statistics if available
    if analyzer.isMemoryTrackingEnabled():
        memory_stats = analyzer.getMemoryStatistics()
        if memory_stats:
            print(f"\nMemory Consumption Statistics:")
            if 'trace_memory_bytes' in memory_stats:
                trace_mb = memory_stats['trace_memory_bytes'] / (1024 * 1024)
                print(f"  Input Trace Memory: {trace_mb:.2f} MB")
            if 'forward_replay_delta_bytes' in memory_stats:
                fwd_mb = memory_stats['forward_replay_delta_bytes'] / (1024 * 1024)
                print(f"  Forward Replay Memory Delta: {fwd_mb:.2f} MB")
            if 'backward_replay_delta_bytes' in memory_stats:
                bwd_mb = memory_stats['backward_replay_delta_bytes'] / (1024 * 1024)
                print(f"  Backward Replay Memory Delta: {bwd_mb:.2f} MB")
            if 'peak_memory_bytes' in memory_stats:
                peak_mb = memory_stats['peak_memory_bytes'] / (1024 * 1024)
                print(f"  Peak Memory Usage: {peak_mb:.2f} MB")


def print_detailed_critical_path(analyzer):
    """
    Print detailed information about events on the critical path.
    
    Args:
        analyzer: CriticalPathFinding analyzer with results
    """
    critical_path = analyzer.getCriticalPath()
    
    print("\n" + "=" * 80)
    print("DETAILED CRITICAL PATH")
    print("=" * 80)
    
    print(f"\nCritical Path Events (in order):")
    print("-" * 80)
    
    for i, event in enumerate(critical_path, 1):
        event_idx = event.getIdx()
        event_type = event.getType()
        event_name = event.getName()
        pid = event.getPid()
        timestamp = event.getTimestamp()
        
        earliest_start = analyzer.getEventEarliestStartTime(event_idx)
        earliest_finish = analyzer.getEventEarliestFinishTime(event_idx)
        latest_start = analyzer.getEventLatestStartTime(event_idx)
        latest_finish = analyzer.getEventLatestFinishTime(event_idx)
        slack = analyzer.getEventSlackTime(event_idx)
        
        print(f"\n  Event {i}:")
        print(f"    Name: {event_name}")
        print(f"    Type: {event_type.name if event_type else 'UNKNOWN'}")
        print(f"    Process: {pid}")
        print(f"    Timestamp: {timestamp:.6f}" if timestamp else "    Timestamp: N/A")
        print(f"    Earliest Start: {earliest_start:.6f}" if earliest_start is not None else "    Earliest Start: N/A")
        print(f"    Earliest Finish: {earliest_finish:.6f}" if earliest_finish is not None else "    Earliest Finish: N/A")
        print(f"    Latest Start: {latest_start:.6f}" if latest_start is not None else "    Latest Start: N/A")
        print(f"    Latest Finish: {latest_finish:.6f}" if latest_finish is not None else "    Latest Finish: N/A")
        print(f"    Slack: {slack:.6f}" if slack is not None else "    Slack: N/A")
        
        # Print communication details if applicable
        if isinstance(event, MpiSendEvent):
            print(f"    Destination: Process {event.getDestPid()}")
            print(f"    Tag: {event.getTag()}")
        elif isinstance(event, MpiRecvEvent):
            print(f"    Source: Process {event.getSrcPid()}")
            print(f"    Tag: {event.getTag()}")


def print_bottleneck_analysis(analyzer):
    """
    Print analysis of bottleneck events on the critical path.
    
    Args:
        analyzer: CriticalPathFinding analyzer with results
    """
    bottlenecks = analyzer.getBottleneckEvents(top_n=5)
    
    print("\n" + "=" * 80)
    print("BOTTLENECK ANALYSIS")
    print("=" * 80)
    
    print(f"\nTop {len(bottlenecks)} Most Time-Consuming Events on Critical Path:")
    print("-" * 80)
    
    for i, (event, cost) in enumerate(bottlenecks, 1):
        print(f"\n  Bottleneck {i}:")
        print(f"    Event: {event.getName()}")
        print(f"    Process: {event.getPid()}")
        print(f"    Cost: {cost:.6f} seconds")
        print(f"    Type: {event.getType().name if event.getType() else 'UNKNOWN'}")


def run_workflow_example(trace):
    """
    Demonstrate using FlowGraph for critical path analysis workflow.
    
    Args:
        trace: Trace object to analyze
    """
    print("\n" + "=" * 80)
    print("WORKFLOW-BASED CRITICAL PATH ANALYSIS")
    print("=" * 80)
    
    # Create workflow
    workflow = FlowGraph()
    
    # Add analysis node
    analyzer = CriticalPathFinding()
    analyzer.get_inputs().add_data(trace)
    workflow.add_node(analyzer)
    
    # Run workflow
    print("\nRunning critical path analysis workflow...")
    workflow.run()
    
    # Get results
    critical_path = analyzer.getCriticalPath()
    path_length = analyzer.getCriticalPathLength()
    
    print(f"\nAnalysis complete!")
    print(f"Critical path length: {path_length:.6f} seconds")
    print(f"Number of events on critical path: {len(critical_path)}")


def run_memory_tracking_example(trace):
    """
    Demonstrate memory tracking functionality in critical path analysis.
    
    Args:
        trace: Trace object to analyze
    """
    print("\n" + "=" * 80)
    print("MEMORY TRACKING DEMONSTRATION")
    print("=" * 80)
    
    print("\nThis example demonstrates memory consumption tracking during")
    print("critical path analysis.")
    
    # Create analyzer with memory tracking enabled
    print("\nStep 1: Creating analyzer with memory tracking enabled...")
    analyzer = CriticalPathFinding(enable_memory_tracking=True)
    analyzer.get_inputs().add_data(trace)
    
    # Run analysis
    print("Step 2: Running critical path analysis with memory tracking...")
    analyzer.run()
    
    # Get results
    critical_path = analyzer.getCriticalPath()
    path_length = analyzer.getCriticalPathLength()
    memory_stats = analyzer.getMemoryStatistics()
    
    print("\nStep 3: Analysis Results:")
    print(f"  Critical path length: {path_length:.6f} seconds")
    print(f"  Number of events on critical path: {len(critical_path)}")
    
    print("\nStep 4: Memory Consumption Results:")
    print("-" * 80)
    
    if 'trace_memory_bytes' in memory_stats:
        trace_kb = memory_stats['trace_memory_bytes'] / 1024
        trace_mb = memory_stats['trace_memory_bytes'] / (1024 * 1024)
        print(f"  Input Trace Memory: {trace_kb:.2f} KB ({trace_mb:.4f} MB)")
    
    if 'forward_replay_start_memory_bytes' in memory_stats:
        fwd_start_mb = memory_stats['forward_replay_start_memory_bytes'] / (1024 * 1024)
        print(f"  Forward Replay Start Memory: {fwd_start_mb:.2f} MB")
    
    if 'forward_replay_end_memory_bytes' in memory_stats:
        fwd_end_mb = memory_stats['forward_replay_end_memory_bytes'] / (1024 * 1024)
        print(f"  Forward Replay End Memory: {fwd_end_mb:.2f} MB")
    
    if 'forward_replay_delta_bytes' in memory_stats:
        fwd_delta_kb = memory_stats['forward_replay_delta_bytes'] / 1024
        fwd_delta_mb = memory_stats['forward_replay_delta_bytes'] / (1024 * 1024)
        print(f"  Forward Replay Memory Delta: {fwd_delta_kb:.2f} KB ({fwd_delta_mb:.4f} MB)")
    
    if 'backward_replay_start_memory_bytes' in memory_stats:
        bwd_start_mb = memory_stats['backward_replay_start_memory_bytes'] / (1024 * 1024)
        print(f"  Backward Replay Start Memory: {bwd_start_mb:.2f} MB")
    
    if 'backward_replay_end_memory_bytes' in memory_stats:
        bwd_end_mb = memory_stats['backward_replay_end_memory_bytes'] / (1024 * 1024)
        print(f"  Backward Replay End Memory: {bwd_end_mb:.2f} MB")
    
    if 'backward_replay_delta_bytes' in memory_stats:
        bwd_delta_kb = memory_stats['backward_replay_delta_bytes'] / 1024
        bwd_delta_mb = memory_stats['backward_replay_delta_bytes'] / (1024 * 1024)
        print(f"  Backward Replay Memory Delta: {bwd_delta_kb:.2f} KB ({bwd_delta_mb:.4f} MB)")
    
    if 'peak_memory_bytes' in memory_stats:
        peak_mb = memory_stats['peak_memory_bytes'] / (1024 * 1024)
        print(f"  Peak Memory Usage: {peak_mb:.2f} MB")
    
    print("\nKey Insights:")
    print("  - Memory tracking can be enabled/disabled as needed")
    print("  - Tracks memory consumption at key points in the analysis")
    print("  - Helps identify memory bottlenecks in large-scale analysis")
    print("  - Can be used to optimize memory usage in performance analysis")



def main():
    """
    Main function demonstrating critical path finding.
    """
    print("\n" + "=" * 80)
    print("PERFLOW EXAMPLE: CRITICAL PATH FINDING")
    print("=" * 80)
    print("\nThis example demonstrates finding the critical path in execution traces.")
    print("The critical path is the longest chain of dependent events that")
    print("determines the minimum possible execution time.")
    
    # Example 1: Simple trace
    print("\n" + "=" * 80)
    print("EXAMPLE 1: SIMPLE TRACE")
    print("=" * 80)
    
    print("\nStep 1: Creating simple trace...")
    simple_trace = create_simple_trace()
    print(f"  Created trace with {simple_trace.getEventCount()} events across 2 processes")
    
    print("\nStep 2: Analyzing critical path...")
    simple_analyzer = analyze_critical_path(simple_trace)
    
    print("\nStep 3: Results:")
    print_critical_path_summary(simple_analyzer)
    print_detailed_critical_path(simple_analyzer)
    
    # Example 2: Complex trace
    print("\n\n" + "=" * 80)
    print("EXAMPLE 2: COMPLEX TRACE")
    print("=" * 80)
    
    print("\nStep 1: Creating complex trace...")
    complex_trace = create_complex_trace()
    
    print("\nStep 2: Analyzing critical path...")
    complex_analyzer = analyze_critical_path(complex_trace)
    
    print("\nStep 3: Results:")
    print_critical_path_summary(complex_analyzer)
    print_detailed_critical_path(complex_analyzer)
    print_bottleneck_analysis(complex_analyzer)
    
    # Example 3: Workflow demonstration
    print("\n\n" + "=" * 80)
    print("EXAMPLE 3: WORKFLOW DEMONSTRATION")
    print("=" * 80)
    run_workflow_example(complex_trace)
    
    # Example 4: Memory tracking demonstration
    print("\n\n" + "=" * 80)
    print("EXAMPLE 4: MEMORY TRACKING DEMONSTRATION")
    print("=" * 80)
    run_memory_tracking_example(complex_trace)
    
    print("\n" + "=" * 80)
    print("ALL EXAMPLES COMPLETE!")
    print("=" * 80)
    print("\nKey Takeaways:")
    print("  - The critical path shows the minimum execution time")
    print("  - Events on the critical path are performance bottlenecks")
    print("  - Optimizing non-critical events won't improve overall performance")
    print("  - Focus optimization efforts on critical path events")
    print("  - Memory tracking helps identify resource consumption patterns")
    print()


if __name__ == "__main__":
    main()
