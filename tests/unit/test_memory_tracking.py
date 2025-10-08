"""
Unit tests for memory consumption tracking in CriticalPathFinding
"""
import pytest
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent


class TestMemoryTracking:
    """Test memory tracking functionality in CriticalPathFinding"""
    
    def create_simple_trace(self):
        """
        Create a simple trace for testing.
        
        Returns:
            Trace object with basic events
        """
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        event_id = 0
        
        # Process 0: computation
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
        
        # Process 1: computation
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
        
        # Process 0: Send to Process 1
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
        
        # Process 1: Receive from Process 0
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
        
        return trace
    
    def test_memory_tracking_disabled_by_default(self):
        """Test that memory tracking is disabled by default"""
        analyzer = CriticalPathFinding()
        assert not analyzer.isMemoryTrackingEnabled()
        assert len(analyzer.getMemoryStatistics()) == 0
    
    def test_memory_tracking_enable_via_constructor(self):
        """Test enabling memory tracking via constructor"""
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        assert analyzer.isMemoryTrackingEnabled()
    
    def test_memory_tracking_enable_via_setter(self):
        """Test enabling memory tracking via setter method"""
        analyzer = CriticalPathFinding()
        assert not analyzer.isMemoryTrackingEnabled()
        
        analyzer.setEnableMemoryTracking(True)
        assert analyzer.isMemoryTrackingEnabled()
        
        analyzer.setEnableMemoryTracking(False)
        assert not analyzer.isMemoryTrackingEnabled()
    
    def test_memory_tracking_disabled_no_stats(self):
        """Test that no memory stats are collected when tracking is disabled"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(trace, enable_memory_tracking=False)
        
        # Run analysis
        analyzer.forwardReplay()
        if analyzer.m_earliest_finish_times:
            analyzer.m_critical_path_length = max(analyzer.m_earliest_finish_times.values())
        analyzer.backwardReplay()
        analyzer._identify_critical_path()
        
        # No memory stats should be collected
        stats = analyzer.getMemoryStatistics()
        assert len(stats) == 0
    
    def test_memory_tracking_enabled_collects_stats(self):
        """Test that memory stats are collected when tracking is enabled via run()"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis (memory tracking happens in run() method)
        analyzer.run()
        
        # Memory stats should be collected
        stats = analyzer.getMemoryStatistics()
        assert len(stats) > 0
        assert 'trace_memory_bytes' in stats
    
    def test_memory_tracking_via_run_method(self):
        """Test memory tracking through the run() method"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis
        analyzer.run()
        
        # Check memory stats
        stats = analyzer.getMemoryStatistics()
        assert 'trace_memory_bytes' in stats
        assert 'forward_replay_start_memory_bytes' in stats
        assert 'forward_replay_end_memory_bytes' in stats
        assert 'forward_replay_delta_bytes' in stats
        assert 'backward_replay_start_memory_bytes' in stats
        assert 'backward_replay_end_memory_bytes' in stats
        assert 'backward_replay_delta_bytes' in stats
        assert 'peak_memory_bytes' in stats
        
        # Verify values are positive integers
        assert stats['trace_memory_bytes'] > 0
        assert stats['forward_replay_start_memory_bytes'] > 0
        assert stats['forward_replay_end_memory_bytes'] > 0
        assert stats['backward_replay_start_memory_bytes'] > 0
        assert stats['backward_replay_end_memory_bytes'] > 0
        assert stats['peak_memory_bytes'] > 0
    
    def test_memory_stats_cleared_on_clear(self):
        """Test that memory stats are cleared when clear() is called"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis
        analyzer.run()
        
        # Verify stats exist
        stats = analyzer.getMemoryStatistics()
        assert len(stats) > 0
        
        # Clear and verify stats are gone
        analyzer.clear()
        stats = analyzer.getMemoryStatistics()
        assert len(stats) == 0
    
    def test_trace_memory_measurement(self):
        """Test trace memory measurement"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        
        # Measure trace memory
        trace_memory = analyzer._measure_trace_memory(trace)
        
        # Should be positive
        assert trace_memory > 0
        
        # Should increase with more events
        for i in range(10):
            event = Event(
                event_type=EventType.COMPUTE,
                idx=100 + i,
                name=f"Extra_Event_{i}",
                pid=0,
                tid=0,
                timestamp=1.0 + i * 0.1
            )
            trace.addEvent(event)
        
        larger_trace_memory = analyzer._measure_trace_memory(trace)
        assert larger_trace_memory > trace_memory
    
    def test_memory_usage_measurement(self):
        """Test memory usage measurement"""
        analyzer = CriticalPathFinding()
        
        # Should return positive value
        memory_usage = analyzer._measure_memory_usage()
        assert memory_usage > 0
        
        # Multiple measurements should return similar (but potentially different) values
        memory_usage2 = analyzer._measure_memory_usage()
        assert memory_usage2 > 0
    
    def test_forward_replay_memory_delta(self):
        """Test forward replay memory delta calculation"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis
        analyzer.run()
        
        stats = analyzer.getMemoryStatistics()
        
        # Delta should be the difference between end and start
        expected_delta = (
            stats['forward_replay_end_memory_bytes'] - 
            stats['forward_replay_start_memory_bytes']
        )
        assert stats['forward_replay_delta_bytes'] == expected_delta
    
    def test_backward_replay_memory_delta(self):
        """Test backward replay memory delta calculation"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis
        analyzer.run()
        
        stats = analyzer.getMemoryStatistics()
        
        # Delta should be the difference between end and start
        expected_delta = (
            stats['backward_replay_end_memory_bytes'] - 
            stats['backward_replay_start_memory_bytes']
        )
        assert stats['backward_replay_delta_bytes'] == expected_delta
    
    def test_peak_memory_calculation(self):
        """Test peak memory calculation"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis
        analyzer.run()
        
        stats = analyzer.getMemoryStatistics()
        
        # Peak should be at least as large as any measurement
        assert stats['peak_memory_bytes'] >= stats['forward_replay_start_memory_bytes']
        assert stats['peak_memory_bytes'] >= stats['forward_replay_end_memory_bytes']
        assert stats['peak_memory_bytes'] >= stats['backward_replay_start_memory_bytes']
        assert stats['peak_memory_bytes'] >= stats['backward_replay_end_memory_bytes']
    
    def test_memory_stats_copy_returned(self):
        """Test that getMemoryStatistics returns a copy"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis
        analyzer.run()
        
        # Get stats
        stats1 = analyzer.getMemoryStatistics()
        stats2 = analyzer.getMemoryStatistics()
        
        # Should be equal but not the same object
        assert stats1 == stats2
        assert stats1 is not stats2
        
        # Modifying returned dict shouldn't affect internal state
        stats1['test_key'] = 'test_value'
        stats3 = analyzer.getMemoryStatistics()
        assert 'test_key' not in stats3
    
    def test_memory_tracking_with_larger_trace(self):
        """Test memory tracking with a larger, more complex trace"""
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=4)
        trace.setTraceInfo(trace_info)
        
        # Create a larger trace with more events
        event_id = 0
        for pid in range(4):
            for i in range(20):
                event = Event(
                    event_type=EventType.COMPUTE,
                    idx=event_id,
                    name=f"Compute_P{pid}_E{i}",
                    pid=pid,
                    tid=0,
                    timestamp=i * 0.1
                )
                event_id += 1
                trace.addEvent(event)
        
        # Add some communication events
        for src_pid in range(3):
            dest_pid = src_pid + 1
            send = MpiSendEvent(
                idx=event_id,
                name=f"Send_{src_pid}_to_{dest_pid}",
                pid=src_pid,
                tid=0,
                timestamp=1.0 + src_pid * 0.1,
                communicator=1,
                tag=100 + src_pid,
                dest_pid=dest_pid
            )
            event_id += 1
            trace.addEvent(send)
            
            recv = MpiRecvEvent(
                idx=event_id,
                name=f"Recv_{src_pid}_to_{dest_pid}",
                pid=dest_pid,
                tid=0,
                timestamp=1.1 + src_pid * 0.1,
                communicator=1,
                tag=100 + src_pid,
                src_pid=src_pid
            )
            event_id += 1
            trace.addEvent(recv)
            
            send.setRecvEvent(recv)
            recv.setSendEvent(send)
        
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        # Run analysis
        analyzer.run()
        
        stats = analyzer.getMemoryStatistics()
        
        # Verify all stats are present
        assert 'trace_memory_bytes' in stats
        assert 'forward_replay_start_memory_bytes' in stats
        assert 'forward_replay_end_memory_bytes' in stats
        assert 'forward_replay_delta_bytes' in stats
        assert 'backward_replay_start_memory_bytes' in stats
        assert 'backward_replay_end_memory_bytes' in stats
        assert 'backward_replay_delta_bytes' in stats
        assert 'peak_memory_bytes' in stats
        
        # Trace memory should be reasonable (more than just the object overhead)
        assert stats['trace_memory_bytes'] > 1000  # Should be larger with more events
    
    def test_multiple_runs_with_memory_tracking(self):
        """Test that memory tracking works correctly across multiple runs"""
        trace1 = self.create_simple_trace()
        trace2 = self.create_simple_trace()
        
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        
        # First run
        analyzer.get_inputs().add_data(trace1)
        analyzer.run()
        stats1 = analyzer.getMemoryStatistics()
        assert len(stats1) > 0
        
        # Clear for second run
        analyzer.clear()
        stats_after_clear = analyzer.getMemoryStatistics()
        assert len(stats_after_clear) == 0
        
        # Second run
        analyzer.get_inputs().add_data(trace2)
        analyzer.run()
        stats2 = analyzer.getMemoryStatistics()
        assert len(stats2) > 0
        
        # Stats should be similar but not necessarily identical
        assert 'trace_memory_bytes' in stats2
        assert 'peak_memory_bytes' in stats2
    
    def test_memory_timeline_collection(self):
        """Test that memory timeline is collected during replay"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.setMemorySampleInterval(1)  # Sample every event
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        timeline = analyzer.getMemoryTimeline()
        assert len(timeline) > 0
        
        # Check timeline structure
        for phase, event_count, memory_bytes in timeline:
            assert phase in ['forward', 'backward']
            assert event_count > 0
            assert memory_bytes > 0
    
    def test_memory_timeline_phases(self):
        """Test that timeline contains both forward and backward phases"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        timeline = analyzer.getMemoryTimeline()
        phases = {phase for phase, _, _ in timeline}
        
        assert 'forward' in phases
        assert 'backward' in phases
    
    def test_memory_sample_interval(self):
        """Test setting memory sample interval"""
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        
        # Default interval
        assert analyzer.m_memory_sample_interval == 1000
        
        # Set custom interval
        analyzer.setMemorySampleInterval(500)
        assert analyzer.m_memory_sample_interval == 500
        
        # Ensure minimum of 1
        analyzer.setMemorySampleInterval(0)
        assert analyzer.m_memory_sample_interval == 1
        
        analyzer.setMemorySampleInterval(-10)
        assert analyzer.m_memory_sample_interval == 1
    
    def test_memory_timeline_cleared_on_clear(self):
        """Test that memory timeline is cleared when clear() is called"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        # Verify timeline exists
        timeline = analyzer.getMemoryTimeline()
        assert len(timeline) > 0
        
        # Clear and verify timeline is gone
        analyzer.clear()
        timeline = analyzer.getMemoryTimeline()
        assert len(timeline) == 0
    
    def test_plot_memory_usage_no_timeline(self):
        """Test plotting when no timeline data is available"""
        analyzer = CriticalPathFinding()
        
        # Should handle gracefully when no data
        try:
            import matplotlib
            # This should print a message but not crash
            analyzer.plotMemoryUsage("/tmp/test_plot.png")
        except ImportError:
            # matplotlib not installed, skip this test
            pass
    
    def test_peak_memory_from_timeline(self):
        """Test that peak memory is computed from timeline when available"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        memory_stats = analyzer.getMemoryStatistics()
        timeline = analyzer.getMemoryTimeline()
        
        if timeline:
            # Peak from timeline should match or be close to peak from stats
            timeline_peak = max(mem for _, _, mem in timeline)
            stats_peak = memory_stats['peak_memory_bytes']
            
            # They should be equal
            assert timeline_peak == stats_peak
    
    def test_detailed_memory_tracking_disabled_by_default(self):
        """Test that detailed memory tracking is disabled by default"""
        analyzer = CriticalPathFinding()
        assert not analyzer.m_enable_detailed_memory_tracking
        
        analyzer2 = CriticalPathFinding(enable_memory_tracking=True)
        assert not analyzer2.m_enable_detailed_memory_tracking
    
    def test_detailed_memory_tracking_enable_via_constructor(self):
        """Test enabling detailed tracking via constructor"""
        analyzer = CriticalPathFinding(enable_memory_tracking=True,
                                      enable_detailed_memory_tracking=True)
        assert analyzer.m_enable_detailed_memory_tracking
    
    def test_detailed_memory_timeline_collection(self):
        """Test that detailed memory timeline is collected"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True,
                                      enable_detailed_memory_tracking=True)
        analyzer.setMemorySampleInterval(1)  # Sample every event
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        detailed_timeline = analyzer.getDetailedMemoryTimeline()
        assert len(detailed_timeline) > 0
        
        # Check structure
        for phase, event_count, mem_dict in detailed_timeline:
            assert phase in ['forward', 'backward']
            assert event_count > 0
            assert isinstance(mem_dict, dict)
            assert len(mem_dict) > 0
    
    def test_detailed_memory_tracks_key_variables(self):
        """Test that detailed tracking captures key data structures"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True,
                                      enable_detailed_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        detailed_timeline = analyzer.getDetailedMemoryTimeline()
        
        if detailed_timeline:
            _, _, mem_dict = detailed_timeline[0]
            
            # Check that key variables are tracked
            expected_vars = [
                'm_event_costs',
                'm_earliest_start_times',
                'm_earliest_finish_times',
                'm_latest_start_times',
                'm_latest_finish_times',
                'm_slack_times',
                'm_predecessors',
                'm_successors'
            ]
            
            for var_name in expected_vars:
                assert var_name in mem_dict
                assert mem_dict[var_name] >= 0
    
    def test_detailed_memory_shows_growth(self):
        """Test that detailed tracking shows memory growth and cleanup"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True,
                                      enable_detailed_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        detailed_timeline = analyzer.getDetailedMemoryTimeline()
        
        if len(detailed_timeline) >= 2:
            first_sample = detailed_timeline[0][2]
            last_sample = detailed_timeline[-1][2]
            
            # Check that forward-only variables shrink during backward pass
            # (they are cleaned up during backward replay as memory optimization)
            for var_name in ['m_earliest_start_times', 'm_earliest_finish_times', 'm_successors']:
                # In backward pass, these should be cleaned up
                if detailed_timeline[-1][0] == 'backward':
                    assert last_sample[var_name] <= first_sample[var_name]
            
            # Check that backward-only variables grow during analysis
            for var_name in ['m_latest_start_times', 'm_latest_finish_times', 'm_slack_times']:
                # These grow during backward pass
                assert last_sample[var_name] >= first_sample[var_name]
    
    def test_detailed_memory_cleared_on_clear(self):
        """Test that detailed timeline is cleared when clear() is called"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True,
                                      enable_detailed_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        # Verify timeline exists
        detailed_timeline = analyzer.getDetailedMemoryTimeline()
        assert len(detailed_timeline) > 0
        
        # Clear and verify timeline is gone
        analyzer.clear()
        detailed_timeline = analyzer.getDetailedMemoryTimeline()
        assert len(detailed_timeline) == 0
    
    def test_plot_detailed_memory(self):
        """Test plotting detailed memory usage"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(enable_memory_tracking=True,
                                      enable_detailed_memory_tracking=True)
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        # Try to plot detailed view
        try:
            import matplotlib
            analyzer.plotMemoryUsage("/tmp/test_detailed_plot.png", plot_detailed=True)
            # If matplotlib is available, this should work
        except ImportError:
            # matplotlib not installed, skip
            pass
    
    def test_memory_cleanup_during_backward_replay(self):
        """Test that memory is cleaned up during backward replay"""
        trace = self.create_simple_trace()
        analyzer = CriticalPathFinding(trace, enable_memory_tracking=True,
                                      enable_detailed_memory_tracking=True)
        
        # Run forward replay
        analyzer.forwardReplay()
        
        # Check that forward-pass data structures are populated
        assert len(analyzer.m_successors) > 0
        assert len(analyzer.m_earliest_start_times) > 0
        assert len(analyzer.m_earliest_finish_times) > 0
        
        # Save sizes after forward pass
        successors_size_after_forward = len(analyzer.m_successors)
        earliest_start_size_after_forward = len(analyzer.m_earliest_start_times)
        earliest_finish_size_after_forward = len(analyzer.m_earliest_finish_times)
        
        # Set critical path length for backward pass
        if analyzer.m_earliest_finish_times:
            analyzer.m_critical_path_length = max(analyzer.m_earliest_finish_times.values())
        
        # Run backward replay
        analyzer.backwardReplay()
        
        # Check that forward-pass data structures are cleaned up during backward replay
        # They should be smaller (ideally empty or very small) after backward pass
        assert len(analyzer.m_successors) < successors_size_after_forward
        assert len(analyzer.m_earliest_start_times) < earliest_start_size_after_forward
        assert len(analyzer.m_earliest_finish_times) < earliest_finish_size_after_forward
        
        # Ideally they should be empty after full backward replay
        assert len(analyzer.m_successors) == 0
        assert len(analyzer.m_earliest_start_times) == 0
        assert len(analyzer.m_earliest_finish_times) == 0
        
        # But backward-pass data structures should be populated
        assert len(analyzer.m_latest_start_times) > 0
        assert len(analyzer.m_latest_finish_times) > 0
        assert len(analyzer.m_slack_times) > 0
    
    def test_memory_reduction_with_large_trace(self):
        """Test that memory reduction is significant with larger traces"""
        # Create a larger trace
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=4)
        trace.setTraceInfo(trace_info)
        
        event_id = 0
        for pid in range(4):
            for i in range(100):
                event = Event(
                    event_type=EventType.COMPUTE,
                    idx=event_id,
                    name=f"Compute_P{pid}_E{i}",
                    pid=pid,
                    tid=0,
                    timestamp=i * 0.1
                )
                event_id += 1
                trace.addEvent(event)
        
        analyzer = CriticalPathFinding(enable_memory_tracking=True,
                                      enable_detailed_memory_tracking=True)
        analyzer.setMemorySampleInterval(50)
        analyzer.get_inputs().add_data(trace)
        
        analyzer.run()
        
        # Get detailed timeline
        detailed_timeline = analyzer.getDetailedMemoryTimeline()
        
        # Find peak memory for forward-pass structures during forward replay
        forward_samples = [sample for sample in detailed_timeline if sample[0] == 'forward']
        backward_samples = [sample for sample in detailed_timeline if sample[0] == 'backward']
        
        if forward_samples and backward_samples:
            # Get peak size of cleaned-up structures during forward pass
            peak_successors_forward = max(sample[2]['m_successors'] for sample in forward_samples)
            peak_earliest_start_forward = max(sample[2]['m_earliest_start_times'] for sample in forward_samples)
            peak_earliest_finish_forward = max(sample[2]['m_earliest_finish_times'] for sample in forward_samples)
            
            # Get final size during backward pass
            final_successors_backward = backward_samples[-1][2]['m_successors']
            final_earliest_start_backward = backward_samples[-1][2]['m_earliest_start_times']
            final_earliest_finish_backward = backward_samples[-1][2]['m_earliest_finish_times']
            
            # Memory should be reduced
            assert final_successors_backward < peak_successors_forward
            assert final_earliest_start_backward < peak_earliest_start_forward
            assert final_earliest_finish_backward < peak_earliest_finish_forward
            
            # Memory reduction should be significant (at least 50% for these structures)
            reduction_ratio = final_successors_backward / peak_successors_forward if peak_successors_forward > 0 else 1.0
            assert reduction_ratio < 0.5  # More than 50% reduction
