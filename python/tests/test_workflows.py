#!/usr/bin/env python3
"""
Integration tests for workflows

Tests the pre-built workflows that compose multiple analysis tasks.
"""

import unittest
import sys
import os
import tempfile
import shutil

# Add parent directory to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

# Import perflow modules
try:
    from perflow import workflows
    from perflow.dataflow import Graph
    BINDINGS_AVAILABLE = True
except ImportError as e:
    BINDINGS_AVAILABLE = False
    print(f"Warning: C++ bindings not available: {e}")
    print("Skipping tests that require bindings")


class TestWorkflowsStructure(unittest.TestCase):
    """Test workflows structure without requiring C++ bindings."""
    
    def test_create_basic_analysis_workflow_exists(self):
        """Test create_basic_analysis_workflow function exists."""
        self.assertTrue(hasattr(workflows, 'create_basic_analysis_workflow'))
    
    def test_create_comparative_analysis_workflow_exists(self):
        """Test create_comparative_analysis_workflow function exists."""
        self.assertTrue(hasattr(workflows, 'create_comparative_analysis_workflow'))
    
    def test_create_hotspot_focused_workflow_exists(self):
        """Test create_hotspot_focused_workflow function exists."""
        self.assertTrue(hasattr(workflows, 'create_hotspot_focused_workflow'))


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestBasicAnalysisWorkflow(unittest.TestCase):
    """Test basic analysis workflow creation."""
    
    def setUp(self):
        """Set up test directory."""
        self.test_dir = tempfile.mkdtemp()
        self.output_dir = os.path.join(self.test_dir, "output")
        os.makedirs(self.output_dir)
    
    def tearDown(self):
        """Clean up test directory."""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_workflow_creation(self):
        """Test basic workflow can be created."""
        sample_files = [
            ("/path/to/rank_0.pflw", 0),
            ("/path/to/rank_1.pflw", 1),
        ]
        
        workflow = workflows.create_basic_analysis_workflow(
            sample_files=sample_files,
            output_dir=self.output_dir
        )
        
        self.assertIsInstance(workflow, Graph)
        self.assertGreater(len(workflow.nodes), 0)
    
    def test_workflow_with_options(self):
        """Test basic workflow with various options."""
        sample_files = [("/path/to/rank_0.pflw", 0)]
        
        workflow = workflows.create_basic_analysis_workflow(
            sample_files=sample_files,
            output_dir=self.output_dir,
            top_n=20,
            parallel=True
        )
        
        self.assertIsInstance(workflow, Graph)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestComparativeAnalysisWorkflow(unittest.TestCase):
    """Test comparative analysis workflow creation."""
    
    def setUp(self):
        """Set up test directory."""
        self.test_dir = tempfile.mkdtemp()
        self.output_dir = os.path.join(self.test_dir, "output")
        os.makedirs(self.output_dir)
    
    def tearDown(self):
        """Clean up test directory."""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_workflow_creation(self):
        """Test comparative workflow can be created."""
        sample_file_sets = [
            [("/path/baseline_0.pflw", 0)],
            [("/path/optimized_0.pflw", 0)]
        ]
        labels = ['baseline', 'optimized']
        
        workflow = workflows.create_comparative_analysis_workflow(
            sample_file_sets=sample_file_sets,
            labels=labels,
            output_dir=self.output_dir
        )
        
        self.assertIsInstance(workflow, Graph)
        self.assertGreater(len(workflow.nodes), 0)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestHotspotFocusedWorkflow(unittest.TestCase):
    """Test hotspot-focused workflow creation."""
    
    def setUp(self):
        """Set up test directory."""
        self.test_dir = tempfile.mkdtemp()
        self.output_dir = os.path.join(self.test_dir, "output")
        os.makedirs(self.output_dir)
    
    def tearDown(self):
        """Clean up test directory."""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_workflow_creation(self):
        """Test hotspot-focused workflow can be created."""
        sample_files = [
            ("/path/to/rank_0.pflw", 0),
            ("/path/to/rank_1.pflw", 1),
        ]
        
        workflow = workflows.create_hotspot_focused_workflow(
            sample_files=sample_files,
            output_dir=self.output_dir
        )
        
        self.assertIsInstance(workflow, Graph)
        self.assertGreater(len(workflow.nodes), 0)
    
    def test_workflow_with_options(self):
        """Test hotspot-focused workflow with options."""
        sample_files = [("/path/to/rank_0.pflw", 0)]
        
        workflow = workflows.create_hotspot_focused_workflow(
            sample_files=sample_files,
            output_dir=self.output_dir,
            sample_threshold=50
        )
        
        self.assertIsInstance(workflow, Graph)


if __name__ == '__main__':
    unittest.main()
