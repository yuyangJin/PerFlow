#!/usr/bin/env python3
"""
Example: Dataflow Graph Construction

This script demonstrates how to manually construct a dataflow graph
for custom analysis workflows.
"""

import sys
import os

# Add parent directory to path for development
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from perflow.dataflow import Graph, Task


class PrintTask(Task):
    """Simple task that prints its inputs."""
    
    def __init__(self, message: str, name: str = None):
        super().__init__(name or "PrintTask")
        self.message = message
    
    def execute(self, **inputs):
        print(f"\n[{self.name}] {self.message}")
        for key, value in inputs.items():
            print(f"  {key}: {str(value)[:100]}...")
        return f"{self.message} - processed"


def main():
    """Demonstrate dataflow graph construction."""
    print("=" * 80)
    print("PerFlow Dataflow Graph Example")
    print("=" * 80)
    print()
    
    # Create a graph
    graph = Graph(name="CustomAnalysis")
    
    print("Building custom dataflow graph...")
    print()
    
    # Create tasks and nodes
    print("1. Adding task nodes...")
    task1 = PrintTask("Task 1: Initial Processing", "Task1")
    node1 = graph.add_node(task1)
    
    task2 = PrintTask("Task 2: Parallel Branch A", "Task2")
    node2 = graph.add_node(task2)
    
    task3 = PrintTask("Task 3: Parallel Branch B", "Task3")
    node3 = graph.add_node(task3)
    
    task4 = PrintTask("Task 4: Merge Results", "Task4")
    node4 = graph.add_node(task4)
    
    # Connect nodes with edges
    print("2. Connecting nodes with edges...")
    graph.add_edge(node1, node2, "data_a")
    graph.add_edge(node1, node3, "data_b")
    graph.add_edge(node2, node4, "result_a")
    graph.add_edge(node3, node4, "result_b")
    
    print()
    print(f"Graph created with {len(graph.nodes)} nodes and {len(graph.edges)} edges")
    print()
    
    # Visualize the graph
    print("Dataflow Graph Structure:")
    print("-" * 80)
    print(graph.visualize())
    print("-" * 80)
    print()
    
    # Show execution order
    print("Execution Order (Topological Sort):")
    sorted_nodes = graph.topological_sort()
    for i, node in enumerate(sorted_nodes, 1):
        print(f"  {i}. {node.task.name}")
    print()
    
    # Show parallel execution groups
    print("Parallel Execution Groups:")
    groups = graph.get_independent_groups(sorted_nodes)
    for i, group in enumerate(groups, 1):
        print(f"  Group {i} (can run in parallel):")
        for node in group:
            print(f"    - {node.task.name}")
    print()
    
    # Execute the workflow
    print("Executing workflow...")
    print("=" * 80)
    
    # Provide initial input
    initial_inputs = {
        node1: {"initial_data": "Starting data"}
    }
    
    results = graph.execute(inputs=initial_inputs)
    
    print("=" * 80)
    print()
    print("Workflow execution completed!")
    print()
    print("Results:")
    for node, result in results.items():
        print(f"  {node.task.name}: {result}")
    print()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
