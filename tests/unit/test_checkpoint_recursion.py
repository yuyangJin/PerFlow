"""
Test to verify that full recomputation with checkpoints avoids RecursionError
"""
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType


def create_long_chain_trace(chain_length=2000):
    """
    Create a trace with a long dependency chain to test recursion depth.
    
    Args:
        chain_length: Length of the dependency chain
        
    Returns:
        Trace with long sequential dependencies
    """
    trace = Trace()
    trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=1)
    trace.setTraceInfo(trace_info)
    
    # Create a long chain of sequential events
    for i in range(chain_length):
        event = Event(
            event_type=EventType.COMPUTE,
            idx=i,
            name=f"Event_{i}",
            pid=0,
            tid=0,
            timestamp=i * 0.001
        )
        trace.addEvent(event)
    
    return trace


def test_full_recomputation_no_recursion_error():
    """
    Test that full recomputation with checkpoints doesn't hit RecursionError.
    
    This test creates a long dependency chain (2000 events) that would
    previously cause RecursionError. With checkpoints, it should complete
    successfully.
    """
    print("\nTesting full recomputation with long dependency chain (2000 events)...")
    
    trace = create_long_chain_trace(chain_length=2000)
    
    # Test with full recomputation mode
    # Default checkpoint_interval=500 should prevent recursion issues
    analyzer = CriticalPathFinding(
        enable_memory_tracking=True,
        recomputation_mode='full'
    )
    analyzer.get_inputs().add_data(trace)
    
    try:
        analyzer.run()
        print("✓ Full recomputation completed successfully!")
        
        # Verify results
        critical_path = analyzer.getCriticalPath()
        stats = analyzer.getRecomputationStats()
        
        print(f"  Critical path length: {analyzer.getCriticalPathLength()}")
        print(f"  Critical path events: {len(critical_path)}")
        print(f"  Checkpoint events: {stats['checkpoint_events']}")
        print(f"  Recomputed events: {stats['recomputed_events']}")
        print(f"  Total events: {stats['checkpoint_events'] + stats['recomputed_events']}")
        
        # Should have checkpoints
        assert stats['checkpoint_events'] > 0, "Should have checkpoint events"
        assert stats['recomputed_events'] > 0, "Should have recomputed events"
        
        # Total should match trace size
        total = stats['checkpoint_events'] + stats['recomputed_events']
        assert total == 2000, f"Total events should be 2000, got {total}"
        
        print("\n✓ All assertions passed!")
        return True
        
    except RecursionError as e:
        print(f"✗ RecursionError occurred: {e}")
        return False


def test_checkpoint_interval_configurable():
    """
    Test that checkpoint interval is configurable.
    """
    print("\nTesting configurable checkpoint interval...")
    
    trace = create_long_chain_trace(chain_length=1000)
    
    # Test with different checkpoint intervals
    for interval in [100, 250, 500]:
        print(f"\n  Testing with checkpoint_interval={interval}...")
        
        analyzer = CriticalPathFinding(
            recomputation_mode='full',
            checkpoint_interval=interval
        )
        analyzer.get_inputs().add_data(trace)
        analyzer.run()
        
        stats = analyzer.getRecomputationStats()
        expected_checkpoints = (1000 + interval - 1) // interval  # Ceiling division
        
        print(f"    Checkpoint events: {stats['checkpoint_events']} (expected ~{expected_checkpoints})")
        print(f"    Recomputed events: {stats['recomputed_events']}")
        
        # Verify checkpoint count is roughly as expected
        assert abs(stats['checkpoint_events'] - expected_checkpoints) <= 1
    
    print("\n✓ Checkpoint interval configuration works correctly!")
    return True


def test_very_long_chain():
    """
    Test with an extremely long chain that would definitely cause RecursionError
    without checkpoints (>3000 events).
    """
    print("\nTesting with very long dependency chain (5000 events)...")
    
    trace = create_long_chain_trace(chain_length=5000)
    
    analyzer = CriticalPathFinding(
        enable_memory_tracking=True,
        recomputation_mode='full',
        checkpoint_interval=400  # Store checkpoint every 400 events
    )
    analyzer.get_inputs().add_data(trace)
    
    try:
        analyzer.run()
        print("✓ Very long chain completed successfully!")
        
        stats = analyzer.getRecomputationStats()
        print(f"  Checkpoint events: {stats['checkpoint_events']}")
        print(f"  Recomputed events: {stats['recomputed_events']}")
        
        # Should have enough checkpoints to avoid deep recursion
        assert stats['checkpoint_events'] >= 10, "Should have at least 10 checkpoints"
        
        return True
        
    except RecursionError as e:
        print(f"✗ RecursionError occurred even with checkpoints: {e}")
        return False


if __name__ == "__main__":
    print("=" * 80)
    print("TESTING FULL RECOMPUTATION WITH CHECKPOINT STRATEGY")
    print("=" * 80)
    
    success = True
    
    success &= test_full_recomputation_no_recursion_error()
    success &= test_checkpoint_interval_configurable()
    success &= test_very_long_chain()
    
    print("\n" + "=" * 80)
    if success:
        print("ALL TESTS PASSED!")
    else:
        print("SOME TESTS FAILED!")
    print("=" * 80)
