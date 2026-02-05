#!/usr/bin/env python3
"""
Unit tests for the dataflow graph system

Tests the core dataflow abstractions:
- Task execution
- Node creation
- Edge connections  
- Graph execution and topological sorting
- Parallel execution
- Caching mechanisms
"""

import unittest
import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from perflow.dataflow import Task, Node, Edge, Graph


class MockTask(Task):
    """Mock task for testing purposes."""
    
    def __init__(self, name, return_value=None, should_fail=False):
        super().__init__(name)
        self.return_value = return_value or f"result_{name}"
        self.should_fail = should_fail
        self.executed = False
        self.execution_count = 0
    
    def execute(self, **inputs):
        """Execute the mock task."""
        self.executed = True
        self.execution_count += 1
        
        if self.should_fail:
            raise RuntimeError(f"Task {self.name} failed intentionally")
        
        # Return either the preset value or process inputs
        if inputs:
            return {f"processed_{k}": v for k, v in inputs.items()}
        return self.return_value


class TestTask(unittest.TestCase):
    """Test Task base class."""
    
    def test_task_creation(self):
        """Test task can be created."""
        task = MockTask("test_task")
        self.assertEqual(task.name, "test_task")
        self.assertFalse(task.executed)
    
    def test_task_execution(self):
        """Test task execution."""
        task = MockTask("test_task", return_value="test_result")
        result = task.execute()
        self.assertEqual(result, "test_result")
        self.assertTrue(task.executed)
    
    def test_task_with_inputs(self):
        """Test task execution with inputs."""
        task = MockTask("test_task")
        result = task.execute(input1="value1", input2="value2")
        self.assertEqual(result["processed_input1"], "value1")
        self.assertEqual(result["processed_input2"], "value2")


class TestNode(unittest.TestCase):
    """Test Node class."""
    
    def test_node_creation(self):
        """Test node can be created with a task."""
        task = MockTask("test_task")
        node = Node(task)
        self.assertEqual(node.task, task)
        self.assertEqual(len(node.inputs), 0)
        self.assertEqual(len(node.outputs), 0)
        self.assertFalse(node._executed)


class TestEdge(unittest.TestCase):
    """Test Edge class."""
    
    def test_edge_creation(self):
        """Test edge can be created between nodes."""
        task1 = MockTask("task1")
        task2 = MockTask("task2")
        node1 = Node(task1)
        node2 = Node(task2)
        
        edge = Edge(node1, node2, "data")
        self.assertEqual(edge.source, node1)
        self.assertEqual(edge.target, node2)
        self.assertEqual(edge.name, "data")


class TestGraph(unittest.TestCase):
    """Test Graph class."""
    
    def test_graph_creation(self):
        """Test graph can be created."""
        graph = Graph("test_graph")
        self.assertEqual(graph.name, "test_graph")
        self.assertEqual(len(graph.nodes), 0)
        self.assertEqual(len(graph.edges), 0)
    
    def test_add_node(self):
        """Test adding nodes to graph."""
        graph = Graph("test_graph")
        task = MockTask("task1")
        node = graph.add_node(task)
        
        self.assertEqual(len(graph.nodes), 1)
        self.assertIn(node, graph.nodes)
        self.assertEqual(node.task, task)
    
    def test_add_edge(self):
        """Test adding edges between nodes."""
        graph = Graph("test_graph")
        task1 = MockTask("task1", return_value={"output": "value1"})
        task2 = MockTask("task2")
        
        node1 = graph.add_node(task1)
        node2 = graph.add_node(task2)
        
        graph.add_edge(node1, node2, "input")
        
        self.assertEqual(len(graph.edges), 1)
        self.assertEqual(len(node1.outputs), 1)
        self.assertEqual(len(node2.inputs), 1)
    
    def test_topological_sort(self):
        """Test topological sorting of graph."""
        graph = Graph("test_graph")
        
        # Create a simple DAG: 1 -> 2 -> 3
        task1 = MockTask("task1", return_value={"data": "value1"})
        task2 = MockTask("task2", return_value={"data": "value2"})
        task3 = MockTask("task3", return_value={"data": "value3"})
        
        node1 = graph.add_node(task1)
        node2 = graph.add_node(task2)
        node3 = graph.add_node(task3)
        
        graph.add_edge(node1, node2, "data")
        graph.add_edge(node2, node3, "data")
        
        sorted_nodes = graph.topological_sort()
        
        # Verify order: node1 should come before node2, node2 before node3
        self.assertEqual(len(sorted_nodes), 3)
        self.assertEqual(sorted_nodes[0], node1)
        self.assertEqual(sorted_nodes[1], node2)
        self.assertEqual(sorted_nodes[2], node3)
    
    def test_diamond_dag(self):
        """Test diamond DAG structure."""
        graph = Graph("test_graph")
        
        # Create a diamond DAG:
        #     1
        #    / \
        #   2   3
        #    \ /
        #     4
        task1 = MockTask("task1", return_value={"data": "value1"})
        task2 = MockTask("task2", return_value={"data": "value2"})
        task3 = MockTask("task3", return_value={"data": "value3"})
        task4 = MockTask("task4")
        
        node1 = graph.add_node(task1)
        node2 = graph.add_node(task2)
        node3 = graph.add_node(task3)
        node4 = graph.add_node(task4)
        
        graph.add_edge(node1, node2, "data1")
        graph.add_edge(node1, node3, "data2")
        graph.add_edge(node2, node4, "data3")
        graph.add_edge(node3, node4, "data4")
        
        # Verify structure
        self.assertEqual(len(graph.nodes), 4)
        self.assertEqual(len(graph.edges), 4)
        self.assertEqual(len(node1.outputs), 2)
        self.assertEqual(len(node4.inputs), 2)
    
    def test_simple_execution(self):
        """Test simple graph execution."""
        graph = Graph("test_graph")
        
        task1 = MockTask("task1", return_value={"result": "success"})
        node1 = graph.add_node(task1)
        
        results = graph.execute()
        
        self.assertTrue(task1.executed)
        self.assertIn(node1, results)
        self.assertEqual(results[node1]["result"], "success")
    
    def test_sequential_execution(self):
        """Test sequential graph execution."""
        graph = Graph("test_graph")
        
        task1 = MockTask("task1", return_value={"data": "value1"})
        task2 = MockTask("task2")
        
        node1 = graph.add_node(task1)
        node2 = graph.add_node(task2)
        
        graph.add_edge(node1, node2, "input_data")
        
        results = graph.execute()
        
        self.assertTrue(task1.executed)
        self.assertTrue(task2.executed)
        self.assertEqual(task1.execution_count, 1)
        self.assertEqual(task2.execution_count, 1)
    
    def test_execution_tracking(self):
        """Test execution tracking."""
        graph = Graph("test_graph")
        
        task = MockTask("task1", return_value={"result": "value"})
        node = graph.add_node(task)
        
        # Execute once
        results = graph.execute()
        self.assertEqual(task.execution_count, 1)
        self.assertTrue(task.executed)
        self.assertTrue(node._executed)
    
    def test_cache_invalidation(self):
        """Test cache invalidation."""
        graph = Graph("test_graph")
        
        task = MockTask("task1", return_value={"result": "value"})
        node = graph.add_node(task)
        
        # Execute once
        results1 = graph.execute()
        self.assertEqual(task.execution_count, 1)
        
        # Reset nodes to allow re-execution
        for n in graph.nodes:
            n.reset()
        
        # Execute again - should re-execute
        results2 = graph.execute()
        self.assertEqual(task.execution_count, 2)
    
    def test_error_handling(self):
        """Test error handling in graph execution."""
        graph = Graph("test_graph")
        
        task = MockTask("failing_task", should_fail=True)
        node = graph.add_node(task)
        
        with self.assertRaises(RuntimeError):
            graph.execute()


class TestGraphUtilities(unittest.TestCase):
    """Test graph utility methods."""
    
    def test_graph_reset(self):
        """Test graph node reset."""
        graph = Graph("test_graph")
        
        task1 = MockTask("task1")
        task2 = MockTask("task2")
        
        node1 = graph.add_node(task1)
        node2 = graph.add_node(task2)
        
        # Execute graph
        graph.execute()
        
        # Verify execution
        self.assertTrue(node1._executed)
        
        # Reset
        for node in graph.nodes:
            node.reset()
        
        # Verify reset
        self.assertFalse(node1._executed)


if __name__ == '__main__':
    unittest.main()
