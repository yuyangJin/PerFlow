"""
Unit tests for CallingContextTree

This module contains comprehensive tests for the CallingContextTree class.
"""

import pytest
from perflow.perf_data_struct.dynamic.profile.calling_context_tree import CallingContextTree
from perflow.perf_data_struct.dynamic.profile.perf_data import PerfData
from perflow.perf_data_struct.dynamic.profile.sample_data import SampleData


class TestCallingContextTreeCreation:
    """Test CallingContextTree creation and initialization."""
    
    def test_cct_creation(self):
        """Test creating an empty CCT."""
        cct = CallingContextTree()
        assert cct.getNodeCount() == 0
        assert cct.getRoot() is None
    
    def test_cct_build_from_empty_profile(self):
        """Test building CCT from an empty profile."""
        cct = CallingContextTree()
        profile = PerfData()
        
        cct.buildFromProfile(profile)
        
        # Should have root node
        assert cct.getRoot() is not None
        assert cct.getRoot().getName() == "<root>"
        assert cct.getNodeCount() == 1


class TestCallingContextTreeBuilding:
    """Test building CCT from profiling data."""
    
    def test_build_simple_context(self):
        """Test building CCT with simple call stack."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add sample with simple call stack
        sample = SampleData(function_name="func_a")
        sample.setCallStack(["main", "func_a"])
        sample.setMetric("cycles", 1000.0)
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Should have root + main + func_a = 3 nodes
        assert cct.getNodeCount() == 3
    
    def test_build_nested_context(self):
        """Test building CCT with nested call stacks."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add samples with nested call stacks
        sample1 = SampleData()
        sample1.setCallStack(["main", "compute", "func_a"])
        sample1.setMetric("cycles", 1000.0)
        profile.addSample(sample1)
        
        sample2 = SampleData()
        sample2.setCallStack(["main", "compute", "func_b"])
        sample2.setMetric("cycles", 500.0)
        profile.addSample(sample2)
        
        cct.buildFromProfile(profile)
        
        # Should have root + main + compute + func_a + func_b = 5 nodes
        assert cct.getNodeCount() == 5
    
    def test_build_shared_context(self):
        """Test building CCT with shared call paths."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add multiple samples with same initial path
        for i in range(3):
            sample = SampleData()
            sample.setCallStack(["main", "compute", "func_a"])
            sample.setMetric("cycles", 100.0)
            profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Should still have root + main + compute + func_a = 4 nodes
        assert cct.getNodeCount() == 4
        
        # Find func_a node and check sample count
        func_a_context = ("main", "compute", "func_a")
        assert func_a_context in cct.m_context_nodes
        node_id = cct.m_context_nodes[func_a_context]
        assert cct.getNodeSampleCount(node_id) == 3


class TestCallingContextTreeMetrics:
    """Test CCT metrics aggregation."""
    
    def test_exclusive_metrics(self):
        """Test getting exclusive metrics for a node."""
        cct = CallingContextTree()
        profile = PerfData()
        
        sample = SampleData()
        sample.setCallStack(["main", "func_a"])
        sample.setMetric("cycles", 1000.0)
        sample.setMetric("instructions", 500.0)
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Get func_a node
        context = ("main", "func_a")
        node_id = cct.m_context_nodes[context]
        
        metrics = cct.getExclusiveMetrics(node_id)
        assert metrics["cycles"] == 1000.0
        assert metrics["instructions"] == 500.0
    
    def test_inclusive_metrics(self):
        """Test getting inclusive metrics (including descendants)."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add samples at different depths
        sample1 = SampleData()
        sample1.setCallStack(["main", "compute"])
        sample1.setMetric("cycles", 100.0)
        profile.addSample(sample1)
        
        sample2 = SampleData()
        sample2.setCallStack(["main", "compute", "func_a"])
        sample2.setMetric("cycles", 500.0)
        profile.addSample(sample2)
        
        cct.buildFromProfile(profile)
        
        # Get compute node
        compute_context = ("main", "compute")
        compute_id = cct.m_context_nodes[compute_context]
        
        # Exclusive should be just compute's samples
        exclusive = cct.getExclusiveMetrics(compute_id)
        assert exclusive["cycles"] == 100.0
        
        # Inclusive should include func_a's samples too
        inclusive = cct.getInclusiveMetrics(compute_id)
        assert inclusive["cycles"] == 600.0
    
    def test_metric_aggregation(self):
        """Test that metrics are aggregated across samples."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add multiple samples with same context
        for i in range(5):
            sample = SampleData()
            sample.setCallStack(["main", "func_a"])
            sample.setMetric("cycles", 100.0)
            profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        context = ("main", "func_a")
        node_id = cct.m_context_nodes[context]
        
        metrics = cct.getNodeMetrics(node_id)
        assert metrics["cycles"] == 500.0  # 5 * 100


class TestCallingContextTreeQueries:
    """Test CCT query methods."""
    
    def test_get_hot_paths(self):
        """Test getting hottest calling paths."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add samples with different cycle counts
        samples = [
            (["main", "func_a"], 1000.0),
            (["main", "func_b"], 500.0),
            (["main", "compute", "func_c"], 2000.0),
        ]
        
        for call_stack, cycles in samples:
            sample = SampleData()
            sample.setCallStack(call_stack)
            sample.setMetric("cycles", cycles)
            profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        hot_paths = cct.getHotPaths("cycles", top_n=2)
        
        assert len(hot_paths) == 2
        # First should be func_c with 2000 cycles
        assert hot_paths[0][1] == 2000.0
        # Second should be func_a with 1000 cycles
        assert hot_paths[1][1] == 1000.0
    
    def test_get_context_depth(self):
        """Test getting maximum context depth."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add samples with different depths
        sample1 = SampleData()
        sample1.setCallStack(["main", "func_a"])
        profile.addSample(sample1)
        
        sample2 = SampleData()
        sample2.setCallStack(["main", "compute", "helper", "func_b"])
        profile.addSample(sample2)
        
        cct.buildFromProfile(profile)
        
        # Maximum depth should be 4 (main, compute, helper, func_b)
        # Note: root is depth 0, main is 1, so func_b is at depth 4
        assert cct.getContextDepth() == 4
    
    def test_get_function_stats(self):
        """Test getting statistics for a specific function."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add samples where func_a appears in different contexts
        sample1 = SampleData()
        sample1.setCallStack(["main", "func_a"])
        sample1.setMetric("cycles", 1000.0)
        profile.addSample(sample1)
        
        sample2 = SampleData()
        sample2.setCallStack(["main", "compute", "func_a"])
        sample2.setMetric("cycles", 500.0)
        profile.addSample(sample2)
        
        cct.buildFromProfile(profile)
        
        stats = cct.getFunctionStats("func_a")
        
        assert stats["total_samples"] == 2
        assert stats["total_metrics"]["cycles"] == 1500.0
        assert stats["num_contexts"] == 2


class TestCallingContextTreeEdgeCases:
    """Test CCT edge cases."""
    
    def test_samples_without_call_stack(self):
        """Test handling samples without call stacks."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Sample with only function name, no call stack
        sample = SampleData(function_name="func_a")
        sample.setMetric("cycles", 1000.0)
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Should create a context with just func_a
        assert cct.getNodeCount() >= 2  # root + func_a
    
    def test_samples_without_function_name(self):
        """Test handling samples without function names."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Sample with no function name or call stack
        sample = SampleData()
        sample.setMetric("cycles", 1000.0)
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Should only have root
        assert cct.getNodeCount() == 1
    
    def test_clear(self):
        """Test clearing the CCT."""
        cct = CallingContextTree()
        profile = PerfData()
        
        sample = SampleData()
        sample.setCallStack(["main", "func_a"])
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        assert cct.getNodeCount() > 1
        
        cct.clear()
        assert cct.getNodeCount() == 0
        assert cct.getRoot() is None
    
    def test_rebuild_from_different_profile(self):
        """Test rebuilding CCT from a different profile."""
        cct = CallingContextTree()
        
        # Build from first profile
        profile1 = PerfData()
        sample1 = SampleData()
        sample1.setCallStack(["main", "func_a"])
        profile1.addSample(sample1)
        cct.buildFromProfile(profile1)
        
        initial_count = cct.getNodeCount()
        
        # Build from second profile
        profile2 = PerfData()
        sample2 = SampleData()
        sample2.setCallStack(["main", "func_b", "func_c"])
        profile2.addSample(sample2)
        cct.buildFromProfile(profile2)
        
        # Should have different structure
        assert cct.getNodeCount() != initial_count


class TestCallingContextTreeVisualization:
    """Test CCT visualization methods."""
    
    def test_visualize_tree_view(self):
        """Test tree view visualization."""
        import os
        import tempfile
        
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add samples
        sample = SampleData()
        sample.setCallStack(["main", "compute", "func_a"])
        sample.setMetric("cycles", 1000.0)
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Generate visualization
        with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
            output_file = f.name
        
        try:
            cct.visualize(output_file, view_type="tree", metric="cycles")
            
            # Check file was created
            assert os.path.exists(output_file)
            assert os.path.getsize(output_file) > 0
        finally:
            if os.path.exists(output_file):
                os.unlink(output_file)
    
    def test_visualize_ring_chart(self):
        """Test ring/sunburst chart visualization."""
        import os
        import tempfile
        
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add samples with multiple paths
        samples = [
            (["main", "compute", "func_a"], 1000.0),
            (["main", "compute", "func_b"], 500.0),
            (["main", "io"], 2000.0),
        ]
        
        for call_stack, cycles in samples:
            sample = SampleData()
            sample.setCallStack(call_stack)
            sample.setMetric("cycles", cycles)
            profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Generate visualization
        with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
            output_file = f.name
        
        try:
            cct.visualize(output_file, view_type="ring", metric="cycles")
            
            # Check file was created
            assert os.path.exists(output_file)
            assert os.path.getsize(output_file) > 0
        finally:
            if os.path.exists(output_file):
                os.unlink(output_file)
    
    def test_visualize_with_different_metrics(self):
        """Test visualization with different metrics."""
        import os
        import tempfile
        
        cct = CallingContextTree()
        profile = PerfData()
        
        # Add sample with multiple metrics
        sample = SampleData()
        sample.setCallStack(["main", "func_a"])
        sample.setMetric("cycles", 1000.0)
        sample.setMetric("instructions", 500.0)
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Test with different metrics
        for metric in ["cycles", "instructions"]:
            with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
                output_file = f.name
            
            try:
                cct.visualize(output_file, view_type="tree", metric=metric)
                assert os.path.exists(output_file)
            finally:
                if os.path.exists(output_file):
                    os.unlink(output_file)
    
    def test_visualize_invalid_view_type(self):
        """Test that invalid view type raises error."""
        cct = CallingContextTree()
        profile = PerfData()
        
        sample = SampleData()
        sample.setCallStack(["main", "func_a"])
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        with pytest.raises(ValueError, match="Unknown view_type"):
            cct.visualize("output.png", view_type="invalid")
    
    def test_visualize_empty_cct(self):
        """Test that visualizing empty CCT raises error."""
        cct = CallingContextTree()
        
        with pytest.raises(ValueError, match="Cannot visualize empty CCT"):
            cct.visualize("output.png", view_type="tree")
    
    def test_visualize_custom_figsize(self):
        """Test visualization with custom figure size."""
        import os
        import tempfile
        
        cct = CallingContextTree()
        profile = PerfData()
        
        sample = SampleData()
        sample.setCallStack(["main", "func_a"])
        sample.setMetric("cycles", 1000.0)
        profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
            output_file = f.name
        
        try:
            cct.visualize(output_file, view_type="tree", 
                         figsize=(16, 10), dpi=100)
            assert os.path.exists(output_file)
        finally:
            if os.path.exists(output_file):
                os.unlink(output_file)


class TestCallingContextTreeIntegration:
    """Integration tests for CCT."""
    
    def test_complete_workflow(self):
        """Test complete CCT workflow with realistic data."""
        cct = CallingContextTree()
        profile = PerfData()
        
        # Simulate profiling data from a compute application
        call_stacks = [
            (["main", "compute", "matrix_multiply"], 50, 1000.0),
            (["main", "compute", "vector_add"], 30, 500.0),
            (["main", "io_read"], 20, 2000.0),
            (["main", "compute", "helper"], 15, 300.0),
        ]
        
        for call_stack, count, cycles in call_stacks:
            for i in range(count):
                sample = SampleData()
                sample.setCallStack(call_stack)
                sample.setMetric("cycles", cycles)
                sample.setMetric("instructions", cycles / 2)
                profile.addSample(sample)
        
        cct.buildFromProfile(profile)
        
        # Verify structure
        assert cct.getNodeCount() > 5
        assert cct.getContextDepth() == 3
        
        # Verify hottest path
        hot_paths = cct.getHotPaths("cycles", top_n=1)
        assert len(hot_paths) == 1
        # matrix_multiply has most cycles (50 * 1000)
        assert hot_paths[0][1] == 50000.0
        
        # Verify function stats
        compute_stats = cct.getFunctionStats("compute")
        assert compute_stats["num_contexts"] >= 1
        
        # Verify metrics
        root = cct.getRoot()
        if root:
            inclusive = cct.getInclusiveMetrics(root.getId())
            # Total cycles should be sum of all samples
            expected_total = (50 * 1000) + (30 * 500) + (20 * 2000) + (15 * 300)
            assert inclusive["cycles"] == expected_total
