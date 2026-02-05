#!/usr/bin/env python3
"""
Unit tests for analysis tasks

Tests the pre-built analysis tasks that wrap C++ backend functionality.
Note: These tests check task structure and logic, but full integration
with C++ bindings requires the bindings to be built.
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
    from perflow import analysis
    from perflow.dataflow import Task
    BINDINGS_AVAILABLE = True
except ImportError as e:
    BINDINGS_AVAILABLE = False
    print(f"Warning: C++ bindings not available: {e}")
    print("Skipping tests that require bindings")


class TestAnalysisTaskStructure(unittest.TestCase):
    """Test analysis task structure without requiring C++ bindings."""
    
    def test_load_tree_task_exists(self):
        """Test LoadTreeTask class exists."""
        self.assertTrue(hasattr(analysis, 'LoadTreeTask'))
    
    def test_balance_analysis_task_exists(self):
        """Test BalanceAnalysisTask class exists."""
        self.assertTrue(hasattr(analysis, 'BalanceAnalysisTask'))
    
    def test_hotspot_analysis_task_exists(self):
        """Test HotspotAnalysisTask class exists."""
        self.assertTrue(hasattr(analysis, 'HotspotAnalysisTask'))
    
    def test_filter_nodes_task_exists(self):
        """Test FilterNodesTask class exists."""
        self.assertTrue(hasattr(analysis, 'FilterNodesTask'))
    
    def test_traverse_tree_task_exists(self):
        """Test TraverseTreeTask class exists."""
        self.assertTrue(hasattr(analysis, 'TraverseTreeTask'))
    
    def test_export_visualization_task_exists(self):
        """Test ExportVisualizationTask class exists."""
        self.assertTrue(hasattr(analysis, 'ExportVisualizationTask'))
    
    def test_report_task_exists(self):
        """Test ReportTask class exists."""
        self.assertTrue(hasattr(analysis, 'ReportTask'))
    
    def test_aggregate_results_task_exists(self):
        """Test AggregateResultsTask class exists."""
        self.assertTrue(hasattr(analysis, 'AggregateResultsTask'))


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestLoadTreeTask(unittest.TestCase):
    """Test LoadTreeTask functionality."""
    
    def test_task_creation(self):
        """Test LoadTreeTask can be created."""
        task = analysis.LoadTreeTask(
            sample_files=[("/path/file.pflw", 0)],
            name="test_load"
        )
        self.assertEqual(task.name, "test_load")
        self.assertIsInstance(task, Task)
    
    def test_task_with_parallel(self):
        """Test LoadTreeTask with parallel option."""
        task = analysis.LoadTreeTask(
            sample_files=[("/path/file.pflw", 0)],
            parallel=True
        )
        self.assertTrue(task.parallel)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestBalanceAnalysisTask(unittest.TestCase):
    """Test BalanceAnalysisTask functionality."""
    
    def test_task_creation(self):
        """Test BalanceAnalysisTask can be created."""
        task = analysis.BalanceAnalysisTask(name="test_balance")
        self.assertEqual(task.name, "test_balance")
        self.assertIsInstance(task, Task)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestHotspotAnalysisTask(unittest.TestCase):
    """Test HotspotAnalysisTask functionality."""
    
    def test_task_creation_self(self):
        """Test HotspotAnalysisTask can be created for self hotspots."""
        task = analysis.HotspotAnalysisTask(
            mode='self',
            top_n=10,
            name="test_hotspot"
        )
        self.assertEqual(task.name, "test_hotspot")
        self.assertEqual(task.mode, 'self')
        self.assertEqual(task.top_n, 10)
    
    def test_task_creation_total(self):
        """Test HotspotAnalysisTask can be created for total hotspots."""
        task = analysis.HotspotAnalysisTask(
            mode='total',
            top_n=20
        )
        self.assertEqual(task.mode, 'total')
        self.assertEqual(task.top_n, 20)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestReportTask(unittest.TestCase):
    """Test ReportTask functionality."""
    
    def setUp(self):
        """Set up test directory."""
        self.test_dir = tempfile.mkdtemp()
    
    def tearDown(self):
        """Clean up test directory."""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_task_creation_text(self):
        """Test ReportTask can be created for text format."""
        output_path = os.path.join(self.test_dir, "report.txt")
        task = analysis.ReportTask(
            output_path=output_path,
            format='text'
        )
        self.assertEqual(task.format, 'text')
        self.assertEqual(task.output_path, output_path)
    
    def test_task_creation_json(self):
        """Test ReportTask can be created for JSON format."""
        output_path = os.path.join(self.test_dir, "report.json")
        task = analysis.ReportTask(
            output_path=output_path,
            format='json'
        )
        self.assertEqual(task.format, 'json')
    
    def test_task_creation_html(self):
        """Test ReportTask can be created for HTML format."""
        output_path = os.path.join(self.test_dir, "report.html")
        task = analysis.ReportTask(
            output_path=output_path,
            format='html'
        )
        self.assertEqual(task.format, 'html')


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestFilterNodesTask(unittest.TestCase):
    """Test FilterNodesTask functionality."""
    
    def test_task_creation_with_callable(self):
        """Test FilterNodesTask can be created with callable predicate."""
        predicate = lambda node: True
        task = analysis.FilterNodesTask(
            predicate=predicate,
            name="test_filter"
        )
        self.assertEqual(task.name, "test_filter")
        self.assertEqual(task.predicate, predicate)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestTraverseTreeTask(unittest.TestCase):
    """Test TraverseTreeTask functionality."""
    
    def test_task_creation_dfs(self):
        """Test TraverseTreeTask with DFS strategy."""
        task = analysis.TraverseTreeTask(
            traversal_mode='dfs_pre',
            visitor=lambda node: None
        )
        self.assertEqual(task.traversal_mode, 'dfs_pre')
    
    def test_task_creation_bfs(self):
        """Test TraverseTreeTask with BFS strategy."""
        task = analysis.TraverseTreeTask(
            traversal_mode='bfs',
            visitor=lambda node: None
        )
        self.assertEqual(task.traversal_mode, 'bfs')


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestExportVisualizationTask(unittest.TestCase):
    """Test ExportVisualizationTask functionality."""
    
    def setUp(self):
        """Set up test directory."""
        self.test_dir = tempfile.mkdtemp()
    
    def tearDown(self):
        """Clean up test directory."""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_task_creation(self):
        """Test ExportVisualizationTask can be created."""
        output_path = os.path.join(self.test_dir, "viz.pdf")
        task = analysis.ExportVisualizationTask(
            output_path=output_path
        )
        self.assertEqual(task.output_path, output_path)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestAggregateResultsTask(unittest.TestCase):
    """Test AggregateResultsTask functionality."""
    
    def test_task_creation(self):
        """Test AggregateResultsTask can be created."""
        def aggregate_func(**inputs):
            return list(inputs.values())
        
        task = analysis.AggregateResultsTask(
            aggregation_func=aggregate_func,
            name="test_aggregate"
        )
        self.assertEqual(task.name, "test_aggregate")
        self.assertEqual(task.aggregation_func, aggregate_func)


if __name__ == '__main__':
    unittest.main()
