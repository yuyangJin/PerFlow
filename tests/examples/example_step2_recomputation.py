"""
Example: Step 2 - Recomputation Strategy Experiments

This example demonstrates Step 2 of the memory optimization strategy: recomputation
of earliest times during backward replay instead of storing them during forward replay.

The experiments test three modes:
1. None (Step 1 only): Store all earliest times
2. Full recomputation: Store no earliest times, recompute all
3. Partial recomputation: Store only for events with >= N predecessors

This generates data to analyze the trade-off between memory usage and execution time.
"""

from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
import matplotlib.pyplot as plt
import sys


def create_experimental_trace(num_processes=16, num_iterations=3000):
    """
    Create a trace for experimentation.
    
    Args:
        num_processes: Number of processes
        num_iterations: Number of iterations
        
    Returns:
        Trace object
    """
    print(f"\n{'='*80}")
    print("CREATING TRACE FOR STEP 2 EXPERIMENTS")
    print(f"{'='*80}")
    print(f"\nConfiguration:")
    print(f"  Number of processes: {num_processes}")
    print(f"  Number of iterations: {num_iterations:,}")
    
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=num_processes)
    trace.setTraceInfo(trace_info)
    
    event_id = 0
    timestamp = 0.0
    
    for iteration in range(num_iterations):
        if iteration % 500 == 0:
            print(f"  Progress: {iteration:,}/{num_iterations:,} ({100*iteration/num_iterations:.1f}%)")
        
        current_time = timestamp + iteration * 0.001
        
        # Phase 1: Local computation
        for pid in range(num_processes):
            comp = Event(
                event_type=EventType.COMPUTE,
                idx=event_id,
                name=f"Compute_I{iteration}_P{pid}",
                pid=pid,
                tid=0,
                timestamp=current_time
            )
            event_id += 1
            trace.addEvent(comp)
        
        # Phase 2: Communication (pipeline pattern)
        comm_time = current_time + 0.0001
        if iteration % 10 == 0:
            for src_pid in range(num_processes - 1):
                dest_pid = src_pid + 1
                
                send = MpiSendEvent(
                    idx=event_id,
                    name=f"Send_I{iteration}_P{src_pid}",
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
                    name=f"Recv_I{iteration}_P{dest_pid}",
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
    
    total_events = trace.getEventCount()
    print(f"  Progress: {num_iterations:,}/{num_iterations:,} (100.0%)")
    print(f"\n{'='*80}")
    print(f"Trace Created: {total_events:,} events")
    print(f"{'='*80}\n")
    
    return trace


def run_experiment_mode(trace, mode, threshold=5):
    """
    Run critical path analysis with specific recomputation mode.
    
    Args:
        trace: Trace to analyze
        mode: Recomputation mode ('none', 'full', 'partial')
        threshold: Threshold for partial mode
        
    Returns:
        Dictionary with results
    """
    print(f"\nRunning: mode={mode}, threshold={threshold}")
    
    analyzer = CriticalPathFinding(
        enable_memory_tracking=True,
        enable_detailed_memory_tracking=True,
        recomputation_mode=mode,
        recomputation_threshold=threshold
    )
    analyzer.setMemorySampleInterval(2000)
    analyzer.get_inputs().add_data(trace)
    
    analyzer.run()
    
    stats = analyzer.getMemoryStatistics()
    recomp_stats = analyzer.getRecomputationStats()
    
    result = {
        'mode': mode,
        'threshold': threshold,
        'forward_time': stats.get('forward_replay_time_seconds', 0),
        'backward_time': stats.get('backward_replay_time_seconds', 0),
        'total_time': stats.get('total_time_seconds', 0),
        'peak_memory_mb': stats.get('peak_memory_bytes', 0) / (1024 * 1024),
        'forward_memory_delta_mb': stats.get('forward_replay_delta_bytes', 0) / (1024 * 1024),
        'backward_memory_delta_mb': stats.get('backward_replay_delta_bytes', 0) / (1024 * 1024),
        'stored_events': recomp_stats.get('stored_events', 0),
        'recomputed_events': recomp_stats.get('recomputed_events', 0)
    }
    
    print(f"  Forward time: {result['forward_time']:.3f}s")
    print(f"  Backward time: {result['backward_time']:.3f}s")
    print(f"  Total time: {result['total_time']:.3f}s")
    print(f"  Peak memory: {result['peak_memory_mb']:.2f} MB")
    print(f"  Stored events: {result['stored_events']:,}")
    print(f"  Recomputed events: {result['recomputed_events']:,}")
    
    return result


def experiment_1_baseline(trace):
    """
    Experiment 1: Record execution time and memory usage separately.
    """
    print(f"\n{'='*80}")
    print("EXPERIMENT 1: BASELINE MEASUREMENTS")
    print(f"{'='*80}")
    
    result = run_experiment_mode(trace, 'none')
    
    print(f"\n{'='*80}")
    print("BASELINE RESULTS")
    print(f"{'='*80}")
    print(f"Forward Replay:")
    print(f"  Time: {result['forward_time']:.3f} seconds")
    print(f"  Memory Delta: {result['forward_memory_delta_mb']:.2f} MB")
    print(f"\nBackward Replay:")
    print(f"  Time: {result['backward_time']:.3f} seconds")
    print(f"  Memory Delta: {result['backward_memory_delta_mb']:.2f} MB")
    print(f"\nTotal:")
    print(f"  Time: {result['total_time']:.3f} seconds")
    print(f"  Peak Memory: {result['peak_memory_mb']:.2f} MB")
    
    return result


def experiment_2_full_recomputation(trace):
    """
    Experiment 2: Full recomputation - no storage of earliest times.
    """
    print(f"\n{'='*80}")
    print("EXPERIMENT 2: FULL RECOMPUTATION")
    print(f"{'='*80}")
    
    result = run_experiment_mode(trace, 'full')
    
    print(f"\n{'='*80}")
    print("FULL RECOMPUTATION RESULTS")
    print(f"{'='*80}")
    print(f"Forward Replay:")
    print(f"  Time: {result['forward_time']:.3f} seconds")
    print(f"  Memory Delta: {result['forward_memory_delta_mb']:.2f} MB")
    print(f"\nBackward Replay:")
    print(f"  Time: {result['backward_time']:.3f} seconds")
    print(f"  Memory Delta: {result['backward_memory_delta_mb']:.2f} MB")
    print(f"\nTotal:")
    print(f"  Time: {result['total_time']:.3f} seconds")
    print(f"  Peak Memory: {result['peak_memory_mb']:.2f} MB")
    print(f"\nRecomputation:")
    print(f"  All {result['recomputed_events']:,} events recomputed")
    
    return result


def experiment_3_partial_recomputation(trace, thresholds=[1, 2, 3, 5, 10, 20]):
    """
    Experiment 3: Partial recomputation with varying thresholds.
    """
    print(f"\n{'='*80}")
    print("EXPERIMENT 3: PARTIAL RECOMPUTATION (VARYING THRESHOLDS)")
    print(f"{'='*80}")
    
    results = []
    for threshold in thresholds:
        result = run_experiment_mode(trace, 'partial', threshold)
        results.append(result)
    
    print(f"\n{'='*80}")
    print("PARTIAL RECOMPUTATION RESULTS SUMMARY")
    print(f"{'='*80}")
    print(f"{'Threshold':>10s} {'Time (s)':>10s} {'Memory (MB)':>12s} {'Stored':>10s} {'Recomputed':>12s}")
    print("-" * 80)
    for r in results:
        print(f"{r['threshold']:>10d} {r['total_time']:>10.3f} {r['peak_memory_mb']:>12.2f} "
              f"{r['stored_events']:>10,} {r['recomputed_events']:>12,}")
    
    return results


def experiment_4_visualize_tradeoffs(baseline, full_recomp, partial_results):
    """
    Experiment 4: Visualize relationships between memory, time, and threshold.
    """
    print(f"\n{'='*80}")
    print("EXPERIMENT 4: VISUALIZING TRADE-OFFS")
    print(f"{'='*80}")
    
    # Prepare data for plotting
    modes = ['Baseline\n(None)', 'Full\nRecompute']
    times = [baseline['total_time'], full_recomp['total_time']]
    memories = [baseline['peak_memory_mb'], full_recomp['peak_memory_mb']]
    
    # Add partial results
    for r in partial_results:
        modes.append(f"Partial\nN={r['threshold']}")
        times.append(r['total_time'])
        memories.append(r['peak_memory_mb'])
    
    # Create figure with subplots
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    
    # Plot 1: Execution time comparison
    colors = ['blue', 'red'] + ['green'] * len(partial_results)
    ax1.bar(range(len(modes)), times, color=colors, alpha=0.7)
    ax1.set_xlabel('Mode', fontsize=12)
    ax1.set_ylabel('Total Execution Time (seconds)', fontsize=12)
    ax1.set_title('Execution Time by Recomputation Mode', fontsize=14, fontweight='bold')
    ax1.set_xticks(range(len(modes)))
    ax1.set_xticklabels(modes, fontsize=10)
    ax1.grid(True, alpha=0.3, axis='y')
    
    # Plot 2: Peak memory comparison
    ax2.bar(range(len(modes)), memories, color=colors, alpha=0.7)
    ax2.set_xlabel('Mode', fontsize=12)
    ax2.set_ylabel('Peak Memory (MB)', fontsize=12)
    ax2.set_title('Peak Memory by Recomputation Mode', fontsize=14, fontweight='bold')
    ax2.set_xticks(range(len(modes)))
    ax2.set_xticklabels(modes, fontsize=10)
    ax2.grid(True, alpha=0.3, axis='y')
    
    # Plot 3: Memory vs Threshold (for partial modes)
    if partial_results:
        thresholds = [r['threshold'] for r in partial_results]
        partial_memories = [r['peak_memory_mb'] for r in partial_results]
        
        ax3.plot(thresholds, partial_memories, 'o-', color='green', linewidth=2, markersize=8)
        ax3.axhline(y=baseline['peak_memory_mb'], color='blue', linestyle='--', 
                   label=f'Baseline: {baseline["peak_memory_mb"]:.1f} MB')
        ax3.axhline(y=full_recomp['peak_memory_mb'], color='red', linestyle='--', 
                   label=f'Full Recompute: {full_recomp["peak_memory_mb"]:.1f} MB')
        ax3.set_xlabel('Threshold N (Number of Predecessors)', fontsize=12)
        ax3.set_ylabel('Peak Memory (MB)', fontsize=12)
        ax3.set_title('Memory Usage vs Threshold (Partial Recomputation)', 
                     fontsize=14, fontweight='bold')
        ax3.legend(fontsize=10)
        ax3.grid(True, alpha=0.3)
    
    # Plot 4: Time vs Threshold (for partial modes)
    if partial_results:
        partial_times = [r['total_time'] for r in partial_results]
        
        ax4.plot(thresholds, partial_times, 'o-', color='green', linewidth=2, markersize=8)
        ax4.axhline(y=baseline['total_time'], color='blue', linestyle='--', 
                   label=f'Baseline: {baseline["total_time"]:.2f}s')
        ax4.axhline(y=full_recomp['total_time'], color='red', linestyle='--', 
                   label=f'Full Recompute: {full_recomp["total_time"]:.2f}s')
        ax4.set_xlabel('Threshold N (Number of Predecessors)', fontsize=12)
        ax4.set_ylabel('Total Execution Time (seconds)', fontsize=12)
        ax4.set_title('Execution Time vs Threshold (Partial Recomputation)', 
                     fontsize=14, fontweight='bold')
        ax4.legend(fontsize=10)
        ax4.grid(True, alpha=0.3)
    
    plt.tight_layout()
    output_file = 'step2_recomputation_experiments.png'
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"\nVisualization saved to: {output_file}")
    plt.close()
    
    # Create second figure: Memory-Time trade-off
    fig2, ax = plt.subplots(figsize=(12, 8))
    
    # Plot all modes
    ax.scatter([baseline['peak_memory_mb']], [baseline['total_time']], 
              s=200, color='blue', marker='o', label='Baseline (None)', zorder=3)
    ax.scatter([full_recomp['peak_memory_mb']], [full_recomp['total_time']], 
              s=200, color='red', marker='s', label='Full Recomputation', zorder=3)
    
    if partial_results:
        partial_mem = [r['peak_memory_mb'] for r in partial_results]
        partial_time = [r['total_time'] for r in partial_results]
        ax.plot(partial_mem, partial_time, 'o-', color='green', 
               linewidth=2, markersize=10, label='Partial Recomputation', zorder=2)
        
        # Annotate thresholds
        for r in partial_results:
            ax.annotate(f'N={r["threshold"]}', 
                       xy=(r['peak_memory_mb'], r['total_time']),
                       xytext=(10, 5), textcoords='offset points',
                       fontsize=9, alpha=0.8)
    
    ax.set_xlabel('Peak Memory (MB)', fontsize=14)
    ax.set_ylabel('Total Execution Time (seconds)', fontsize=14)
    ax.set_title('Memory-Time Trade-off: Recomputation Strategies', 
                fontsize=16, fontweight='bold')
    ax.legend(fontsize=12, loc='best')
    ax.grid(True, alpha=0.3)
    
    output_file2 = 'step2_memory_time_tradeoff.png'
    plt.savefig(output_file2, dpi=150, bbox_inches='tight')
    print(f"Trade-off plot saved to: {output_file2}")
    plt.close()


def print_final_analysis(baseline, full_recomp, partial_results):
    """
    Print final analysis of all experiments.
    """
    print(f"\n{'='*80}")
    print("FINAL ANALYSIS: STEP 2 RECOMPUTATION STRATEGY")
    print(f"{'='*80}")
    
    print(f"\n1. BASELINE (Step 1 Only)")
    print(f"   - Time: {baseline['total_time']:.3f}s")
    print(f"   - Memory: {baseline['peak_memory_mb']:.2f} MB")
    print(f"   - Strategy: Store all earliest times")
    
    print(f"\n2. FULL RECOMPUTATION")
    time_overhead = ((full_recomp['total_time'] - baseline['total_time']) / baseline['total_time']) * 100
    memory_savings = ((baseline['peak_memory_mb'] - full_recomp['peak_memory_mb']) / baseline['peak_memory_mb']) * 100
    print(f"   - Time: {full_recomp['total_time']:.3f}s ({time_overhead:+.1f}%)")
    print(f"   - Memory: {full_recomp['peak_memory_mb']:.2f} MB ({memory_savings:.1f}% savings)")
    print(f"   - Strategy: Store nothing, recompute all")
    
    if partial_results:
        print(f"\n3. PARTIAL RECOMPUTATION (Best Configuration)")
        # Find best trade-off (lowest combined normalized score)
        best_idx = 0
        best_score = float('inf')
        for i, r in enumerate(partial_results):
            norm_time = r['total_time'] / baseline['total_time']
            norm_mem = r['peak_memory_mb'] / baseline['peak_memory_mb']
            score = norm_time + norm_mem
            if score < best_score:
                best_score = score
                best_idx = i
        
        best = partial_results[best_idx]
        time_overhead = ((best['total_time'] - baseline['total_time']) / baseline['total_time']) * 100
        memory_savings = ((baseline['peak_memory_mb'] - best['peak_memory_mb']) / baseline['peak_memory_mb']) * 100
        print(f"   - Threshold: N={best['threshold']}")
        print(f"   - Time: {best['total_time']:.3f}s ({time_overhead:+.1f}%)")
        print(f"   - Memory: {best['peak_memory_mb']:.2f} MB ({memory_savings:.1f}% savings)")
        print(f"   - Stored: {best['stored_events']:,} events")
        print(f"   - Recomputed: {best['recomputed_events']:,} events")
    
    print(f"\n{'='*80}")
    print("KEY FINDINGS")
    print(f"{'='*80}")
    print(f"✓ Full recomputation saves memory but increases execution time")
    print(f"✓ Partial recomputation balances memory savings with acceptable time overhead")
    print(f"✓ Optimal threshold depends on application requirements:")
    print(f"  - Memory-constrained: Use full or low threshold")
    print(f"  - Time-constrained: Use higher threshold or baseline")


def main():
    """
    Main function to run all Step 2 experiments.
    """
    print("\n" + "=" * 80)
    print("PERFLOW STEP 2: RECOMPUTATION STRATEGY EXPERIMENTS")
    print("=" * 80)
    print("\nThis experiment evaluates the trade-off between memory usage and")
    print("execution time when using recomputation strategies for critical path analysis.")
    
    # Create trace
    trace = create_experimental_trace(num_processes=16, num_iterations=3000)
    
    # Run experiments
    baseline = experiment_1_baseline(trace)
    full_recomp = experiment_2_full_recomputation(trace)
    partial_results = experiment_3_partial_recomputation(trace, thresholds=[1, 2, 3, 5, 10, 20])
    experiment_4_visualize_tradeoffs(baseline, full_recomp, partial_results)
    
    # Final analysis
    print_final_analysis(baseline, full_recomp, partial_results)
    
    print("\n" + "=" * 80)
    print("EXPERIMENTS COMPLETE!")
    print("=" * 80)
    print("\nGenerated outputs:")
    print("  - step2_recomputation_experiments.png: Comparison charts")
    print("  - step2_memory_time_tradeoff.png: Memory-Time trade-off plot")
    print()


if __name__ == "__main__":
    main()
