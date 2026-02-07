#!/usr/bin/env python3
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

"""
Unit tests for the PerFlow Dataflow Framework

These tests verify the dataflow graph system, nodes, executors, and builder.
They use mock data to avoid dependency on the native C++ module.
"""

import unittest
import threading
import time
import sys
import os
from typing import Any, Dict

# Add the python directory to path for direct imports
script_dir = os.path.dirname(os.path.abspath(__file__))
python_dir = os.path.join(os.path.dirname(script_dir), 'python')
if os.path.exists(python_dir):
    sys.path.insert(0, python_dir)

# Import dataflow components directly from submodules to avoid C++ module dependency
# Use explicit path-based imports
import importlib.util

def load_module(module_name, file_path):
    """Load a module directly from file path."""
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module

# Load the dataflow modules directly
dataflow_dir = os.path.join(python_dir, 'perflow', 'dataflow')
graph_module = load_module('perflow.dataflow.graph', os.path.join(dataflow_dir, 'graph.py'))
executor_module = load_module('perflow.dataflow.executor', os.path.join(dataflow_dir, 'executor.py'))
nodes_module = load_module('perflow.dataflow.nodes', os.path.join(dataflow_dir, 'nodes.py'))
builder_module = load_module('perflow.dataflow.builder', os.path.join(dataflow_dir, 'builder.py'))

# Import from the loaded modules
DataflowGraph = graph_module.DataflowGraph
DataflowNode = graph_module.DataflowNode
DataflowEdge = graph_module.DataflowEdge
NodeState = graph_module.NodeState

GraphExecutor = executor_module.GraphExecutor
ParallelExecutor = executor_module.ParallelExecutor
CachingExecutor = executor_module.CachingExecutor

CustomNode = nodes_module.CustomNode
TransformNode = nodes_module.TransformNode
MergeNode = nodes_module.MergeNode

WorkflowBuilder = builder_module.WorkflowBuilder


# ============================================================================
# Mock Classes for Testing (avoids C++ dependency)
# ============================================================================

class MockNode(DataflowNode):
    """A simple mock node for testing."""
    
    def __init__(self, name: str, value: Any = None, delay: float = 0):
        super().__init__(
            name=name,
            inputs={},
            outputs={'value': 'Any'}
        )
        self._value = value
        self._delay = delay
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        if self._delay > 0:
            time.sleep(self._delay)
        return {'value': self._value}


class MockProcessNode(DataflowNode):
    """A mock node that processes input."""
    
    def __init__(self, name: str, transform=None):
        super().__init__(
            name=name,
            inputs={'value': 'Any'},
            outputs={'result': 'Any'}
        )
        self._transform = transform or (lambda x: x)
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        self.validate_inputs(inputs)
        result = self._transform(inputs['value'])
        return {'result': result}


class MockMultiInputNode(DataflowNode):
    """A mock node with multiple inputs."""
    
    def __init__(self, name: str):
        super().__init__(
            name=name,
            inputs={'a': 'Any', 'b': 'Any'},
            outputs={'sum': 'Any'}
        )
    
    def execute(self, inputs: Dict[str, Any]) -> Dict[str, Any]:
        self.validate_inputs(inputs)
        return {'sum': inputs['a'] + inputs['b']}


# ============================================================================
# DataflowNode Tests
# ============================================================================

class TestDataflowNode(unittest.TestCase):
    """Tests for DataflowNode base class."""
    
    def test_node_creation(self):
        """Test basic node creation."""
        node = MockNode("TestNode", value=42)
        
        self.assertEqual(node.name, "TestNode")
        self.assertIsNotNone(node.node_id)
        self.assertEqual(node.state, NodeState.PENDING)
        self.assertEqual(node.outputs, {'value': 'Any'})
    
    def test_node_execution(self):
        """Test node execution."""
        node = MockNode("TestNode", value="hello")
        
        result = node.execute({})
        
        self.assertEqual(result, {'value': 'hello'})
    
    def test_node_reset(self):
        """Test node reset."""
        node = MockNode("TestNode", value=42)
        node.state = NodeState.COMPLETED
        node.result = {'value': 42}
        
        node.reset()
        
        self.assertEqual(node.state, NodeState.PENDING)
        self.assertIsNone(node.result)
    
    def test_validate_inputs(self):
        """Test input validation."""
        node = MockProcessNode("Process")
        
        # Should pass with required input
        self.assertTrue(node.validate_inputs({'value': 1}))
        
        # Should fail without required input
        with self.assertRaises(ValueError):
            node.validate_inputs({})
    
    def test_unique_node_ids(self):
        """Test that each node gets a unique ID."""
        nodes = [MockNode(f"Node", value=i) for i in range(10)]
        ids = [n.node_id for n in nodes]
        
        self.assertEqual(len(ids), len(set(ids)))


# ============================================================================
# DataflowEdge Tests
# ============================================================================

class TestDataflowEdge(unittest.TestCase):
    """Tests for DataflowEdge class."""
    
    def test_edge_creation(self):
        """Test edge creation."""
        source = MockNode("Source", value=1)
        target = MockProcessNode("Target")
        
        edge = DataflowEdge(source, 'value', target, 'value')
        
        self.assertEqual(edge.source_node, source)
        self.assertEqual(edge.source_port, 'value')
        self.assertEqual(edge.target_node, target)
        self.assertEqual(edge.target_port, 'value')
    
    def test_invalid_source_port(self):
        """Test edge creation with invalid source port."""
        source = MockNode("Source", value=1)
        target = MockProcessNode("Target")
        
        with self.assertRaises(ValueError):
            DataflowEdge(source, 'invalid_port', target, 'value')
    
    def test_invalid_target_port(self):
        """Test edge creation with invalid target port."""
        source = MockNode("Source", value=1)
        target = MockProcessNode("Target")
        
        with self.assertRaises(ValueError):
            DataflowEdge(source, 'value', target, 'invalid_port')


# ============================================================================
# DataflowGraph Tests
# ============================================================================

class TestDataflowGraph(unittest.TestCase):
    """Tests for DataflowGraph class."""
    
    def test_graph_creation(self):
        """Test graph creation."""
        graph = DataflowGraph(name="TestGraph")
        
        self.assertEqual(graph.name, "TestGraph")
        self.assertEqual(len(graph), 0)
    
    def test_add_node(self):
        """Test adding nodes to graph."""
        graph = DataflowGraph()
        node = MockNode("Test", value=1)
        
        graph.add_node(node)
        
        self.assertEqual(len(graph), 1)
        self.assertIn(node, graph.nodes)
    
    def test_add_duplicate_node(self):
        """Test adding duplicate node raises error."""
        graph = DataflowGraph()
        node = MockNode("Test", value=1)
        
        graph.add_node(node)
        
        with self.assertRaises(ValueError):
            graph.add_node(node)
    
    def test_connect_nodes(self):
        """Test connecting nodes."""
        graph = DataflowGraph()
        source = MockNode("Source", value=1)
        target = MockProcessNode("Target")
        
        graph.add_node(source)
        graph.add_node(target)
        graph.connect(source, target, 'value')
        
        self.assertEqual(len(graph.edges), 1)
    
    def test_connect_nodes_not_in_graph(self):
        """Test connecting nodes not in graph raises error."""
        graph = DataflowGraph()
        source = MockNode("Source", value=1)
        target = MockProcessNode("Target")
        
        with self.assertRaises(ValueError):
            graph.connect(source, target, 'value')
    
    def test_topological_sort_linear(self):
        """Test topological sort on linear graph."""
        graph = DataflowGraph()
        n1 = MockNode("N1", value=1)
        n2 = MockProcessNode("N2")
        n3 = MockProcessNode("N3")
        
        graph.add_node(n1)
        graph.add_node(n2)
        graph.add_node(n3)
        graph.connect(n1, n2, 'value')
        graph.connect(n2, n3, 'result', target_port='value')
        
        order = graph.topological_sort()
        
        self.assertEqual(order, [n1, n2, n3])
    
    def test_topological_sort_diamond(self):
        """Test topological sort on diamond-shaped graph."""
        graph = DataflowGraph()
        source = MockNode("Source", value=10)
        left = MockProcessNode("Left", transform=lambda x: x * 2)
        right = MockProcessNode("Right", transform=lambda x: x + 5)
        merge = MockMultiInputNode("Merge")
        
        graph.add_node(source)
        graph.add_node(left)
        graph.add_node(right)
        graph.add_node(merge)
        
        graph.connect(source, left, 'value')
        graph.connect(source, right, 'value')
        graph.connect(left, merge, 'result', target_port='a')
        graph.connect(right, merge, 'result', target_port='b')
        
        order = graph.topological_sort()
        
        # Source must be first, merge must be last
        self.assertEqual(order[0], source)
        self.assertEqual(order[-1], merge)
        # Left and right can be in any order
        self.assertIn(left, order[1:3])
        self.assertIn(right, order[1:3])
    
    def test_cycle_detection(self):
        """Test that cycles are detected."""
        graph = DataflowGraph()
        n1 = MockProcessNode("N1")
        n2 = MockProcessNode("N2")
        
        # Create nodes with compatible ports for cycle
        n1._inputs = {'value': 'Any'}
        n1._outputs = {'result': 'Any'}
        n2._inputs = {'value': 'Any'}
        n2._outputs = {'result': 'Any'}
        
        graph.add_node(n1)
        graph.add_node(n2)
        graph.connect(n1, n2, 'result', target_port='value')
        graph.connect(n2, n1, 'result', target_port='value')
        
        with self.assertRaises(ValueError):
            graph.topological_sort()
    
    def test_get_source_and_sink_nodes(self):
        """Test getting source and sink nodes."""
        graph = DataflowGraph()
        source = MockNode("Source", value=1)
        middle = MockProcessNode("Middle")
        sink = MockProcessNode("Sink")
        
        graph.add_node(source)
        graph.add_node(middle)
        graph.add_node(sink)
        graph.connect(source, middle, 'value')
        graph.connect(middle, sink, 'result', target_port='value')
        
        self.assertEqual(graph.get_source_nodes(), [source])
        self.assertEqual(graph.get_sink_nodes(), [sink])
    
    def test_parallel_groups(self):
        """Test parallel execution grouping."""
        graph = DataflowGraph()
        source = MockNode("Source", value=10)
        p1 = MockProcessNode("P1")
        p2 = MockProcessNode("P2")
        p3 = MockProcessNode("P3")
        
        graph.add_node(source)
        graph.add_node(p1)
        graph.add_node(p2)
        graph.add_node(p3)
        
        # P1, P2, P3 all depend only on source - can run in parallel
        graph.connect(source, p1, 'value')
        graph.connect(source, p2, 'value')
        graph.connect(source, p3, 'value')
        
        groups = graph.get_parallel_groups()
        
        self.assertEqual(len(groups), 2)
        self.assertEqual(groups[0], [source])
        self.assertEqual(set(groups[1]), {p1, p2, p3})


# ============================================================================
# GraphExecutor Tests
# ============================================================================

class TestGraphExecutor(unittest.TestCase):
    """Tests for graph execution."""
    
    def test_simple_execution(self):
        """Test simple linear graph execution."""
        graph = DataflowGraph()
        source = MockNode("Source", value=10)
        process = MockProcessNode("Process", transform=lambda x: x * 2)
        
        graph.add_node(source)
        graph.add_node(process)
        graph.connect(source, process, 'value')
        
        results = graph.execute()
        
        self.assertEqual(results[process.node_id]['result'], 20)
    
    def test_diamond_execution(self):
        """Test diamond-shaped graph execution."""
        graph = DataflowGraph()
        source = MockNode("Source", value=10)
        left = MockProcessNode("Left", transform=lambda x: x * 2)
        right = MockProcessNode("Right", transform=lambda x: x + 5)
        merge = MockMultiInputNode("Merge")
        
        graph.add_node(source)
        graph.add_node(left)
        graph.add_node(right)
        graph.add_node(merge)
        
        graph.connect(source, left, 'value')
        graph.connect(source, right, 'value')
        graph.connect(left, merge, 'result', target_port='a')
        graph.connect(right, merge, 'result', target_port='b')
        
        results = graph.execute()
        
        # left: 10 * 2 = 20
        # right: 10 + 5 = 15
        # merge: 20 + 15 = 35
        self.assertEqual(results[merge.node_id]['sum'], 35)
    
    def test_progress_callback(self):
        """Test progress callback is called."""
        graph = DataflowGraph()
        for i in range(3):
            graph.add_node(MockNode(f"Node{i}", value=i))
        
        progress_calls = []
        
        def progress_callback(completed, total, current):
            progress_calls.append((completed, total, current))
        
        executor = GraphExecutor()
        executor.set_progress_callback(progress_callback)
        graph.execute(executor=executor)
        
        self.assertGreater(len(progress_calls), 0)


# ============================================================================
# ParallelExecutor Tests
# ============================================================================

class TestParallelExecutor(unittest.TestCase):
    """Tests for parallel execution."""
    
    def test_parallel_execution(self):
        """Test that independent nodes run in parallel."""
        graph = DataflowGraph()
        source = MockNode("Source", value=1)
        
        # Add nodes with small delays
        p1 = MockNode("P1", value=1, delay=0.1)
        p2 = MockNode("P2", value=2, delay=0.1)
        p3 = MockNode("P3", value=3, delay=0.1)
        
        # Modify nodes to accept input
        for node in [p1, p2, p3]:
            node._inputs = {'value': 'Any'}
        
        graph.add_node(source)
        graph.add_node(p1)
        graph.add_node(p2)
        graph.add_node(p3)
        
        graph.connect(source, p1, 'value')
        graph.connect(source, p2, 'value')
        graph.connect(source, p3, 'value')
        
        executor = ParallelExecutor(max_workers=3)
        
        start = time.time()
        results = graph.execute(executor=executor)
        elapsed = time.time() - start
        
        # If truly parallel, should take ~0.1s, not 0.3s
        # Allow some overhead
        self.assertLess(elapsed, 0.25)
    
    def test_parallel_respects_dependencies(self):
        """Test that parallel execution respects dependencies."""
        graph = DataflowGraph()
        source = MockNode("Source", value=5)
        process = MockProcessNode("Process", transform=lambda x: x * 2)
        
        graph.add_node(source)
        graph.add_node(process)
        graph.connect(source, process, 'value')
        
        executor = ParallelExecutor(max_workers=4)
        results = graph.execute(executor=executor)
        
        # Process depends on source, so result should be correct
        self.assertEqual(results[process.node_id]['result'], 10)


# ============================================================================
# CachingExecutor Tests
# ============================================================================

class TestCachingExecutor(unittest.TestCase):
    """Tests for caching execution."""
    
    def test_caching(self):
        """Test that results are cached."""
        graph = DataflowGraph()
        
        execution_count = [0]
        
        class CountingNode(DataflowNode):
            def __init__(self):
                super().__init__(name="Counter", inputs={}, outputs={'value': 'int'})
            
            def execute(self, inputs):
                execution_count[0] += 1
                return {'value': 42}
        
        graph.add_node(CountingNode())
        
        executor = CachingExecutor()
        
        # First execution
        results1 = graph.execute(executor=executor)
        self.assertEqual(execution_count[0], 1)
        
        # Reset graph state
        graph.reset()
        
        # Second execution should use cache
        results2 = graph.execute(executor=executor)
        self.assertEqual(execution_count[0], 1)  # Still 1, cached
        
        stats = executor.get_cache_stats()
        self.assertEqual(stats['cache_hits'], 1)
        self.assertEqual(stats['cache_misses'], 1)
    
    def test_force_recompute(self):
        """Test force recompute bypasses cache."""
        graph = DataflowGraph()
        
        execution_count = [0]
        
        class CountingNode(DataflowNode):
            def __init__(self):
                super().__init__(name="Counter", inputs={}, outputs={'value': 'int'})
            
            def execute(self, inputs):
                execution_count[0] += 1
                return {'value': 42}
        
        graph.add_node(CountingNode())
        
        executor = CachingExecutor()
        
        # First execution
        graph.execute(executor=executor)
        self.assertEqual(execution_count[0], 1)
        
        graph.reset()
        
        # Second execution with force_recompute
        graph.execute(executor=executor, force_recompute=True)
        self.assertEqual(execution_count[0], 2)
    
    def test_cache_eviction(self):
        """Test LRU cache eviction."""
        executor = CachingExecutor(max_cache_size=2)
        
        # Create multiple graphs to fill cache
        for i in range(3):
            graph = DataflowGraph()
            graph.add_node(MockNode(f"Node{i}", value=i))
            graph.execute(executor=executor)
        
        stats = executor.get_cache_stats()
        self.assertLessEqual(stats['cache_size'], 2)


# ============================================================================
# Built-in Node Tests
# ============================================================================

class TestBuiltinNodes(unittest.TestCase):
    """Tests for built-in node types."""
    
    def test_custom_node(self):
        """Test CustomNode execution."""
        node = CustomNode(
            execute_fn=lambda inputs: {'result': inputs.get('value', 0) * 2},
            input_ports={'value': 'int'},
            output_ports={'result': 'int'},
            name="DoubleIt"
        )
        
        result = node.execute({'value': 21})
        self.assertEqual(result, {'result': 42})
    
    def test_transform_node(self):
        """Test TransformNode execution."""
        node = TransformNode(
            transform=lambda inputs: {'upper': inputs['text'].upper()},
            input_ports={'text': 'str'},
            output_ports={'upper': 'str'},
            name="ToUpper"
        )
        
        result = node.execute({'text': 'hello'})
        self.assertEqual(result, {'upper': 'HELLO'})
    
    def test_merge_node(self):
        """Test MergeNode execution."""
        node = MergeNode(
            input_ports=['a', 'b', 'c'],
            name="MergeThree"
        )
        
        result = node.execute({'a': 1, 'b': 2, 'c': 3})
        self.assertEqual(result['merged'], {'a': 1, 'b': 2, 'c': 3})
        self.assertEqual(set(result['keys']), {'a', 'b', 'c'})


# ============================================================================
# WorkflowBuilder Tests
# ============================================================================

class TestWorkflowBuilder(unittest.TestCase):
    """Tests for WorkflowBuilder."""
    
    def test_fluent_api(self):
        """Test fluent API returns self."""
        builder = WorkflowBuilder("Test")
        
        # All methods should return self
        result = builder.custom(
            execute_fn=lambda x: {'result': 1},
            name="Custom1"
        )
        self.assertIs(result, builder)
    
    def test_build_returns_graph(self):
        """Test build() returns a DataflowGraph."""
        builder = WorkflowBuilder("Test")
        builder.custom(
            execute_fn=lambda x: {'result': 1},
            name="Custom1"
        )
        
        graph = builder.build()
        
        self.assertIsInstance(graph, DataflowGraph)
        self.assertEqual(len(graph), 1)
    
    def test_custom_workflow(self):
        """Test building and executing a custom workflow."""
        builder = WorkflowBuilder("CustomWorkflow")
        
        # Create a simple workflow with custom nodes
        source_node = CustomNode(
            execute_fn=lambda _: {'value': 100},
            input_ports={},
            output_ports={'value': 'int'},
            name="Source"
        )
        
        process_node = CustomNode(
            execute_fn=lambda inputs: {'result': inputs['value'] * 2},
            input_ports={'value': 'int'},
            output_ports={'result': 'int'},
            name="Process"
        )
        
        builder.add_node(source_node)
        builder.add_node(process_node)
        builder.connect('source', 'process', 'value')
        
        results = builder.execute()
        
        self.assertEqual(results[process_node.node_id]['result'], 200)


# ============================================================================
# Thread Safety Tests
# ============================================================================

class TestThreadSafety(unittest.TestCase):
    """Tests for thread safety."""
    
    def test_concurrent_node_creation(self):
        """Test creating nodes from multiple threads."""
        nodes = []
        errors = []
        
        def create_nodes(thread_id):
            try:
                for i in range(10):
                    node = MockNode(f"Thread{thread_id}_Node{i}", value=i)
                    nodes.append(node)
            except Exception as e:
                errors.append(e)
        
        threads = [threading.Thread(target=create_nodes, args=(i,)) for i in range(5)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        
        self.assertEqual(len(errors), 0)
        self.assertEqual(len(nodes), 50)
        
        # All node IDs should be unique
        ids = [n.node_id for n in nodes]
        self.assertEqual(len(ids), len(set(ids)))


if __name__ == '__main__':
    unittest.main()
