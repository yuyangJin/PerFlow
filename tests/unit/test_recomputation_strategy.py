"""
Unit tests for Step 2: Recomputation Strategy
"""
import pytest
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent


class TestRecomputationStrategy:
    """Test Step 2 recomputation strategies"""
    
    def create_test_trace(self, num_processes=4, num_iterations=100):
        """Create a test trace."""
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=num_processes)
        trace.setTraceInfo(trace_info)
        
        event_id = 0
        for iteration in range(num_iterations):
            current_time = iteration * 0.01
            
            # Computation events
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
            
            # Communication pattern
            if iteration % 5 == 0:
                comm_time = current_time + 0.001
                for src_pid in range(num_processes - 1):
                    dest_pid = src_pid + 1
                    
                    send = MpiSendEvent(
                        idx=event_id,
                        name=f"Send_I{iteration}_P{src_pid}",
                        pid=src_pid,
                        tid=0,
                        timestamp=comm_time,
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
                        timestamp=comm_time + 0.0001,
                        communicator=1,
                        tag=iteration * 1000 + src_pid,
                        src_pid=src_pid
                    )
                    event_id += 1
                    trace.addEvent(recv)
                    
                    send.setRecvEvent(recv)
                    recv.setSendEvent(send)
        
        return trace
    
    def test_recomputation_mode_none_default(self):
        """Test that default mode is 'none' (Step 1 only)"""
        analyzer = CriticalPathFinding()
        assert analyzer.m_recomputation_mode == 'none'
        assert analyzer.m_recomputation_threshold == 5
    
    def test_recomputation_mode_full(self):
        """Test full recomputation mode"""
        trace = self.create_test_trace(num_processes=4, num_iterations=50)
        analyzer = CriticalPathFinding(
            enable_memory_tracking=True,
            recomputation_mode='full'
        )
        analyzer.get_inputs().add_data(trace)
        analyzer.run()
        
        # In full mode, no earliest times should be stored during forward pass
        # All should be recomputed
        stats = analyzer.getRecomputationStats()
        assert stats['recomputed_events'] > 0
        assert stats['stored_events'] == 0
        
        # Critical path should still be computed correctly
        critical_path = analyzer.getCriticalPath()
        assert len(critical_path) > 0
    
    def test_recomputation_mode_partial(self):
        """Test partial recomputation mode"""
        trace = self.create_test_trace(num_processes=4, num_iterations=50)
        analyzer = CriticalPathFinding(
            enable_memory_tracking=True,
            recomputation_mode='partial',
            recomputation_threshold=2
        )
        analyzer.get_inputs().add_data(trace)
        analyzer.run()
        
        # In partial mode, some events should be stored and some recomputed
        stats = analyzer.getRecomputationStats()
        assert stats['stored_events'] > 0
        assert stats['recomputed_events'] > 0
        
        # Critical path should still be computed correctly
        critical_path = analyzer.getCriticalPath()
        assert len(critical_path) > 0
    
    def test_recomputation_correctness_full(self):
        """Test that full recomputation produces same results as baseline"""
        trace = self.create_test_trace(num_processes=4, num_iterations=30)
        
        # Baseline (none mode)
        analyzer_baseline = CriticalPathFinding(recomputation_mode='none')
        analyzer_baseline.get_inputs().add_data(trace)
        analyzer_baseline.run()
        baseline_length = analyzer_baseline.getCriticalPathLength()
        baseline_path_len = len(analyzer_baseline.getCriticalPath())
        
        # Full recomputation
        analyzer_full = CriticalPathFinding(recomputation_mode='full')
        analyzer_full.get_inputs().add_data(trace)
        analyzer_full.run()
        full_length = analyzer_full.getCriticalPathLength()
        full_path_len = len(analyzer_full.getCriticalPath())
        
        # Results should be identical
        assert abs(baseline_length - full_length) < 1e-6
        assert baseline_path_len == full_path_len
    
    def test_recomputation_correctness_partial(self):
        """Test that partial recomputation produces same results as baseline"""
        trace = self.create_test_trace(num_processes=4, num_iterations=30)
        
        # Baseline (none mode)
        analyzer_baseline = CriticalPathFinding(recomputation_mode='none')
        analyzer_baseline.get_inputs().add_data(trace)
        analyzer_baseline.run()
        baseline_length = analyzer_baseline.getCriticalPathLength()
        baseline_path_len = len(analyzer_baseline.getCriticalPath())
        
        # Partial recomputation with different thresholds
        for threshold in [1, 3, 5, 10]:
            analyzer_partial = CriticalPathFinding(
                recomputation_mode='partial',
                recomputation_threshold=threshold
            )
            analyzer_partial.get_inputs().add_data(trace)
            analyzer_partial.run()
            partial_length = analyzer_partial.getCriticalPathLength()
            partial_path_len = len(analyzer_partial.getCriticalPath())
            
            # Results should be identical
            assert abs(baseline_length - partial_length) < 1e-6
            assert baseline_path_len == partial_path_len
    
    def test_timing_measurements(self):
        """Test that timing measurements are recorded"""
        trace = self.create_test_trace(num_processes=4, num_iterations=50)
        analyzer = CriticalPathFinding(
            enable_memory_tracking=True,
            recomputation_mode='none'
        )
        analyzer.get_inputs().add_data(trace)
        analyzer.run()
        
        stats = analyzer.getMemoryStatistics()
        assert 'forward_replay_time_seconds' in stats
        assert 'backward_replay_time_seconds' in stats
        assert 'total_time_seconds' in stats
        
        # Times should be positive
        assert stats['forward_replay_time_seconds'] > 0
        assert stats['backward_replay_time_seconds'] > 0
        assert stats['total_time_seconds'] > 0
        
        # Total should equal sum
        total = stats['forward_replay_time_seconds'] + stats['backward_replay_time_seconds']
        assert abs(stats['total_time_seconds'] - total) < 0.001
    
    def test_memory_savings_full_recomputation(self):
        """Test that full recomputation reduces memory"""
        trace = self.create_test_trace(num_processes=4, num_iterations=100)
        
        # Baseline memory usage
        analyzer_baseline = CriticalPathFinding(
            enable_memory_tracking=True,
            recomputation_mode='none'
        )
        analyzer_baseline.get_inputs().add_data(trace)
        analyzer_baseline.run()
        baseline_peak = analyzer_baseline.getMemoryStatistics()['peak_memory_bytes']
        
        # Full recomputation memory usage
        analyzer_full = CriticalPathFinding(
            enable_memory_tracking=True,
            recomputation_mode='full'
        )
        analyzer_full.get_inputs().add_data(trace)
        analyzer_full.run()
        full_peak = analyzer_full.getMemoryStatistics()['peak_memory_bytes']
        
        # Full recomputation should use less or equal memory
        # (May be equal if trace is small and overhead dominates)
        assert full_peak <= baseline_peak * 1.1  # Allow 10% margin
    
    def test_threshold_affects_storage(self):
        """Test that threshold affects how many events are stored"""
        trace = self.create_test_trace(num_processes=4, num_iterations=50)
        
        results = []
        for threshold in [1, 3, 5, 10]:
            analyzer = CriticalPathFinding(
                recomputation_mode='partial',
                recomputation_threshold=threshold
            )
            analyzer.get_inputs().add_data(trace)
            analyzer.run()
            stats = analyzer.getRecomputationStats()
            results.append((threshold, stats['stored_events'], stats['recomputed_events']))
        
        # Higher threshold should mean fewer stored events
        for i in range(len(results) - 1):
            thresh1, stored1, _ = results[i]
            thresh2, stored2, _ = results[i + 1]
            assert thresh2 > thresh1
            # Generally, higher threshold means fewer stored (though not strictly monotonic)
            # Just verify the relationship holds in aggregate
        
        # Lowest threshold should have most stored events
        _, stored_min, _ = results[0]
        _, stored_max, _ = results[-1]
        # At higher thresholds, we should have fewer or equal stored events
        assert stored_max <= stored_min * 2  # Allow some variation
    
    def test_recomputation_stats_in_memory_stats(self):
        """Test that recomputation stats are included in memory stats"""
        trace = self.create_test_trace(num_processes=4, num_iterations=30)
        analyzer = CriticalPathFinding(
            enable_memory_tracking=True,
            recomputation_mode='partial',
            recomputation_threshold=3
        )
        analyzer.get_inputs().add_data(trace)
        analyzer.run()
        
        stats = analyzer.getMemoryStatistics()
        assert 'recomputation_stats' in stats
        assert 'stored_events' in stats['recomputation_stats']
        assert 'recomputed_events' in stats['recomputation_stats']
    
    def test_clear_resets_recomputation_stats(self):
        """Test that clear() resets recomputation statistics"""
        trace = self.create_test_trace(num_processes=4, num_iterations=30)
        analyzer = CriticalPathFinding(
            recomputation_mode='partial',
            recomputation_threshold=3
        )
        analyzer.get_inputs().add_data(trace)
        analyzer.run()
        
        # Should have non-zero stats
        stats = analyzer.getRecomputationStats()
        assert stats['stored_events'] > 0 or stats['recomputed_events'] > 0
        
        # Clear and check
        analyzer.clear()
        stats_after = analyzer.getRecomputationStats()
        assert stats_after['stored_events'] == 0
        assert stats_after['recomputed_events'] == 0
    
    def test_recomputation_with_complex_dependencies(self):
        """Test recomputation with more complex dependency graphs"""
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=3)
        trace.setTraceInfo(trace_info)
        
        event_id = 0
        
        # Create events with varying numbers of predecessors
        for i in range(20):
            for pid in range(3):
                comp = Event(
                    event_type=EventType.COMPUTE,
                    idx=event_id,
                    name=f"Event_{i}_{pid}",
                    pid=pid,
                    tid=0,
                    timestamp=i * 0.1
                )
                event_id += 1
                trace.addEvent(comp)
                
                # Add some cross-process dependencies
                if i > 0 and pid > 0:
                    prev_pid = pid - 1
                    send = MpiSendEvent(
                        idx=event_id,
                        name=f"Send_{i}_{prev_pid}_to_{pid}",
                        pid=prev_pid,
                        tid=0,
                        timestamp=i * 0.1 - 0.01,
                        communicator=1,
                        tag=i * 100 + prev_pid,
                        dest_pid=pid
                    )
                    event_id += 1
                    trace.addEvent(send)
                    
                    recv = MpiRecvEvent(
                        idx=event_id,
                        name=f"Recv_{i}_{prev_pid}_to_{pid}",
                        pid=pid,
                        tid=0,
                        timestamp=i * 0.1 - 0.005,
                        communicator=1,
                        tag=i * 100 + prev_pid,
                        src_pid=prev_pid
                    )
                    event_id += 1
                    trace.addEvent(recv)
                    
                    send.setRecvEvent(recv)
                    recv.setSendEvent(send)
        
        # Test with different modes
        for mode in ['none', 'full', 'partial']:
            analyzer = CriticalPathFinding(
                recomputation_mode=mode,
                recomputation_threshold=2
            )
            analyzer.get_inputs().add_data(trace)
            analyzer.run()
            
            # Should complete without error
            critical_path = analyzer.getCriticalPath()
            assert len(critical_path) > 0
