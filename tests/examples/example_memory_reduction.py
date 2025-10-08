"""
Example: Memory Reduction in Critical Path Analysis

This example demonstrates the memory optimization in critical path analysis
where m_successors, m_earliest_start_times, and m_earliest_finish_times are
progressively cleaned up during backward replay to reduce memory consumption.

The optimization is particularly important for large-scale traces where memory
usage can grow significantly during analysis.
"""

from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
import sys


def create_large_trace(num_processes=16, num_iterations=5000):
    """
    Create a large trace to demonstrate memory reduction.
    
    This creates a realistic trace with:
    - Multiple processes
    - Multiple iterations with computation and communication
    - Various communication patterns (pipeline, gather, scatter)
    
    Args:
        num_processes: Number of processes in the trace
        num_iterations: Number of computation-communication iterations
        
    Returns:
        Trace object with large memory footprint
    """
    print(f"\n{'='*80}")
    print("CREATING LARGE-SCALE TRACE FOR MEMORY REDUCTION DEMONSTRATION")
    print(f"{'='*80}")
    print(f"\nConfiguration:")
    print(f"  Number of processes: {num_processes}")
    print(f"  Number of iterations: {num_iterations:,}")
    
    estimated_events = num_processes * num_iterations * 4
    estimated_mb = (estimated_events * 344) / (1024 * 1024)
    print(f"  Estimated total events: {estimated_events:,}")
    print(f"  Estimated memory consumption: ~{estimated_mb:.1f} MB")
    print(f"\nGenerating trace...")
    
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=num_processes)
    trace.setTraceInfo(trace_info)
    
    event_id = 0
    timestamp = 0.0
    
    for iteration in range(num_iterations):
        if iteration % 500 == 0:
            print(f"  Progress: {iteration:,}/{num_iterations:,} iterations ({100*iteration/num_iterations:.1f}%)")
        
        current_time = timestamp + iteration * 0.001
        
        # Phase 1: Local computation on all processes
        for pid in range(num_processes):
            comp = Event(
                event_type=EventType.COMPUTE,
                idx=event_id,
                name=f"Compute_Iter{iteration}_P{pid}",
                pid=pid,
                tid=0,
                timestamp=current_time
            )
            event_id += 1
            trace.addEvent(comp)
        
        # Phase 2: Communication pattern (varies by iteration)
        comm_time = current_time + 0.0001
        
        # Every 10 iterations: Pipeline communication
        if iteration % 10 == 0:
            for src_pid in range(num_processes - 1):
                dest_pid = src_pid + 1
                
                send = MpiSendEvent(
                    idx=event_id,
                    name=f"Pipeline_Send_I{iteration}_P{src_pid}",
                    pid=src_pid,
                    tid=0,
                    timestamp=comm_time + src_pid * 0.00001,
                    communicator=1,
                    tag=iteration * 1000 + src_pid,
                    dest_pid=dest_pid
                )
                event_id += 1
                trace.addEvent(send)
                
                recv = MpiRecvEvent(
                    idx=event_id,
                    name=f"Pipeline_Recv_I{iteration}_P{dest_pid}",
                    pid=dest_pid,
                    tid=0,
                    timestamp=comm_time + src_pid * 0.00001 + 0.000005,
                    communicator=1,
                    tag=iteration * 1000 + src_pid,
                    src_pid=src_pid
                )
                event_id += 1
                trace.addEvent(recv)
                
                send.setRecvEvent(recv)
                recv.setSendEvent(send)
        
        # Every 25 iterations: Gather pattern
        elif iteration % 25 == 0:
            for src_pid in range(1, num_processes):
                send = MpiSendEvent(
                    idx=event_id,
                    name=f"Gather_Send_I{iteration}_P{src_pid}",
                    pid=src_pid,
                    tid=0,
                    timestamp=comm_time + src_pid * 0.000001,
                    communicator=1,
                    tag=iteration * 1000 + src_pid,
                    dest_pid=0
                )
                event_id += 1
                trace.addEvent(send)
                
                recv = MpiRecvEvent(
                    idx=event_id,
                    name=f"Gather_Recv_I{iteration}_P0",
                    pid=0,
                    tid=0,
                    timestamp=comm_time + src_pid * 0.000001 + 0.000002,
                    communicator=1,
                    tag=iteration * 1000 + src_pid,
                    src_pid=src_pid
                )
                event_id += 1
                trace.addEvent(recv)
                
                send.setRecvEvent(recv)
                recv.setSendEvent(send)
        
        # Phase 3: Post-communication computation
        post_comm_time = comm_time + 0.0002
        for pid in range(num_processes):
            comp = Event(
                event_type=EventType.COMPUTE,
                idx=event_id,
                name=f"PostComm_Compute_I{iteration}_P{pid}",
                pid=pid,
                tid=0,
                timestamp=post_comm_time
            )
            event_id += 1
            trace.addEvent(comp)
    
    actual_events = trace.getEventCount()
    actual_mb = (actual_events * 344) / (1024 * 1024)
    
    print(f"  Progress: {num_iterations:,}/{num_iterations:,} iterations (100.0%)")
    print(f"\n{'='*80}")
    print(f"Trace Generation Complete!")
    print(f"{'='*80}")
    print(f"  Total events created: {actual_events:,}")
    print(f"  Estimated trace memory: ~{actual_mb:.1f} MB")
    print(f"{'='*80}\n")
    
    return trace


def demonstrate_memory_reduction(trace):
    """
    Demonstrate memory reduction during critical path analysis.
    
    This function runs the critical path analysis with detailed memory tracking
    enabled and shows how memory is progressively freed during backward replay.
    
    Args:
        trace: Trace object to analyze
    """
    print(f"\n{'='*80}")
    print("MEMORY REDUCTION DEMONSTRATION")
    print(f"{'='*80}")
    
    # Create analyzer with detailed memory tracking
    print("\nStep 1: Creating analyzer with detailed memory tracking...")
    analyzer = CriticalPathFinding(enable_memory_tracking=True,
                                  enable_detailed_memory_tracking=True)
    analyzer.setMemorySampleInterval(2500)  # Sample every 2500 events
    analyzer.get_inputs().add_data(trace)
    
    print("Step 2: Running critical path analysis with memory tracking...")
    print("  (This may take a few seconds...)")
    analyzer.run()
    
    print("\nStep 3: Analysis Complete!")
    
    # Get results
    critical_path = analyzer.getCriticalPath()
    path_length = analyzer.getCriticalPathLength()
    memory_stats = analyzer.getMemoryStatistics()
    detailed_timeline = analyzer.getDetailedMemoryTimeline()
    
    print(f"\nCritical Path Results:")
    print(f"  Critical path length: {path_length:.6f} seconds")
    print(f"  Number of events on critical path: {len(critical_path)}")
    
    # Display memory statistics
    print(f"\n{'='*80}")
    print("MEMORY CONSUMPTION ANALYSIS")
    print(f"{'='*80}")
    
    if memory_stats:
        if 'trace_memory_bytes' in memory_stats:
            trace_mb = memory_stats['trace_memory_bytes'] / (1024 * 1024)
            print(f"\nInput Trace Memory: {trace_mb:.2f} MB")
        
        print(f"\nForward Replay (Building Dependencies):")
        if 'forward_replay_start_memory_bytes' in memory_stats:
            fwd_start_mb = memory_stats['forward_replay_start_memory_bytes'] / (1024 * 1024)
            print(f"  Start Memory: {fwd_start_mb:.2f} MB")
        
        if 'forward_replay_end_memory_bytes' in memory_stats:
            fwd_end_mb = memory_stats['forward_replay_end_memory_bytes'] / (1024 * 1024)
            print(f"  End Memory: {fwd_end_mb:.2f} MB")
        
        if 'forward_replay_delta_bytes' in memory_stats:
            fwd_delta_mb = memory_stats['forward_replay_delta_bytes'] / (1024 * 1024)
            print(f"  Memory Growth: {fwd_delta_mb:.2f} MB")
        
        print(f"\nBackward Replay (Computing Latest Times + Cleanup):")
        if 'backward_replay_start_memory_bytes' in memory_stats:
            bwd_start_mb = memory_stats['backward_replay_start_memory_bytes'] / (1024 * 1024)
            print(f"  Start Memory: {bwd_start_mb:.2f} MB")
        
        if 'backward_replay_end_memory_bytes' in memory_stats:
            bwd_end_mb = memory_stats['backward_replay_end_memory_bytes'] / (1024 * 1024)
            print(f"  End Memory: {bwd_end_mb:.2f} MB")
        
        if 'backward_replay_delta_bytes' in memory_stats:
            bwd_delta_mb = memory_stats['backward_replay_delta_bytes'] / (1024 * 1024)
            print(f"  Memory Change: {bwd_delta_mb:+.2f} MB")
            if bwd_delta_mb < 0:
                print(f"  ✓ Memory REDUCED during backward replay!")
        
        if 'peak_memory_bytes' in memory_stats:
            peak_mb = memory_stats['peak_memory_bytes'] / (1024 * 1024)
            print(f"\nPeak Memory Usage: {peak_mb:.2f} MB")
    
    # Analyze detailed memory timeline
    if detailed_timeline:
        print(f"\n{'='*80}")
        print("DETAILED MEMORY REDUCTION ANALYSIS")
        print(f"{'='*80}")
        
        # Find forward and backward samples
        forward_samples = [s for s in detailed_timeline if s[0] == 'forward']
        backward_samples = [s for s in detailed_timeline if s[0] == 'backward']
        
        if forward_samples and backward_samples:
            # Get peak size during forward pass for cleaned-up structures
            peak_forward = forward_samples[-1][2]
            final_backward = backward_samples[-1][2]
            
            print(f"\nMemory Reduction for Cleaned-up Data Structures:")
            print(f"{'':50s} {'Peak (Forward)':<15s} {'Final (Backward)':<15s} {'Reduction':<15s}")
            print("-" * 80)
            
            for var_name in ['m_successors', 'm_earliest_start_times', 'm_earliest_finish_times']:
                peak_kb = peak_forward[var_name] / 1024
                final_kb = final_backward[var_name] / 1024
                reduction_kb = peak_kb - final_kb
                reduction_pct = (reduction_kb / peak_kb * 100) if peak_kb > 0 else 0
                
                print(f"  {var_name:46s} {peak_kb:12.2f} KB  {final_kb:12.2f} KB  "
                      f"{reduction_kb:9.2f} KB ({reduction_pct:5.1f}%)")
            
            # Calculate total memory saved
            total_peak = sum(peak_forward[v] for v in ['m_successors', 'm_earliest_start_times', 'm_earliest_finish_times'])
            total_final = sum(final_backward[v] for v in ['m_successors', 'm_earliest_start_times', 'm_earliest_finish_times'])
            total_saved_kb = (total_peak - total_final) / 1024
            total_saved_pct = (total_saved_kb / (total_peak / 1024) * 100) if total_peak > 0 else 0
            
            print("-" * 80)
            print(f"  {'TOTAL MEMORY SAVED':46s} {total_peak/1024:12.2f} KB  {total_final/1024:12.2f} KB  "
                  f"{total_saved_kb:9.2f} KB ({total_saved_pct:5.1f}%)")
            
            print(f"\nMemory Growth for Backward-pass Data Structures:")
            print(f"{'':50s} {'Start (Forward)':<15s} {'Final (Backward)':<15s} {'Growth':<15s}")
            print("-" * 80)
            
            for var_name in ['m_latest_start_times', 'm_latest_finish_times', 'm_slack_times']:
                start_kb = forward_samples[0][2][var_name] / 1024
                final_kb = final_backward[var_name] / 1024
                growth_kb = final_kb - start_kb
                
                print(f"  {var_name:46s} {start_kb:12.2f} KB  {final_kb:12.2f} KB  "
                      f"{growth_kb:+9.2f} KB")
    
    # Generate visualizations
    print(f"\n{'='*80}")
    print("GENERATING VISUALIZATIONS")
    print(f"{'='*80}")
    
    print("\n1. Generating overall memory usage plot...")
    try:
        analyzer.plotMemoryUsage("memory_reduction_summary.png")
        print("   ✓ Saved: memory_reduction_summary.png")
    except ImportError as e:
        print(f"   ✗ Could not generate plot: {e}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print("\n2. Generating detailed memory plot (by data structure)...")
    try:
        analyzer.plotMemoryUsage("memory_reduction_detailed.png", plot_detailed=True)
        print("   ✓ Saved: memory_reduction_detailed.png")
    except ImportError as e:
        print(f"   ✗ Could not generate plot: {e}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print(f"\n{'='*80}")
    print("KEY INSIGHTS")
    print(f"{'='*80}")
    print("\n✓ Memory Optimization Strategy:")
    print("  During forward replay: Build dependency graph and compute earliest times")
    print("  During backward replay: Compute latest times AND clean up forward-pass data")
    print("")
    print("✓ Three Data Structures Cleaned Up:")
    print("  - m_successors: Dependency edges (no longer needed after reading)")
    print("  - m_earliest_start_times: Only needed for slack calculation")
    print("  - m_earliest_finish_times: Only needed during forward pass")
    print("")
    print("✓ Benefits:")
    print("  - Progressive memory reduction during backward replay")
    print("  - Lower peak memory consumption")
    print("  - Enables analysis of larger traces on single-node systems")
    print("")
    print("✓ Implementation:")
    print("  - Minimal code changes (added deletion after reading)")
    print("  - No impact on correctness or performance")
    print("  - Backward compatible with existing code")


def main():
    """Main function demonstrating memory reduction."""
    print("\n" + "=" * 80)
    print("PERFLOW EXAMPLE: MEMORY REDUCTION IN CRITICAL PATH ANALYSIS")
    print("=" * 80)
    print("\nThis example demonstrates Step 1 of memory optimization:")
    print("Removing unnecessary memory during backward replay.")
    print("\nThe optimization focuses on three data structures:")
    print("  1. m_successors")
    print("  2. m_earliest_start_times")
    print("  3. m_earliest_finish_times")
    print("\nThese structures are built during forward replay but only read")
    print("during backward replay. By deleting entries as they're used,")
    print("we progressively reduce memory consumption.")
    
    # Create a large trace for demonstration
    trace = create_large_trace(num_processes=16, num_iterations=5000)
    
    # Demonstrate memory reduction
    demonstrate_memory_reduction(trace)
    
    print("\n" + "=" * 80)
    print("EXAMPLE COMPLETE!")
    print("=" * 80)
    print("\nCheck the generated plots to visualize the memory reduction:")
    print("  - memory_reduction_summary.png: Overall memory usage")
    print("  - memory_reduction_detailed.png: Per-structure memory usage")
    print()


if __name__ == "__main__":
    main()
