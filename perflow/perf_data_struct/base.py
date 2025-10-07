'''
@module base
basic structure for performance data
the classes in this file can be derived from Graph class in existing package
'''

from typing import List, Optional, Dict, Any

'''
@class Node
A Node is a program structure or a code segment
'''


class Node:
    """
    Node represents a program structure or code segment.
    
    A node can represent various program elements like functions, loops,
    basic blocks, or communication operations in the program structure.
    
    Attributes:
        m_id: Unique identifier for the node
        m_name: Name of the node (e.g., function name)
        m_type: Type of the node (e.g., "function", "loop", "basic_block")
        m_attributes: Dictionary for additional attributes
    """
    
    def __init__(self, node_id: Optional[int] = None, name: Optional[str] = None, 
                 node_type: Optional[str] = None) -> None:
        """
        Initialize a Node.
        
        Args:
            node_id: Unique identifier for the node
            name: Name of the node
            node_type: Type of the node
        """
        self.m_id: Optional[int] = node_id
        self.m_name: Optional[str] = name
        self.m_type: Optional[str] = node_type
        self.m_attributes: Dict[str, Any] = {}
    
    def getId(self) -> Optional[int]:
        """Get the node ID."""
        return self.m_id
    
    def setId(self, node_id: int) -> None:
        """Set the node ID."""
        self.m_id = node_id
    
    def getName(self) -> Optional[str]:
        """Get the node name."""
        return self.m_name
    
    def setName(self, name: str) -> None:
        """Set the node name."""
        self.m_name = name
    
    def getType(self) -> Optional[str]:
        """Get the node type."""
        return self.m_type
    
    def setType(self, node_type: str) -> None:
        """Set the node type."""
        self.m_type = node_type
    
    def getAttribute(self, key: str) -> Any:
        """Get an attribute value."""
        return self.m_attributes.get(key)
    
    def setAttribute(self, key: str, value: Any) -> None:
        """Set an attribute value."""
        self.m_attributes[key] = value
    
    def getAttributes(self) -> Dict[str, Any]:
        """Get all attributes."""
        return self.m_attributes


'''
@class Graph
A Graph is the structure/abstraction of a program
'''


class Graph:
    """
    Graph represents a program structure or abstraction.
    
    A graph consists of nodes and edges representing relationships
    between program elements.
    
    Attributes:
        m_nodes: List of nodes in the graph
        m_edges: Dictionary mapping node IDs to lists of successor node IDs
        m_node_map: Dictionary mapping node IDs to node objects
    """
    
    def __init__(self) -> None:
        """Initialize a Graph."""
        self.m_nodes: List[Node] = []
        self.m_edges: Dict[int, List[int]] = {}
        self.m_node_map: Dict[int, Node] = {}
    
    def addNode(self, node: Node) -> None:
        """
        Add a node to the graph.
        
        Args:
            node: Node to add
        """
        if node.getId() is not None:
            self.m_nodes.append(node)
            self.m_node_map[node.getId()] = node
            if node.getId() not in self.m_edges:
                self.m_edges[node.getId()] = []
    
    def getNode(self, node_id: int) -> Optional[Node]:
        """
        Get a node by ID.
        
        Args:
            node_id: ID of the node to retrieve
            
        Returns:
            Node object or None if not found
        """
        return self.m_node_map.get(node_id)
    
    def getNodes(self) -> List[Node]:
        """Get all nodes in the graph."""
        return self.m_nodes
    
    def addEdge(self, from_id: int, to_id: int) -> None:
        """
        Add an edge between two nodes.
        
        Args:
            from_id: Source node ID
            to_id: Destination node ID
        """
        if from_id not in self.m_edges:
            self.m_edges[from_id] = []
        if to_id not in self.m_edges[from_id]:
            self.m_edges[from_id].append(to_id)
    
    def getSuccessors(self, node_id: int) -> List[int]:
        """
        Get successor nodes of a given node.
        
        Args:
            node_id: ID of the node
            
        Returns:
            List of successor node IDs
        """
        return self.m_edges.get(node_id, [])
    
    def getNodeCount(self) -> int:
        """Get the number of nodes in the graph."""
        return len(self.m_nodes)
    
    def getEdgeCount(self) -> int:
        """Get the total number of edges in the graph."""
        return sum(len(successors) for successors in self.m_edges.values())
    
    def clear(self) -> None:
        """Clear all nodes and edges from the graph."""
        self.m_nodes.clear()
        self.m_edges.clear()
        self.m_node_map.clear()


'''
@class Tree
A Tree is a specific Graph with tree structure
'''


class Tree(Graph):
    """
    Tree represents a hierarchical program structure.
    
    A tree is a special case of a graph where there's a single root
    and each node (except root) has exactly one parent.
    
    Attributes:
        m_root: Root node of the tree
        m_parent_map: Dictionary mapping node IDs to parent node IDs
    """
    
    def __init__(self) -> None:
        """Initialize a Tree."""
        super().__init__()
        self.m_root: Optional[Node] = None
        self.m_parent_map: Dict[int, int] = {}
    
    def setRoot(self, node: Node) -> None:
        """
        Set the root node of the tree.
        
        Args:
            node: Root node
        """
        self.m_root = node
        if node.getId() is not None and node not in self.m_nodes:
            self.addNode(node)
    
    def getRoot(self) -> Optional[Node]:
        """Get the root node of the tree."""
        return self.m_root
    
    def addChild(self, parent_id: int, child: Node) -> None:
        """
        Add a child node to a parent node.
        
        Args:
            parent_id: ID of the parent node
            child: Child node to add
        """
        if child.getId() is not None:
            self.addNode(child)
            self.addEdge(parent_id, child.getId())
            self.m_parent_map[child.getId()] = parent_id
    
    def getParent(self, node_id: int) -> Optional[int]:
        """
        Get the parent of a node.
        
        Args:
            node_id: ID of the node
            
        Returns:
            Parent node ID or None if no parent
        """
        return self.m_parent_map.get(node_id)
    
    def getChildren(self, node_id: int) -> List[int]:
        """
        Get children of a node.
        
        Args:
            node_id: ID of the node
            
        Returns:
            List of child node IDs
        """
        return self.getSuccessors(node_id)
    
    def getDepth(self, node_id: int) -> int:
        """
        Get the depth of a node in the tree.
        
        Args:
            node_id: ID of the node
            
        Returns:
            Depth of the node (root has depth 0)
        """
        depth = 0
        current_id = node_id
        while current_id in self.m_parent_map:
            depth += 1
            current_id = self.m_parent_map[current_id]
        return depth