"""
Example: GPU-Accelerated Late Receiver Analysis

This example demonstrates how to use PerFlow's GPU-accelerated trace analysis
to detect late receiver patterns in MPI communication traces with high performance.
"""

from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.task.trace_analysis.gpu import GPULateReceiver, GPUAvailable


def create_sample_mpi_trace():
    """
    Create a sample MPI trace with late receiver patterns.
    
    Simulates a parallel application where receives are delayed.
    """
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0)
    trace.setTraceInfo(trace_info)
    
    # Scenario 1: Late receiver (Process 0 -> Process 1)
    # Send ready at t=2.0, receive not ready until t=5.0 (wait time = 3.0)
    send1 = MpiSendEvent(
        idx=1, name="MPI_Send", pid=0, tid=0, timestamp=2.0,
        replay_pid=0, communicator=1, tag=100, dest_pid=1
    )
    
    recv1 = MpiRecvEvent(
        idx=2, name="MPI_Recv", pid=1, tid=0, timestamp=5.0,
        replay_pid=1, communicator=1, tag=100, src_pid=0
    )
    
    send1.setRecvEvent(recv1)
    recv1.setSendEvent(send1)
    
    # Scenario 2: On-time receiver (Process 1 -> Process 0)
    # Send ready at t=8.0, receive ready at t=7.0 (no wait)
    send2 = MpiSendEvent(
        idx=3, name="MPI_Send", pid=1, tid=0, timestamp=8.0,
        replay_pid=1, communicator=1, tag=200, dest_pid=0
    )
    
    recv2 = MpiRecvEvent(
        idx=4, name="MPI_Recv", pid=0, tid=0, timestamp=7.0,
        replay_pid=0, communicator=1, tag=200, src_pid=1
    )
    
    send2.setRecvEvent(recv2)
    recv2.setSendEvent(send2)
    
    # Scenario 3: Another late receiver (Process 0 -> Process 1)
    # Send ready at t=10.0, receive ready at t=12.0 (wait time = 2.0)
    send3 = MpiSendEvent(
        idx=5, name="MPI_Send", pid=0, tid=0, timestamp=10.0,
        replay_pid=0, communicator=1, tag=300, dest_pid=1
    )
    
    recv3 = MpiRecvEvent(
        idx=6, name="MPI_Recv", pid=1, tid=0, timestamp=12.0,
        replay_pid=1, communicator=1, tag=300, src_pid=0
    )
    
    send3.setRecvEvent(recv3)
    recv3.setSendEvent(send3)
    
    # Scenario 4: Very late receiver (Process 1 -> Process 0)
    # Send ready at t=15.0, receive ready at t=21.0 (wait time = 6.0)
    send4 = MpiSendEvent(
        idx=7, name="MPI_Send", pid=1, tid=0, timestamp=15.0,
        replay_pid=1, communicator=1, tag=400, dest_pid=0
    )
    
    recv4 = MpiRecvEvent(
        idx=8, name="MPI_Recv", pid=0, tid=0, timestamp=21.0,
        replay_pid=0, communicator=1, tag=400, src_pid=1
    )
    
    send4.setRecvEvent(recv4)
    recv4.setSendEvent(send4)
    
    # Add all events to trace
    for event in [send1, recv1, send2, recv2, send3, recv3, send4, recv4]:
        trace.addEvent(event)
    
    return trace


def analyze_with_gpu(trace):
    """
    Analyze trace for late receivers using GPU acceleration.
    
    Args:
        trace: Trace object to analyze
        
    Returns:
        Dictionary with analysis results
    """
    # Create GPU-accelerated analyzer
    analyzer = GPULateReceiver(trace, use_gpu=True)
    
    # Enable shared memory optimization for better performance
    analyzer.enableSharedMemory(True)
    
    # Run analysis
    analyzer.forwardReplay()
    
    # Get results
    late_recvs = analyzer.getLateRecvs()
    wait_times = analyzer.getWaitTimes()
    total_wait = analyzer.getTotalWaitTime()
    
    return {
        "late_recv_count": len(late_recvs),
        "late_recvs": late_recvs,
        "wait_times": wait_times,
        "total_wait_time": total_wait,
        "gpu_enabled": analyzer.isGPUEnabled()
    }


def print_analysis_results(results):
    """
    Print analysis results in a readable format.
    
    Args:
        results: Dictionary with analysis results
    """
    print("=" * 70)
    print("GPU LATE RECEIVER ANALYSIS RESULTS")
    print("=" * 70)
    print(f"\nGPU Acceleration: {'ENABLED' if results['gpu_enabled'] else 'DISABLED (CPU fallback)'}")
    print(f"Total late receives detected: {results['late_recv_count']}")
    print(f"Total wait time: {results['total_wait_time']:.6f} seconds")
    
    if results['late_recv_count'] > 0:
        print("\nDetailed Late Receiver Information:")
        print("-" * 70)
        
        for i, recv_event in enumerate(results['late_recvs'], 1):
            send_event = recv_event.getSendEvent()
            recv_time = recv_event.getTimestamp()
            send_time = send_event.getTimestamp() if send_event else None
            wait_time = recv_time - send_time if (recv_time and send_time) else 0
            
            print(f"\n  Late Receive #{i}:")
            print(f"    Sender PID: {recv_event.getSrcPid()}")
            print(f"    Receiver PID: {recv_event.getPid()}")
            print(f"    Tag: {recv_event.getTag()}")
            print(f"    Send Time: {send_time:.6f}")
            print(f"    Recv Time: {recv_time:.6f}")
            print(f"    Wait Time: {wait_time:.6f} seconds")
    else:
        print("\nNo late receivers detected. All communications are well-synchronized!")
    
    print("\n" + "=" * 70)


def main():
    """
    Main function demonstrating GPU-accelerated late receiver analysis.
    """
    print("\n" + "=" * 70)
    print("PERFLOW EXAMPLE: GPU-ACCELERATED LATE RECEIVER ANALYSIS")
    print("=" * 70)
    
    # Check GPU availability
    if GPUAvailable:
        print("\n✓ GPU acceleration is AVAILABLE")
    else:
        print("\n✗ GPU acceleration is NOT AVAILABLE")
        print("  The analyzer will use CPU fallback implementation")
    
    # Step 1: Create sample trace
    print("\nStep 1: Creating sample MPI trace with late receivers...")
    trace = create_sample_mpi_trace()
    print(f"  Created trace with {trace.getEventCount()} events")
    
    # Step 2: Analyze with GPU
    print("\nStep 2: Analyzing for late receivers using GPU...")
    results = analyze_with_gpu(trace)
    
    # Step 3: Display results
    print("\nStep 3: Analysis complete!")
    print_analysis_results(results)
    
    print("\nExample complete!")
    print("\nKey Features Demonstrated:")
    print("  • GPU-accelerated parallel trace analysis")
    print("  • Late receiver detection (complement to late sender)")
    print("  • Automatic CPU fallback when GPU unavailable")
    print("  • Shared memory optimization for performance")


if __name__ == "__main__":
    main()
