"""
Example: Late Sender Analysis

This example demonstrates how to use PerFlow to detect late sender patterns
in MPI communication traces. Late senders occur when a receive operation is
ready but must wait for the corresponding send to arrive.
"""

from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.task.trace_analysis.late_sender import LateSender
from perflow.task.reporter.timeline_viewer import TimelineViewer
from perflow.flow.flow import FlowGraph


def create_sample_mpi_trace():
    """
    Create a sample MPI trace with communication patterns.
    
    Simulates a simple parallel application with 2 processes
    exchanging messages. Some messages arrive late.
    """
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0)
    trace.setTraceInfo(trace_info)
    
    # Scenario 1: Process 0 sends to Process 1 (late sender)
    # Receive is ready at t=2.0, but send doesn't arrive until t=5.0
    send1 = MpiSendEvent(
        event_type=EventType.SEND,
        idx=1,
        name="MPI_Send",
        pid=0,
        tid=0,
        timestamp=5.0,
        replay_pid=0,
        communicator=1,
        tag=100,
        dest_pid=1
    )
    
    recv1 = MpiRecvEvent(
        event_type=EventType.RECV,
        idx=2,
        name="MPI_Recv",
        pid=1,
        tid=0,
        timestamp=2.0,
        replay_pid=1,
        communicator=1,
        tag=100,
        src_pid=0
    )
    
    # Match the events
    send1.setRecvEvent(recv1)
    recv1.setSendEvent(send1)
    
    # Scenario 2: Process 1 sends to Process 0 (on time)
    # Send arrives at t=7.0, receive is ready at t=8.0
    send2 = MpiSendEvent(
        event_type=EventType.SEND,
        idx=3,
        name="MPI_Send",
        pid=1,
        tid=0,
        timestamp=7.0,
        replay_pid=1,
        communicator=1,
        tag=200,
        dest_pid=0
    )
    
    recv2 = MpiRecvEvent(
        event_type=EventType.RECV,
        idx=4,
        name="MPI_Recv",
        pid=0,
        tid=0,
        timestamp=8.0,
        replay_pid=0,
        communicator=1,
        tag=200,
        src_pid=1
    )
    
    send2.setRecvEvent(recv2)
    recv2.setSendEvent(send2)
    
    # Scenario 3: Another late sender
    # Receive ready at t=10.0, send arrives at t=12.0
    send3 = MpiSendEvent(
        event_type=EventType.SEND,
        idx=5,
        name="MPI_Send",
        pid=0,
        tid=0,
        timestamp=12.0,
        replay_pid=0,
        communicator=1,
        tag=300,
        dest_pid=1
    )
    
    recv3 = MpiRecvEvent(
        event_type=EventType.RECV,
        idx=6,
        name="MPI_Recv",
        pid=1,
        tid=0,
        timestamp=10.0,
        replay_pid=1,
        communicator=1,
        tag=300,
        src_pid=0
    )
    
    send3.setRecvEvent(recv3)
    recv3.setSendEvent(send3)
    
    # Add all events to trace
    for event in [recv1, send1, send2, recv2, recv3, send3]:
        trace.addEvent(event)
    
    return trace


def analyze_late_senders(trace):
    """
    Analyze trace for late sender patterns.
    
    Args:
        trace: Trace object to analyze
        
    Returns:
        Dictionary with analysis results
    """
    analyzer = LateSender(trace)
    analyzer.forwardReplay()
    
    late_sends = analyzer.getLateSends()
    wait_times = analyzer.getWaitTimes()
    total_wait = analyzer.getTotalWaitTime()
    
    return {
        "late_send_count": len(late_sends),
        "late_sends": late_sends,
        "wait_times": wait_times,
        "total_wait_time": total_wait
    }


def print_analysis_results(results):
    """
    Print analysis results in a readable format.
    
    Args:
        results: Dictionary with analysis results
    """
    print("=" * 70)
    print("LATE SENDER ANALYSIS RESULTS")
    print("=" * 70)
    print(f"\nTotal late sends detected: {results['late_send_count']}")
    print(f"Total wait time: {results['total_wait_time']:.6f} seconds")
    
    if results['late_send_count'] > 0:
        print("\nDetailed Late Sender Information:")
        print("-" * 70)
        
        for i, send_event in enumerate(results['late_sends'], 1):
            recv_event = send_event.getRecvEvent()
            send_time = send_event.getTimestamp()
            recv_time = recv_event.getTimestamp() if recv_event else None
            wait_time = send_time - recv_time if (send_time and recv_time) else 0
            
            print(f"\n  Late Send #{i}:")
            print(f"    Sender PID: {send_event.getPid()}")
            print(f"    Receiver PID: {send_event.getDestPid()}")
            print(f"    Tag: {send_event.getTag()}")
            print(f"    Send Time: {send_time:.6f}")
            print(f"    Recv Ready Time: {recv_time:.6f}")
            print(f"    Wait Time: {wait_time:.6f} seconds")
    else:
        print("\nNo late senders detected. All communications are well-synchronized!")
    
    print("\n" + "=" * 70)


def run_workflow_example():
    """
    Demonstrate using FlowGraph for late sender analysis workflow.
    """
    print("\n" + "=" * 70)
    print("WORKFLOW-BASED LATE SENDER ANALYSIS")
    print("=" * 70)
    
    # Create trace
    trace = create_sample_mpi_trace()
    
    # Create workflow
    workflow = FlowGraph()
    
    # Add analysis node
    analyzer = LateSender()
    analyzer.get_inputs().add_data(trace)
    
    # Add visualization node
    viewer = TimelineViewer("text")
    
    # Build pipeline: analyzer -> viewer
    workflow.add_edge(analyzer, viewer)
    
    # Run workflow
    print("\nRunning analysis workflow...")
    workflow.run()
    
    # Get results
    print(f"\nAnalysis complete!")
    print(f"Late sends found: {len(analyzer.getLateSends())}")
    print(f"Total wait time: {analyzer.getTotalWaitTime():.6f} seconds")


def main():
    """
    Main function demonstrating late sender analysis.
    """
    print("\n" + "=" * 70)
    print("PERFLOW EXAMPLE: LATE SENDER ANALYSIS")
    print("=" * 70)
    print("\nThis example demonstrates detecting late sender patterns")
    print("in MPI communication traces.")
    
    # Step 1: Create sample trace
    print("\nStep 1: Creating sample MPI trace...")
    trace = create_sample_mpi_trace()
    print(f"  Created trace with {trace.getEventCount()} events")
    
    # Step 2: Analyze for late senders
    print("\nStep 2: Analyzing for late senders...")
    results = analyze_late_senders(trace)
    
    # Step 3: Display results
    print("\nStep 3: Analysis complete!")
    print_analysis_results(results)
    
    # Step 4: Demonstrate workflow
    run_workflow_example()
    
    print("\nExample complete!")


if __name__ == "__main__":
    main()
