"""
Example: GPU-Accelerated Late Sender Analysis

This example demonstrates how to use PerFlow's GPU-accelerated trace analysis
to detect late sender patterns in MPI communication traces with high performance.
"""

from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.task.trace_analysis.gpu import GPULateSender, GPUAvailable


def create_sample_mpi_trace():
    """
    Create a sample MPI trace with communication patterns.
    
    Simulates a parallel application with multiple processes exchanging messages.
    Some messages arrive late, causing waiting time.
    """
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0)
    trace.setTraceInfo(trace_info)
    
    # Scenario 1: Late sender (Process 0 -> Process 1)
    # Receive ready at t=2.0, send arrives at t=5.0 (wait time = 3.0)
    send1 = MpiSendEvent(
        idx=1, name="MPI_Send", pid=0, tid=0, timestamp=5.0,
        replay_pid=0, communicator=1, tag=100, dest_pid=1
    )
    
    recv1 = MpiRecvEvent(
        idx=2, name="MPI_Recv", pid=1, tid=0, timestamp=2.0,
        replay_pid=1, communicator=1, tag=100, src_pid=0
    )
    
    send1.setRecvEvent(recv1)
    recv1.setSendEvent(send1)
    
    # Scenario 2: On-time sender (Process 1 -> Process 0)
    # Send arrives at t=7.0, receive ready at t=8.0 (no wait)
    send2 = MpiSendEvent(
        idx=3, name="MPI_Send", pid=1, tid=0, timestamp=7.0,
        replay_pid=1, communicator=1, tag=200, dest_pid=0
    )
    
    recv2 = MpiRecvEvent(
        idx=4, name="MPI_Recv", pid=0, tid=0, timestamp=8.0,
        replay_pid=0, communicator=1, tag=200, src_pid=1
    )
    
    send2.setRecvEvent(recv2)
    recv2.setSendEvent(send2)
    
    # Scenario 3: Another late sender (Process 0 -> Process 1)
    # Receive ready at t=10.0, send arrives at t=12.0 (wait time = 2.0)
    send3 = MpiSendEvent(
        idx=5, name="MPI_Send", pid=0, tid=0, timestamp=12.0,
        replay_pid=0, communicator=1, tag=300, dest_pid=1
    )
    
    recv3 = MpiRecvEvent(
        idx=6, name="MPI_Recv", pid=1, tid=0, timestamp=10.0,
        replay_pid=1, communicator=1, tag=300, src_pid=0
    )
    
    send3.setRecvEvent(recv3)
    recv3.setSendEvent(send3)
    
    # Scenario 4: Very late sender (Process 1 -> Process 0)
    # Receive ready at t=15.0, send arrives at t=20.0 (wait time = 5.0)
    send4 = MpiSendEvent(
        idx=7, name="MPI_Send", pid=1, tid=0, timestamp=20.0,
        replay_pid=1, communicator=1, tag=400, dest_pid=0
    )
    
    recv4 = MpiRecvEvent(
        idx=8, name="MPI_Recv", pid=0, tid=0, timestamp=15.0,
        replay_pid=0, communicator=1, tag=400, src_pid=1
    )
    
    send4.setRecvEvent(recv4)
    recv4.setSendEvent(send4)
    
    # Add all events to trace
    for event in [recv1, send1, send2, recv2, recv3, send3, recv4, send4]:
        trace.addEvent(event)
    
    return trace


def analyze_with_gpu(trace):
    """
    Analyze trace for late senders using GPU acceleration.
    
    Args:
        trace: Trace object to analyze
        
    Returns:
        Dictionary with analysis results
    """
    # Create GPU-accelerated analyzer
    analyzer = GPULateSender(trace, use_gpu=True)
    
    # Enable shared memory optimization for better performance
    analyzer.enableSharedMemory(True)
    
    # Run analysis
    analyzer.forwardReplay()
    
    # Get results
    late_sends = analyzer.getLateSends()
    wait_times = analyzer.getWaitTimes()
    total_wait = analyzer.getTotalWaitTime()
    
    return {
        "late_send_count": len(late_sends),
        "late_sends": late_sends,
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
    print("GPU LATE SENDER ANALYSIS RESULTS")
    print("=" * 70)
    print(f"\nGPU Acceleration: {'ENABLED' if results['gpu_enabled'] else 'DISABLED (CPU fallback)'}")
    print(f"Total late sends detected: {results['late_send_count']}")
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


def compare_performance():
    """
    Compare GPU vs CPU performance for late sender analysis.
    
    Note: This is a demonstration. Real performance gains are visible
    with larger traces (thousands to millions of events).
    """
    import time
    
    print("\n" + "=" * 70)
    print("PERFORMANCE COMPARISON: GPU vs CPU")
    print("=" * 70)
    
    # Create trace
    trace = create_sample_mpi_trace()
    print(f"\nTrace size: {trace.getEventCount()} events")
    
    # GPU analysis
    print("\n1. GPU-Accelerated Analysis:")
    start = time.time()
    gpu_results = analyze_with_gpu(trace)
    gpu_time = time.time() - start
    print(f"   Time: {gpu_time*1000:.3f} ms")
    print(f"   Late sends found: {gpu_results['late_send_count']}")
    
    # CPU analysis (import regular LateSender)
    from perflow.task.trace_analysis.late_sender import LateSender
    
    print("\n2. CPU Analysis:")
    start = time.time()
    cpu_analyzer = LateSender(trace)
    cpu_analyzer.forwardReplay()
    cpu_time = time.time() - start
    cpu_late_sends = len(cpu_analyzer.getLateSends())
    print(f"   Time: {cpu_time*1000:.3f} ms")
    print(f"   Late sends found: {cpu_late_sends}")
    
    # Compare
    if gpu_results['gpu_enabled']:
        speedup = cpu_time / gpu_time if gpu_time > 0 else 1.0
        print(f"\n3. Speedup: {speedup:.2f}x")
        print("   Note: GPU benefits are more significant with larger traces")
    else:
        print("\n3. GPU not available - both used CPU implementation")
    
    print("=" * 70)


def main():
    """
    Main function demonstrating GPU-accelerated late sender analysis.
    """
    print("\n" + "=" * 70)
    print("PERFLOW EXAMPLE: GPU-ACCELERATED LATE SENDER ANALYSIS")
    print("=" * 70)
    
    # Check GPU availability
    if GPUAvailable:
        print("\n✓ GPU acceleration is AVAILABLE")
    else:
        print("\n✗ GPU acceleration is NOT AVAILABLE")
        print("  The analyzer will use CPU fallback implementation")
    
    # Step 1: Create sample trace
    print("\nStep 1: Creating sample MPI trace...")
    trace = create_sample_mpi_trace()
    print(f"  Created trace with {trace.getEventCount()} events")
    
    # Step 2: Analyze with GPU
    print("\nStep 2: Analyzing for late senders using GPU...")
    results = analyze_with_gpu(trace)
    
    # Step 3: Display results
    print("\nStep 3: Analysis complete!")
    print_analysis_results(results)
    
    # Step 4: Performance comparison
    compare_performance()
    
    print("\nExample complete!")
    print("\nKey Features Demonstrated:")
    print("  • GPU-accelerated parallel trace analysis")
    print("  • Automatic CPU fallback when GPU unavailable")
    print("  • Shared memory optimization for performance")
    print("  • Identical results to CPU implementation")


if __name__ == "__main__":
    main()
