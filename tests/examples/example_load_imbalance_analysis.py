"""
Example: Load Imbalance Analysis

This example demonstrates how to detect and analyze load imbalance
in parallel applications by examining computation times across processes.
"""

from collections import defaultdict
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.task.trace_analysis.low_level.trace_replayer import TraceReplayer


def create_imbalanced_trace():
    """
    Create a trace with load imbalance.
    
    Simulates 4 processes with varying computation times,
    demonstrating load imbalance.
    """
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0)
    trace.setTraceInfo(trace_info)
    
    # Define computation times for each process (in seconds)
    # Process 0: 5.0s (overloaded)
    # Process 1: 2.0s (balanced)
    # Process 2: 2.5s (balanced)
    # Process 3: 1.5s (underutilized)
    computation_times = {
        0: 5.0,
        1: 2.0,
        2: 2.5,
        3: 1.5
    }
    
    event_id = 0
    
    for pid, comp_time in computation_times.items():
        # Enter computation
        enter_event = Event(
            event_type=EventType.ENTER,
            idx=event_id,
            name="compute_kernel",
            pid=pid,
            tid=0,
            timestamp=0.0,
            replay_pid=pid
        )
        event_id += 1
        
        # Computation work
        compute_event = Event(
            event_type=EventType.COMPUTE,
            idx=event_id,
            name="matrix_multiply",
            pid=pid,
            tid=0,
            timestamp=0.1,
            replay_pid=pid
        )
        event_id += 1
        
        # Leave computation
        leave_event = Event(
            event_type=EventType.LEAVE,
            idx=event_id,
            name="compute_kernel",
            pid=pid,
            tid=0,
            timestamp=comp_time,
            replay_pid=pid
        )
        event_id += 1
        
        trace.addEvent(enter_event)
        trace.addEvent(compute_event)
        trace.addEvent(leave_event)
    
    return trace


class LoadBalanceAnalyzer:
    """Analyzer for load balance"""
    
    def __init__(self):
        self.process_times = defaultdict(float)
        self.process_stack = defaultdict(list)  # Stack for nested function calls
        self.total_time = 0.0
    
    def analyze_event(self, event):
        """Callback to analyze computation events"""
        pid = event.getPid()
        event_type = event.getType()
        timestamp = event.getTimestamp() or 0.0
        
        if event_type == EventType.ENTER:
            # Push timestamp onto stack
            self.process_stack[pid].append(timestamp)
        
        elif event_type == EventType.LEAVE:
            # Pop timestamp and calculate duration
            if self.process_stack[pid]:
                enter_time = self.process_stack[pid].pop()
                duration = timestamp - enter_time
                self.process_times[pid] += duration
    
    def get_results(self):
        """Get load balance analysis results"""
        if not self.process_times:
            return None
        
        times = list(self.process_times.values())
        num_processes = len(times)
        
        avg_time = sum(times) / num_processes
        max_time = max(times)
        min_time = min(times)
        
        # Calculate imbalance percentage
        imbalance = ((max_time - avg_time) / avg_time * 100) if avg_time > 0 else 0
        
        # Calculate efficiency (how well balanced)
        efficiency = (avg_time / max_time * 100) if max_time > 0 else 100
        
        return {
            "process_times": dict(self.process_times),
            "average_time": avg_time,
            "max_time": max_time,
            "min_time": min_time,
            "imbalance_percent": imbalance,
            "parallel_efficiency": efficiency
        }


def analyze_load_balance(trace):
    """
    Analyze load balance in trace.
    
    Args:
        trace: Trace object to analyze
        
    Returns:
        Dictionary with load balance analysis results
    """
    analyzer = LoadBalanceAnalyzer()
    
    replayer = TraceReplayer(trace)
    replayer.registerCallback("load_analyzer", analyzer.analyze_event)
    replayer.forwardReplay()
    
    return analyzer.get_results()


def print_load_balance_analysis(results):
    """
    Print load balance analysis results.
    
    Args:
        results: Dictionary with analysis results
    """
    if not results:
        print("No load balance data available")
        return
    
    print("=" * 70)
    print("LOAD BALANCE ANALYSIS")
    print("=" * 70)
    
    print("\nPer-Process Computation Times:")
    print("-" * 40)
    for pid, time in sorted(results['process_times'].items()):
        deviation = ((time - results['average_time']) / results['average_time'] * 100)
        print(f"  Process {pid}: {time:.6f}s ({deviation:+.1f}% from average)")
    
    print("\nAggregate Statistics:")
    print("-" * 40)
    print(f"  Average Time: {results['average_time']:.6f}s")
    print(f"  Maximum Time: {results['max_time']:.6f}s")
    print(f"  Minimum Time: {results['min_time']:.6f}s")
    print(f"  Time Spread: {results['max_time'] - results['min_time']:.6f}s")
    
    print("\nLoad Balance Metrics:")
    print("-" * 40)
    print(f"  Load Imbalance: {results['imbalance_percent']:.2f}%")
    print(f"  Parallel Efficiency: {results['parallel_efficiency']:.2f}%")
    
    # Provide recommendations
    print("\nRecommendations:")
    print("-" * 40)
    
    if results['imbalance_percent'] > 20:
        print("  ⚠ Significant load imbalance detected!")
        print("    → Consider redistributing work across processes")
        print("    → Look for opportunities to balance computation")
    elif results['imbalance_percent'] > 10:
        print("  ⚡ Moderate load imbalance present")
        print("    → Minor optimization opportunities exist")
    else:
        print("  ✓ Load is well balanced across processes")
    
    if results['parallel_efficiency'] < 80:
        print(f"  ⚠ Low parallel efficiency ({results['parallel_efficiency']:.1f}%)")
        print("    → Significant time wasted due to imbalance")
    
    print("\n" + "=" * 70)


def compare_scenarios():
    """Compare balanced vs imbalanced scenarios"""
    print("\n" + "=" * 70)
    print("COMPARING BALANCED VS IMBALANCED SCENARIOS")
    print("=" * 70)
    
    # Scenario 1: Imbalanced (current)
    print("\nScenario 1: Imbalanced Workload")
    print("-" * 40)
    imbalanced_trace = create_imbalanced_trace()
    imbalanced_results = analyze_load_balance(imbalanced_trace)
    print(f"Parallel Efficiency: {imbalanced_results['parallel_efficiency']:.2f}%")
    print(f"Load Imbalance: {imbalanced_results['imbalance_percent']:.2f}%")
    
    # Scenario 2: Balanced (hypothetical)
    print("\nScenario 2: Balanced Workload (Hypothetical)")
    print("-" * 40)
    avg_time = imbalanced_results['average_time']
    print(f"If all processes took {avg_time:.2f}s:")
    print(f"Parallel Efficiency: 100.00%")
    print(f"Load Imbalance: 0.00%")
    
    # Calculate potential speedup
    current_runtime = imbalanced_results['max_time']
    optimal_runtime = avg_time
    speedup = current_runtime / optimal_runtime
    
    print(f"\nPotential Improvement:")
    print(f"  Current Runtime: {current_runtime:.2f}s")
    print(f"  Optimal Runtime: {optimal_runtime:.2f}s")
    print(f"  Potential Speedup: {speedup:.2f}x")


def main():
    """
    Main function demonstrating load balance analysis.
    """
    print("\n" + "=" * 70)
    print("PERFLOW EXAMPLE: LOAD IMBALANCE ANALYSIS")
    print("=" * 70)
    print("\nThis example detects and analyzes load imbalance in parallel traces.")
    
    # Create imbalanced trace
    print("\nCreating trace with load imbalance...")
    trace = create_imbalanced_trace()
    print(f"  Created trace with {trace.getEventCount()} events")
    print("  4 processes with varying computation times")
    
    # Analyze load balance
    print("\nAnalyzing load balance...")
    results = analyze_load_balance(trace)
    
    # Display results
    print_load_balance_analysis(results)
    
    # Compare scenarios
    compare_scenarios()
    
    print("\nExample complete!")


if __name__ == "__main__":
    main()
