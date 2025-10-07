'''
@module Communication Structure Tree
'''

from ..base import Tree, Node
from .program_structure_graph import NodeType
from typing import List, Optional, Dict
from enum import Enum

'''
@enum CommType
'''


class CommType(Enum):
    """Enumeration of communication types."""
    POINT_TO_POINT = "p2p"
    COLLECTIVE = "collective"
    ONE_SIDED = "one_sided"
    BROADCAST = "broadcast"
    REDUCE = "reduce"
    SCATTER = "scatter"
    GATHER = "gather"
    ALLTOALL = "alltoall"


'''
@class CommStructureTree
Represent the static communication structure of a program
'''


class CommStructureTree(Tree):
    """
    CommStructureTree represents the hierarchical communication structure of a parallel program.
    
    This tree captures the communication patterns and their relationships in a parallel
    application, including point-to-point communications, collectives, and their nesting.
    
    Attributes:
        m_comm_nodes: Dictionary mapping communication operation IDs to nodes
        m_comm_type_map: Dictionary mapping node IDs to communication types
    """
    
    def __init__(self) -> None:
        """Initialize a CommStructureTree."""
        super().__init__()
        self.m_comm_nodes: Dict[int, Node] = {}
        self.m_comm_type_map: Dict[int, CommType] = {}
    
    def addCommunicationNode(self, node_id: int, name: str, comm_type: CommType,
                            parent_id: Optional[int] = None,
                            source: Optional[int] = None,
                            dest: Optional[int] = None,
                            tag: Optional[int] = None,
                            communicator: Optional[int] = None) -> Node:
        """
        Add a communication node to the tree.
        
        Args:
            node_id: Unique identifier for the communication node
            name: Name of the communication operation (e.g., "MPI_Send")
            comm_type: Type of communication
            parent_id: Parent node ID (if nested within another communication)
            source: Source process rank (for point-to-point)
            dest: Destination process rank (for point-to-point)
            tag: Message tag (for point-to-point)
            communicator: Communicator ID
            
        Returns:
            Created communication node
        """
        node = Node(node_id, name, "communication")
        node.setAttribute("comm_type", comm_type.value)
        
        if source is not None:
            node.setAttribute("source", source)
        if dest is not None:
            node.setAttribute("dest", dest)
        if tag is not None:
            node.setAttribute("tag", tag)
        if communicator is not None:
            node.setAttribute("communicator", communicator)
        
        self.addNode(node)
        self.m_comm_nodes[node_id] = node
        self.m_comm_type_map[node_id] = comm_type
        
        if parent_id is not None:
            self.addChild(parent_id, node)
        elif self.m_root is None:
            self.setRoot(node)
        
        return node
    
    def addPointToPointComm(self, node_id: int, name: str, source: int, dest: int,
                           tag: int = 0, communicator: int = 0,
                           parent_id: Optional[int] = None) -> Node:
        """
        Add a point-to-point communication node.
        
        Args:
            node_id: Unique identifier
            name: Communication operation name
            source: Source process rank
            dest: Destination process rank
            tag: Message tag
            communicator: Communicator ID
            parent_id: Parent node ID
            
        Returns:
            Created communication node
        """
        return self.addCommunicationNode(
            node_id, name, CommType.POINT_TO_POINT,
            parent_id=parent_id, source=source, dest=dest,
            tag=tag, communicator=communicator
        )
    
    def addCollectiveComm(self, node_id: int, name: str, collective_type: CommType,
                         root: Optional[int] = None, communicator: int = 0,
                         parent_id: Optional[int] = None) -> Node:
        """
        Add a collective communication node.
        
        Args:
            node_id: Unique identifier
            name: Communication operation name
            collective_type: Type of collective (BROADCAST, REDUCE, etc.)
            root: Root process rank (for rooted collectives)
            communicator: Communicator ID
            parent_id: Parent node ID
            
        Returns:
            Created communication node
        """
        node = self.addCommunicationNode(
            node_id, name, collective_type,
            parent_id=parent_id, communicator=communicator
        )
        
        if root is not None:
            node.setAttribute("root", root)
        
        return node
    
    def getCommunicationNodes(self) -> List[Node]:
        """
        Get all communication nodes.
        
        Returns:
            List of communication nodes
        """
        return list(self.m_comm_nodes.values())
    
    def getCommNodesByType(self, comm_type: CommType) -> List[Node]:
        """
        Get communication nodes of a specific type.
        
        Args:
            comm_type: Type of communication to filter by
            
        Returns:
            List of communication nodes of the specified type
        """
        return [
            node for node_id, node in self.m_comm_nodes.items()
            if self.m_comm_type_map.get(node_id) == comm_type
        ]
    
    def getPointToPointCount(self) -> int:
        """Get the number of point-to-point communications."""
        return len(self.getCommNodesByType(CommType.POINT_TO_POINT))
    
    def getCollectiveCount(self) -> int:
        """Get the total number of collective communications."""
        count = 0
        for comm_type in CommType:
            if comm_type != CommType.POINT_TO_POINT and comm_type != CommType.ONE_SIDED:
                count += len(self.getCommNodesByType(comm_type))
        return count
    
    def getCommunicationPattern(self) -> Dict[str, List[tuple]]:
        """
        Extract communication patterns from the tree.
        
        Returns:
            Dictionary mapping pattern types to lists of (source, dest) tuples
        """
        patterns = {
            "p2p": [],
            "collective": []
        }
        
        for node in self.m_comm_nodes.values():
            comm_type = node.getAttribute("comm_type")
            if comm_type == CommType.POINT_TO_POINT.value:
                source = node.getAttribute("source")
                dest = node.getAttribute("dest")
                if source is not None and dest is not None:
                    patterns["p2p"].append((source, dest))
            else:
                patterns["collective"].append((node.getName(), comm_type))
        
        return patterns
    
    def getMaxNestingDepth(self) -> int:
        """
        Get the maximum nesting depth of communications in the tree.
        
        Returns:
            Maximum nesting depth
        """
        if not self.m_comm_nodes:
            return 0
        
        max_depth = 0
        for node_id in self.m_comm_nodes.keys():
            depth = self.getDepth(node_id)
            max_depth = max(max_depth, depth)
        
        return max_depth + 1