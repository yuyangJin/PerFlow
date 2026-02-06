#!/usr/bin/env python3
"""
Test suite for PerFlow Python bindings (pybind11 C++ interface).

This file tests the C++ Python bindings directly, ensuring that:
- All bound classes can be instantiated
- All bound methods work correctly
- Type conversions between Python and C++ work properly
- Enums are properly exposed
- Error handling works as expected

These tests require the C++ bindings to be built and available.
"""

import unittest
import sys
import os

# Try to import the C++ bindings
try:
    from perflow._perflow_bindings import (
        # Enums
        TreeBuildMode, SampleCountMode, ColorScheme,
        # Classes
        ResolvedFrame, TreeNode, PerformanceTree, TreeBuilder,
        TreeVisitor, TreeTraversal,
        BalanceAnalysisResult, HotspotInfo,
        BalanceAnalyzer, HotspotAnalyzer, OnlineAnalysis
    )
    BINDINGS_AVAILABLE = True
except ImportError as e:
    BINDINGS_AVAILABLE = False
    print(f"Warning: C++ bindings not available: {e}")
    print("Binding tests will be skipped. Build the project first to enable these tests.")


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestEnums(unittest.TestCase):
    """Test enum bindings."""
    
    def test_tree_build_mode_enum(self):
        """Test TreeBuildMode enum values."""
        self.assertTrue(hasattr(TreeBuildMode, 'CONTEXT_FREE'))
        self.assertTrue(hasattr(TreeBuildMode, 'CONTEXT_AWARE'))
        
        # Test enum values can be compared
        mode = TreeBuildMode.CONTEXT_FREE
        self.assertEqual(mode, TreeBuildMode.CONTEXT_FREE)
        self.assertNotEqual(mode, TreeBuildMode.CONTEXT_AWARE)
    
    def test_sample_count_mode_enum(self):
        """Test SampleCountMode enum values."""
        self.assertTrue(hasattr(SampleCountMode, 'EXCLUSIVE'))
        self.assertTrue(hasattr(SampleCountMode, 'INCLUSIVE'))
        self.assertTrue(hasattr(SampleCountMode, 'BOTH'))
        
        mode = SampleCountMode.BOTH
        self.assertEqual(mode, SampleCountMode.BOTH)
    
    def test_color_scheme_enum(self):
        """Test ColorScheme enum values."""
        self.assertTrue(hasattr(ColorScheme, 'GRAYSCALE'))
        self.assertTrue(hasattr(ColorScheme, 'HEATMAP'))
        self.assertTrue(hasattr(ColorScheme, 'RAINBOW'))
        
        scheme = ColorScheme.HEATMAP
        self.assertEqual(scheme, ColorScheme.HEATMAP)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestResolvedFrame(unittest.TestCase):
    """Test ResolvedFrame binding."""
    
    def test_resolved_frame_creation(self):
        """Test creating a ResolvedFrame object."""
        frame = ResolvedFrame()
        self.assertIsNotNone(frame)
    
    def test_resolved_frame_attributes(self):
        """Test ResolvedFrame attributes are accessible."""
        frame = ResolvedFrame()
        
        # Test setting attributes
        frame.raw_address = 0x12345678
        frame.offset = 0x1000
        frame.library_name = "libtest.so"
        frame.function_name = "test_function"
        frame.filename = "test.cpp"
        frame.line_number = 42
        
        # Test reading attributes
        self.assertEqual(frame.raw_address, 0x12345678)
        self.assertEqual(frame.offset, 0x1000)
        self.assertEqual(frame.library_name, "libtest.so")
        self.assertEqual(frame.function_name, "test_function")
        self.assertEqual(frame.filename, "test.cpp")
        self.assertEqual(frame.line_number, 42)
    
    def test_resolved_frame_repr(self):
        """Test ResolvedFrame __repr__ method."""
        frame = ResolvedFrame()
        frame.function_name = "my_func"
        frame.library_name = "mylib.so"
        
        repr_str = repr(frame)
        self.assertIn("ResolvedFrame", repr_str)
        self.assertIn("my_func", repr_str)
        self.assertIn("mylib.so", repr_str)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestTreeNode(unittest.TestCase):
    """Test TreeNode binding."""
    
    def test_tree_node_creation(self):
        """Test creating a TreeNode object."""
        node = TreeNode()
        self.assertIsNotNone(node)
    
    def test_tree_node_frame_access(self):
        """Test accessing TreeNode frame."""
        node = TreeNode()
        
        # Frame should be accessible
        frame = node.frame()
        self.assertIsNotNone(frame)
        self.assertIsInstance(frame, ResolvedFrame)
    
    def test_tree_node_sample_counts(self):
        """Test TreeNode sample count methods."""
        node = TreeNode()
        
        # Should have methods for sample counts
        self.assertTrue(hasattr(node, 'get_total_samples'))
        self.assertTrue(hasattr(node, 'get_self_samples'))
        
        # Test getting values (should work even if 0)
        total = node.get_total_samples()
        self.assertIsInstance(total, int)
        self.assertGreaterEqual(total, 0)
        
        self_samples = node.get_self_samples()
        self.assertIsInstance(self_samples, int)
        self.assertGreaterEqual(self_samples, 0)
    
    def test_tree_node_parent_child(self):
        """Test TreeNode parent/child relationships."""
        node = TreeNode()
        
        # Should have parent method
        self.assertTrue(hasattr(node, 'parent'))
        
        # Should have children access
        self.assertTrue(hasattr(node, 'children'))
        children = node.children()
        self.assertIsInstance(children, list)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestPerformanceTree(unittest.TestCase):
    """Test PerformanceTree binding."""
    
    def test_performance_tree_creation(self):
        """Test creating a PerformanceTree object."""
        tree = PerformanceTree()
        self.assertIsNotNone(tree)
    
    def test_performance_tree_root_access(self):
        """Test accessing PerformanceTree root."""
        tree = PerformanceTree()
        
        # Should have root() method
        self.assertTrue(hasattr(tree, 'root'))
        
        # Root should return a TreeNode or None
        root = tree.root()
        # Root might be None for empty tree, or a TreeNode
        if root is not None:
            self.assertIsInstance(root, TreeNode)
    
    def test_performance_tree_node_count(self):
        """Test PerformanceTree node count."""
        tree = PerformanceTree()
        
        # Should have node_count method
        self.assertTrue(hasattr(tree, 'node_count'))
        
        count = tree.node_count()
        self.assertIsInstance(count, int)
        self.assertGreaterEqual(count, 0)
    
    def test_performance_tree_total_samples(self):
        """Test PerformanceTree total samples."""
        tree = PerformanceTree()
        
        # Should have total_samples method
        self.assertTrue(hasattr(tree, 'total_samples'))
        
        total = tree.total_samples()
        self.assertIsInstance(total, int)
        self.assertGreaterEqual(total, 0)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestTreeBuilder(unittest.TestCase):
    """Test TreeBuilder binding."""
    
    def test_tree_builder_creation(self):
        """Test creating a TreeBuilder object."""
        builder = TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.BOTH)
        self.assertIsNotNone(builder)
    
    def test_tree_builder_modes(self):
        """Test TreeBuilder with different modes."""
        # Test CONTEXT_FREE
        builder1 = TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.EXCLUSIVE)
        self.assertIsNotNone(builder1)
        
        # Test CONTEXT_AWARE
        builder2 = TreeBuilder(TreeBuildMode.CONTEXT_AWARE, SampleCountMode.INCLUSIVE)
        self.assertIsNotNone(builder2)
        
        # Test BOTH sample count mode
        builder3 = TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.BOTH)
        self.assertIsNotNone(builder3)
    
    def test_tree_builder_has_build_methods(self):
        """Test TreeBuilder has build methods."""
        builder = TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.BOTH)
        
        # Should have build method
        self.assertTrue(hasattr(builder, 'build'))
        
        # Should have build_from_files method
        self.assertTrue(hasattr(builder, 'build_from_files'))
        
        # Should have build_from_files_parallel method
        self.assertTrue(hasattr(builder, 'build_from_files_parallel'))
    
    def test_tree_builder_returns_performance_tree(self):
        """Test TreeBuilder build returns PerformanceTree."""
        builder = TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.BOTH)
        
        # build() should return a PerformanceTree
        tree = builder.build()
        self.assertIsInstance(tree, PerformanceTree)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestTreeVisitor(unittest.TestCase):
    """Test TreeVisitor binding."""
    
    def test_tree_visitor_is_abstract(self):
        """Test TreeVisitor cannot be instantiated directly."""
        # TreeVisitor is abstract, so we need to subclass it
        class MyVisitor(TreeVisitor):
            def visit(self, node, depth):
                return True
        
        visitor = MyVisitor()
        self.assertIsNotNone(visitor)
        self.assertIsInstance(visitor, TreeVisitor)
    
    def test_tree_visitor_subclass(self):
        """Test creating a TreeVisitor subclass."""
        visited_nodes = []
        
        class CountingVisitor(TreeVisitor):
            def visit(self, node, depth):
                visited_nodes.append((node, depth))
                return True
        
        visitor = CountingVisitor()
        
        # Create a dummy node and test visit
        node = TreeNode()
        result = visitor.visit(node, 0)
        
        self.assertTrue(result)
        self.assertEqual(len(visited_nodes), 1)
        self.assertEqual(visited_nodes[0][1], 0)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestTreeTraversal(unittest.TestCase):
    """Test TreeTraversal binding."""
    
    def test_tree_traversal_has_methods(self):
        """Test TreeTraversal has traversal methods."""
        self.assertTrue(hasattr(TreeTraversal, 'traverse_dfs_pre'))
        self.assertTrue(hasattr(TreeTraversal, 'traverse_dfs_post'))
        self.assertTrue(hasattr(TreeTraversal, 'traverse_bfs'))
        self.assertTrue(hasattr(TreeTraversal, 'find_node'))
        self.assertTrue(hasattr(TreeTraversal, 'find_nodes_if'))


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestBalanceAnalysisResult(unittest.TestCase):
    """Test BalanceAnalysisResult binding."""
    
    def test_balance_result_creation(self):
        """Test creating a BalanceAnalysisResult object."""
        result = BalanceAnalysisResult()
        self.assertIsNotNone(result)
    
    def test_balance_result_attributes(self):
        """Test BalanceAnalysisResult attributes."""
        result = BalanceAnalysisResult()
        
        # Should have these attributes matching the C++ struct
        self.assertTrue(hasattr(result, 'mean_samples'))
        self.assertTrue(hasattr(result, 'std_dev_samples'))
        self.assertTrue(hasattr(result, 'min_samples'))
        self.assertTrue(hasattr(result, 'max_samples'))
        self.assertTrue(hasattr(result, 'imbalance_factor'))
        self.assertTrue(hasattr(result, 'most_loaded_process'))
        self.assertTrue(hasattr(result, 'least_loaded_process'))
        self.assertTrue(hasattr(result, 'process_samples'))


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestHotspotInfo(unittest.TestCase):
    """Test HotspotInfo binding."""
    
    def test_hotspot_info_creation(self):
        """Test creating a HotspotInfo object."""
        info = HotspotInfo()
        self.assertIsNotNone(info)
    
    def test_hotspot_info_attributes(self):
        """Test HotspotInfo attributes."""
        info = HotspotInfo()
        
        # Should have these attributes matching the C++ struct
        self.assertTrue(hasattr(info, 'function_name'))
        self.assertTrue(hasattr(info, 'library_name'))
        self.assertTrue(hasattr(info, 'source_location'))
        self.assertTrue(hasattr(info, 'total_samples'))
        self.assertTrue(hasattr(info, 'percentage'))
        self.assertTrue(hasattr(info, 'self_samples'))
        self.assertTrue(hasattr(info, 'self_percentage'))


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestBalanceAnalyzer(unittest.TestCase):
    """Test BalanceAnalyzer binding."""
    
    def test_balance_analyzer_is_static_class(self):
        """Test BalanceAnalyzer provides static analyze method."""
        # BalanceAnalyzer is a utility class with static methods only
        self.assertTrue(hasattr(BalanceAnalyzer, 'analyze'))
    
    def test_balance_analyzer_analyze_is_callable(self):
        """Test BalanceAnalyzer.analyze is callable."""
        # Should be callable as a static method
        self.assertTrue(callable(BalanceAnalyzer.analyze))


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestHotspotAnalyzer(unittest.TestCase):
    """Test HotspotAnalyzer binding."""
    
    def test_hotspot_analyzer_is_static_class(self):
        """Test HotspotAnalyzer provides static analysis methods."""
        # HotspotAnalyzer is a utility class with static methods only
        self.assertTrue(hasattr(HotspotAnalyzer, 'find_hotspots'))
    
    def test_hotspot_analyzer_has_methods(self):
        """Test HotspotAnalyzer has analysis methods."""
        # Should have static methods
        self.assertTrue(hasattr(HotspotAnalyzer, 'find_hotspots'))
        
        # Methods should be callable
        self.assertTrue(callable(HotspotAnalyzer.find_hotspots))


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestOnlineAnalysis(unittest.TestCase):
    """Test OnlineAnalysis binding."""
    
    def test_online_analysis_creation(self):
        """Test creating an OnlineAnalysis object."""
        analysis = OnlineAnalysis()
        self.assertIsNotNone(analysis)
    
    def test_online_analysis_has_methods(self):
        """Test OnlineAnalysis has required methods."""
        analysis = OnlineAnalysis()
        
        # Should have analysis methods
        self.assertTrue(hasattr(analysis, 'set_sample_dir'))
        self.assertTrue(hasattr(analysis, 'set_output_dir'))


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestTypeConversions(unittest.TestCase):
    """Test Python-C++ type conversions."""
    
    def test_string_conversion(self):
        """Test string conversion between Python and C++."""
        frame = ResolvedFrame()
        
        # Python string -> C++ string
        test_str = "test_function_name"
        frame.function_name = test_str
        
        # C++ string -> Python string
        result = frame.function_name
        self.assertEqual(result, test_str)
        self.assertIsInstance(result, str)
    
    def test_int_conversion(self):
        """Test integer conversion between Python and C++."""
        frame = ResolvedFrame()
        
        # Python int -> C++ uint64_t
        test_val = 0xDEADBEEF
        frame.raw_address = test_val
        
        # C++ uint64_t -> Python int
        result = frame.raw_address
        self.assertEqual(result, test_val)
        self.assertIsInstance(result, int)
    
    def test_list_conversion(self):
        """Test list conversion for children nodes."""
        node = TreeNode()
        
        # C++ vector -> Python list
        children = node.children()
        self.assertIsInstance(children, list)
    
    def test_enum_conversion(self):
        """Test enum can be used in function calls."""
        # TreeBuildMode and SampleCountMode should be usable
        builder = TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.BOTH)
        self.assertIsNotNone(builder)
        
        # Different enum values should work
        builder2 = TreeBuilder(TreeBuildMode.CONTEXT_AWARE, SampleCountMode.EXCLUSIVE)
        self.assertIsNotNone(builder2)


@unittest.skipUnless(BINDINGS_AVAILABLE, "Requires C++ bindings")
class TestErrorHandling(unittest.TestCase):
    """Test error handling in bindings."""
    
    def test_invalid_enum_value(self):
        """Test that invalid enum values raise appropriate errors."""
        # This should work
        builder = TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.BOTH)
        self.assertIsNotNone(builder)
        
        # Invalid enum value should raise TypeError
        with self.assertRaises(TypeError):
            TreeBuilder("invalid", SampleCountMode.BOTH)
        
        with self.assertRaises(TypeError):
            TreeBuilder(TreeBuildMode.CONTEXT_FREE, "invalid")
    
    def test_wrong_argument_count(self):
        """Test that wrong argument count raises TypeError."""
        # TreeBuilder has default parameters, so it can be called with 0, 1, or 2 args
        # This should work - no args (uses defaults)
        builder1 = TreeBuilder()
        self.assertIsNotNone(builder1)
        
        # This should work - 1 arg
        builder2 = TreeBuilder(TreeBuildMode.CONTEXT_FREE)
        self.assertIsNotNone(builder2)
        
        # This should work - 2 args
        builder3 = TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.BOTH)
        self.assertIsNotNone(builder3)
        
        # But too many args should fail
        with self.assertRaises(TypeError):
            TreeBuilder(TreeBuildMode.CONTEXT_FREE, SampleCountMode.BOTH, "extra")


def run_tests():
    """Run all binding tests."""
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(sys.modules[__name__])
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Print summary
    print("\n" + "="*70)
    print("Binding Test Summary")
    print("="*70)
    print(f"Tests run: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print(f"Skipped: {len(result.skipped)}")
    
    if not BINDINGS_AVAILABLE:
        print("\nNote: C++ bindings not available. Most tests were skipped.")
        print("Build the project with Python bindings to run all tests.")
    
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_tests())
