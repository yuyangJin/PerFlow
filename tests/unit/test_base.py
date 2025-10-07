"""
Unit tests for base classes (Node, Graph, Tree)

This module contains tests for the fundamental base classes used
throughout the performance data structures.
"""

import pytest
from perflow.perf_data_struct.base import Node, Graph, Tree


class TestNode:
    """Test Node class."""
    
    def test_node_creation_default(self):
        """Test creating a node with default values."""
        node = Node()
        assert node.getId() is None
        assert node.getName() is None
        assert node.getType() is None
    
    def test_node_creation_with_parameters(self):
        """Test creating a node with all parameters."""
        node = Node(node_id=1, name="test_node", node_type="function")
        assert node.getId() == 1
        assert node.getName() == "test_node"
        assert node.getType() == "function"
    
    def test_node_setters(self):
        """Test node setters."""
        node = Node()
        node.setId(10)
        node.setName("my_function")
        node.setType("loop")
        
        assert node.getId() == 10
        assert node.getName() == "my_function"
        assert node.getType() == "loop"
    
    def test_node_attributes(self):
        """Test node attributes."""
        node = Node(1, "test")
        node.setAttribute("source_file", "test.c")
        node.setAttribute("line_number", 42)
        
        assert node.getAttribute("source_file") == "test.c"
        assert node.getAttribute("line_number") == 42
        assert node.getAttribute("nonexistent") is None
    
    def test_node_get_all_attributes(self):
        """Test getting all attributes."""
        node = Node()
        node.setAttribute("key1", "value1")
        node.setAttribute("key2", 123)
        
        attrs = node.getAttributes()
        assert len(attrs) == 2
        assert attrs["key1"] == "value1"
        assert attrs["key2"] == 123


class TestGraph:
    """Test Graph class."""
    
    def test_graph_creation(self):
        """Test creating an empty graph."""
        graph = Graph()
        assert graph.getNodeCount() == 0
        assert graph.getEdgeCount() == 0
    
    def test_add_node(self):
        """Test adding nodes to graph."""
        graph = Graph()
        node1 = Node(1, "node1")
        node2 = Node(2, "node2")
        
        graph.addNode(node1)
        graph.addNode(node2)
        
        assert graph.getNodeCount() == 2
    
    def test_get_node(self):
        """Test retrieving nodes."""
        graph = Graph()
        node = Node(5, "test_node")
        graph.addNode(node)
        
        retrieved = graph.getNode(5)
        assert retrieved is not None
        assert retrieved.getName() == "test_node"
    
    def test_get_nonexistent_node(self):
        """Test getting a node that doesn't exist."""
        graph = Graph()
        assert graph.getNode(999) is None
    
    def test_add_edge(self):
        """Test adding edges between nodes."""
        graph = Graph()
        node1 = Node(1, "node1")
        node2 = Node(2, "node2")
        graph.addNode(node1)
        graph.addNode(node2)
        
        graph.addEdge(1, 2)
        
        successors = graph.getSuccessors(1)
        assert 2 in successors
        assert graph.getEdgeCount() == 1
    
    def test_multiple_edges(self):
        """Test adding multiple edges."""
        graph = Graph()
        for i in range(5):
            graph.addNode(Node(i, f"node{i}"))
        
        graph.addEdge(0, 1)
        graph.addEdge(0, 2)
        graph.addEdge(1, 3)
        graph.addEdge(2, 4)
        
        assert len(graph.getSuccessors(0)) == 2
        assert graph.getEdgeCount() == 4
    
    def test_get_all_nodes(self):
        """Test getting all nodes."""
        graph = Graph()
        nodes = [Node(i, f"node{i}") for i in range(3)]
        for node in nodes:
            graph.addNode(node)
        
        all_nodes = graph.getNodes()
        assert len(all_nodes) == 3
    
    def test_clear(self):
        """Test clearing the graph."""
        graph = Graph()
        graph.addNode(Node(1, "node1"))
        graph.addNode(Node(2, "node2"))
        graph.addEdge(1, 2)
        
        graph.clear()
        
        assert graph.getNodeCount() == 0
        assert graph.getEdgeCount() == 0


class TestTree:
    """Test Tree class."""
    
    def test_tree_creation(self):
        """Test creating an empty tree."""
        tree = Tree()
        assert tree.getNodeCount() == 0
        assert tree.getRoot() is None
    
    def test_set_root(self):
        """Test setting the root node."""
        tree = Tree()
        root = Node(0, "root")
        tree.setRoot(root)
        
        assert tree.getRoot() is not None
        assert tree.getRoot().getName() == "root"
        assert tree.getNodeCount() == 1
    
    def test_add_child(self):
        """Test adding child nodes."""
        tree = Tree()
        root = Node(0, "root")
        tree.setRoot(root)
        
        child1 = Node(1, "child1")
        child2 = Node(2, "child2")
        
        tree.addChild(0, child1)
        tree.addChild(0, child2)
        
        assert tree.getNodeCount() == 3
        children = tree.getChildren(0)
        assert len(children) == 2
        assert 1 in children
        assert 2 in children
    
    def test_get_parent(self):
        """Test getting parent of a node."""
        tree = Tree()
        root = Node(0, "root")
        tree.setRoot(root)
        
        child = Node(1, "child")
        tree.addChild(0, child)
        
        parent_id = tree.getParent(1)
        assert parent_id == 0
        assert tree.getParent(0) is None  # Root has no parent
    
    def test_get_depth(self):
        """Test calculating node depth."""
        tree = Tree()
        root = Node(0, "root")
        tree.setRoot(root)
        
        child1 = Node(1, "child1")
        tree.addChild(0, child1)
        
        grandchild = Node(2, "grandchild")
        tree.addChild(1, grandchild)
        
        assert tree.getDepth(0) == 0  # Root
        assert tree.getDepth(1) == 1  # Child
        assert tree.getDepth(2) == 2  # Grandchild
    
    def test_hierarchical_structure(self):
        """Test building a hierarchical tree structure."""
        tree = Tree()
        
        # Build tree: root -> child1, child2
        #                    child1 -> grandchild1
        root = Node(0, "root")
        tree.setRoot(root)
        
        child1 = Node(1, "child1")
        child2 = Node(2, "child2")
        tree.addChild(0, child1)
        tree.addChild(0, child2)
        
        grandchild1 = Node(3, "grandchild1")
        tree.addChild(1, grandchild1)
        
        assert tree.getNodeCount() == 4
        assert len(tree.getChildren(0)) == 2
        assert len(tree.getChildren(1)) == 1
        assert len(tree.getChildren(2)) == 0
        assert tree.getDepth(3) == 2
