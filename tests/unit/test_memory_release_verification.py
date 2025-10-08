"""
Unit test to verify that memory is actually released (not just dictionary entries removed).

This test uses psutil to monitor the actual RSS (Resident Set Size) memory of the process
to confirm that deleting dictionary entries results in actual memory being freed back to the OS.
"""
import pytest
import gc
import psutil
from perflow.task.trace_analysis.critical_path_finding import CriticalPathFinding
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent


class TestMemoryReleaseVerification:
    """Test that memory is actually released to the OS, not just removed from dictionaries"""
    
    def create_large_trace(self, num_processes=8, num_iterations=2000):
        """
        Create a trace large enough to see memory changes.
        
        Args:
            num_processes: Number of processes
            num_iterations: Number of iterations
            
        Returns:
            Trace object
        """
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0, num_execution_processes=num_processes)
        trace.setTraceInfo(trace_info)
        
        event_id = 0
        timestamp = 0.0
        
        for iteration in range(num_iterations):
            current_time = timestamp + iteration * 0.001
            
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
            
            # Communication pattern every 5 iterations
            if iteration % 5 == 0:
                comm_time = current_time + 0.0001
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
                        timestamp=comm_time + 0.00001,
                        communicator=1,
                        tag=iteration * 1000 + src_pid,
                        src_pid=src_pid
                    )
                    event_id += 1
                    trace.addEvent(recv)
                    
                    send.setRecvEvent(recv)
                    recv.setSendEvent(send)
        
        return trace
    
    def get_memory_usage(self):
        """Get current process memory usage in bytes (RSS)"""
        process = psutil.Process()
        return process.memory_info().rss
    
    def test_memory_actually_released_during_backward_replay(self):
        """
        Test that actual process memory (RSS) decreases during backward replay.
        
        This test verifies that deleting dictionary entries not only removes them
        from the dictionaries but also results in actual memory being freed and
        returned to the operating system.
        """
        # Create a large trace
        trace = self.create_large_trace(num_processes=8, num_iterations=2000)
        
        # Create analyzer
        analyzer = CriticalPathFinding(trace)
        
        # Force garbage collection to get clean baseline
        gc.collect()
        
        # Get memory before forward replay
        memory_before_forward = self.get_memory_usage()
        
        # Run forward replay - this should increase memory
        analyzer.forwardReplay()
        
        # Force garbage collection
        gc.collect()
        
        # Get memory after forward replay
        memory_after_forward = self.get_memory_usage()
        
        # Verify memory increased during forward replay
        memory_growth_forward = memory_after_forward - memory_before_forward
        assert memory_growth_forward > 0, "Memory should grow during forward replay"
        
        # Check dictionary sizes after forward replay
        successors_count_after_forward = len(analyzer.m_successors)
        earliest_start_count_after_forward = len(analyzer.m_earliest_start_times)
        earliest_finish_count_after_forward = len(analyzer.m_earliest_finish_times)
        
        # All should be populated
        assert successors_count_after_forward > 0
        assert earliest_start_count_after_forward > 0
        assert earliest_finish_count_after_forward > 0
        
        # Set critical path length for backward pass
        if analyzer.m_earliest_finish_times:
            analyzer.m_critical_path_length = max(analyzer.m_earliest_finish_times.values())
        
        # Get memory before backward replay
        memory_before_backward = self.get_memory_usage()
        
        # Run backward replay - this should clean up memory
        analyzer.backwardReplay()
        
        # Force garbage collection to ensure freed memory is returned
        gc.collect()
        
        # Get memory after backward replay
        memory_after_backward = self.get_memory_usage()
        
        # Check dictionary sizes after backward replay
        successors_count_after_backward = len(analyzer.m_successors)
        earliest_start_count_after_backward = len(analyzer.m_earliest_start_times)
        earliest_finish_count_after_backward = len(analyzer.m_earliest_finish_times)
        
        # These should be empty or nearly empty
        assert successors_count_after_backward == 0, "m_successors should be empty after backward replay"
        assert earliest_start_count_after_backward == 0, "m_earliest_start_times should be empty"
        assert earliest_finish_count_after_backward == 0, "m_earliest_finish_times should be empty"
        
        # Calculate memory change during backward replay
        memory_change_backward = memory_after_backward - memory_before_backward
        
        # The key verification: Memory should NOT grow as much during backward replay
        # compared to forward replay, because we're cleaning up as we go.
        # In fact, it might even decrease or grow much less.
        #
        # Without cleanup: backward would grow similar to forward (building latest_* dicts)
        # With cleanup: backward grows less because we're freeing forward-pass data
        
        print(f"\n=== Memory Release Verification ===")
        print(f"Memory before forward replay: {memory_before_forward / (1024*1024):.2f} MB")
        print(f"Memory after forward replay:  {memory_after_forward / (1024*1024):.2f} MB")
        print(f"Memory growth during forward: {memory_growth_forward / (1024*1024):.2f} MB")
        print(f"\nMemory before backward replay: {memory_before_backward / (1024*1024):.2f} MB")
        print(f"Memory after backward replay:  {memory_after_backward / (1024*1024):.2f} MB")
        print(f"Memory change during backward: {memory_change_backward / (1024*1024):.2f} MB")
        print(f"\nDictionary counts after backward:")
        print(f"  m_successors: {successors_count_after_backward}")
        print(f"  m_earliest_start_times: {earliest_start_count_after_backward}")
        print(f"  m_earliest_finish_times: {earliest_finish_count_after_backward}")
        print(f"  m_latest_start_times: {len(analyzer.m_latest_start_times)}")
        print(f"  m_latest_finish_times: {len(analyzer.m_latest_finish_times)}")
        print(f"  m_slack_times: {len(analyzer.m_slack_times)}")
        
        # The memory change during backward should be significantly less than forward
        # because we're cleaning up the forward-pass data structures as we build
        # the backward-pass structures.
        # 
        # We expect: memory_change_backward < memory_growth_forward
        # This proves that the cleanup is actually freeing memory.
        assert memory_change_backward < memory_growth_forward, \
            f"Backward memory change ({memory_change_backward / (1024*1024):.2f} MB) should be " \
            f"less than forward growth ({memory_growth_forward / (1024*1024):.2f} MB) due to cleanup"
    
    def test_memory_released_to_os_not_just_python_heap(self):
        """
        Test that memory is released to OS, not just marked free in Python's heap.
        
        This test creates a large data structure, deletes it, and verifies that
        the RSS actually decreases after garbage collection.
        """
        # Get baseline memory
        gc.collect()
        baseline_memory = self.get_memory_usage()
        
        # Create analyzer and run forward replay to build large structures
        trace = self.create_large_trace(num_processes=8, num_iterations=3000)
        analyzer = CriticalPathFinding(trace)
        analyzer.forwardReplay()
        
        # Get memory after building structures
        gc.collect()
        memory_with_structures = self.get_memory_usage()
        memory_allocated = memory_with_structures - baseline_memory
        
        assert memory_allocated > 1024 * 1024, "Should have allocated at least 1MB"
        
        # Now clear the dictionaries
        analyzer.m_successors.clear()
        analyzer.m_earliest_start_times.clear()
        analyzer.m_earliest_finish_times.clear()
        
        # Force garbage collection
        gc.collect()
        
        # Get memory after cleanup
        memory_after_cleanup = self.get_memory_usage()
        memory_freed = memory_with_structures - memory_after_cleanup
        
        print(f"\n=== OS Memory Release Verification ===")
        print(f"Baseline memory: {baseline_memory / (1024*1024):.2f} MB")
        print(f"Memory with structures: {memory_with_structures / (1024*1024):.2f} MB")
        print(f"Memory allocated: {memory_allocated / (1024*1024):.2f} MB")
        print(f"Memory after cleanup: {memory_after_cleanup / (1024*1024):.2f} MB")
        print(f"Memory freed: {memory_freed / (1024*1024):.2f} MB")
        print(f"Percentage freed: {100 * memory_freed / memory_allocated:.1f}%")
        
        # Verify that at least some memory was freed
        # (We can't expect 100% because Python's memory allocator may keep some)
        assert memory_freed > 0, "Some memory should be freed to OS after cleanup"
        
        # Verify that a significant portion was freed (at least 10%)
        percentage_freed = 100 * memory_freed / memory_allocated
        assert percentage_freed > 10, \
            f"At least 10% of allocated memory should be freed, got {percentage_freed:.1f}%"
    
    def test_incremental_memory_release_during_backward_replay(self):
        """
        Test that memory is released incrementally during backward replay,
        not just at the end.
        """
        trace = self.create_large_trace(num_processes=8, num_iterations=2000)
        analyzer = CriticalPathFinding(trace)
        
        # Run forward replay
        analyzer.forwardReplay()
        
        # Set critical path length
        if analyzer.m_earliest_finish_times:
            analyzer.m_critical_path_length = max(analyzer.m_earliest_finish_times.values())
        
        # Get initial counts
        initial_successors = len(analyzer.m_successors)
        initial_earliest_start = len(analyzer.m_earliest_start_times)
        initial_earliest_finish = len(analyzer.m_earliest_finish_times)
        
        # Process half of the events in backward order
        events = analyzer.m_trace.getEvents()
        half_count = len(events) // 2
        
        for event in reversed(events[:half_count]):
            # Call the backward callback
            analyzer._compute_latest_times(event)
        
        # Check that some entries have been cleaned up
        mid_successors = len(analyzer.m_successors)
        mid_earliest_start = len(analyzer.m_earliest_start_times)
        mid_earliest_finish = len(analyzer.m_earliest_finish_times)
        
        print(f"\n=== Incremental Release Verification ===")
        print(f"After forward replay:")
        print(f"  m_successors: {initial_successors}")
        print(f"  m_earliest_start_times: {initial_earliest_start}")
        print(f"  m_earliest_finish_times: {initial_earliest_finish}")
        print(f"\nAfter processing half of backward replay:")
        print(f"  m_successors: {mid_successors}")
        print(f"  m_earliest_start_times: {mid_earliest_start}")
        print(f"  m_earliest_finish_times: {mid_earliest_finish}")
        print(f"\nReduction so far:")
        print(f"  m_successors: {initial_successors - mid_successors} ({100*(initial_successors - mid_successors)/initial_successors:.1f}%)")
        print(f"  m_earliest_start_times: {initial_earliest_start - mid_earliest_start} ({100*(initial_earliest_start - mid_earliest_start)/initial_earliest_start:.1f}%)")
        print(f"  m_earliest_finish_times: {initial_earliest_finish - mid_earliest_finish} ({100*(initial_earliest_finish - mid_earliest_finish)/initial_earliest_finish:.1f}%)")
        
        # Verify incremental cleanup is happening
        assert mid_successors < initial_successors, \
            "m_successors should decrease during backward replay"
        assert mid_earliest_start < initial_earliest_start, \
            "m_earliest_start_times should decrease during backward replay"
        assert mid_earliest_finish < initial_earliest_finish, \
            "m_earliest_finish_times should decrease during backward replay"
        
        # Verify significant cleanup has happened (at least 20% reduced)
        reduction_percentage = 100 * (initial_successors - mid_successors) / initial_successors
        assert reduction_percentage > 20, \
            f"At least 20% of entries should be cleaned up halfway through, got {reduction_percentage:.1f}%"
