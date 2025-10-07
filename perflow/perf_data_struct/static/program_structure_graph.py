'''
@module Program Structure Graph
'''

from ..base import Graph, Node
from enum import Enum
from typing import List, Optional, Set

'''
@enum NodeType
'''


class NodeType(Enum):
    """Enumeration of program structure node types."""
    FUNCTION = "function"
    LOOP = "loop"
    BASIC_BLOCK = "basic_block"
    CALL_SITE = "call_site"
    BRANCH = "branch"
    STATEMENT = "statement"


'''
@class ProgramStructureGraph
Represent the static program structure graph
'''


class ProgramStructureGraph(Graph):
    """
    ProgramStructureGraph represents the static control flow structure of a program.
    
    This graph captures the hierarchical structure of a program including functions,
    loops, basic blocks, and their relationships. It's used for static analysis
    and understanding program structure.
    
    Attributes:
        m_entry_points: List of entry point node IDs (e.g., main function)
        m_function_nodes: Set of function node IDs
        m_loop_nodes: Set of loop node IDs
    """
    
    def __init__(self) -> None:
        """Initialize a ProgramStructureGraph."""
        super().__init__()
        self.m_entry_points: List[int] = []
        self.m_function_nodes: Set[int] = set()
        self.m_loop_nodes: Set[int] = set()
    
    def addFunctionNode(self, node_id: int, name: str, source_file: Optional[str] = None, 
                        line_number: Optional[int] = None) -> Node:
        """
        Add a function node to the graph.
        
        Args:
            node_id: Unique identifier for the function
            name: Function name
            source_file: Source file containing the function
            line_number: Starting line number of the function
            
        Returns:
            Created function node
        """
        node = Node(node_id, name, NodeType.FUNCTION.value)
        if source_file:
            node.setAttribute("source_file", source_file)
        if line_number is not None:
            node.setAttribute("line_number", line_number)
        
        self.addNode(node)
        self.m_function_nodes.add(node_id)
        return node
    
    def addLoopNode(self, node_id: int, name: str, parent_id: int, 
                    loop_type: Optional[str] = None) -> Node:
        """
        Add a loop node to the graph.
        
        Args:
            node_id: Unique identifier for the loop
            name: Loop name or identifier
            parent_id: Parent node ID (usually a function or another loop)
            loop_type: Type of loop (e.g., "for", "while", "do-while")
            
        Returns:
            Created loop node
        """
        node = Node(node_id, name, NodeType.LOOP.value)
        if loop_type:
            node.setAttribute("loop_type", loop_type)
        
        self.addNode(node)
        self.addEdge(parent_id, node_id)
        self.m_loop_nodes.add(node_id)
        return node
    
    def addBasicBlock(self, node_id: int, name: str, parent_id: int,
                      instructions: Optional[List[str]] = None) -> Node:
        """
        Add a basic block node to the graph.
        
        Args:
            node_id: Unique identifier for the basic block
            name: Basic block name or identifier
            parent_id: Parent node ID (usually a function or loop)
            instructions: List of instructions in the basic block
            
        Returns:
            Created basic block node
        """
        node = Node(node_id, name, NodeType.BASIC_BLOCK.value)
        if instructions:
            node.setAttribute("instructions", instructions)
            node.setAttribute("instruction_count", len(instructions))
        
        self.addNode(node)
        self.addEdge(parent_id, node_id)
        return node
    
    def addCallSite(self, node_id: int, caller_id: int, callee_name: str,
                    call_type: Optional[str] = None) -> Node:
        """
        Add a call site node to the graph.
        
        Args:
            node_id: Unique identifier for the call site
            caller_id: ID of the calling function/block
            callee_name: Name of the called function
            call_type: Type of call (e.g., "direct", "indirect", "virtual")
            
        Returns:
            Created call site node
        """
        node = Node(node_id, callee_name, NodeType.CALL_SITE.value)
        node.setAttribute("caller_id", caller_id)
        if call_type:
            node.setAttribute("call_type", call_type)
        
        self.addNode(node)
        self.addEdge(caller_id, node_id)
        return node
    
    def setEntryPoint(self, node_id: int) -> None:
        """
        Set a node as an entry point of the program.
        
        Args:
            node_id: ID of the entry point node
        """
        if node_id not in self.m_entry_points:
            self.m_entry_points.append(node_id)
    
    def getEntryPoints(self) -> List[int]:
        """Get list of entry point node IDs."""
        return self.m_entry_points
    
    def getFunctions(self) -> List[Node]:
        """
        Get all function nodes in the graph.
        
        Returns:
            List of function nodes
        """
        return [self.getNode(node_id) for node_id in self.m_function_nodes 
                if self.getNode(node_id) is not None]
    
    def getLoops(self) -> List[Node]:
        """
        Get all loop nodes in the graph.
        
        Returns:
            List of loop nodes
        """
        return [self.getNode(node_id) for node_id in self.m_loop_nodes 
                if self.getNode(node_id) is not None]
    
    def getFunctionCount(self) -> int:
        """Get the number of functions in the graph."""
        return len(self.m_function_nodes)
    
    def getLoopCount(self) -> int:
        """Get the number of loops in the graph."""
        return len(self.m_loop_nodes)
    
    def getCallGraph(self) -> 'Graph':
        """
        Extract the call graph from the program structure.
        
        Returns:
            Graph representing function call relationships
        """
        call_graph = Graph()
        
        # Add function nodes
        for func in self.getFunctions():
            call_graph.addNode(func)
        
        # Add edges for call sites
        for node in self.m_nodes:
            if node.getType() == NodeType.CALL_SITE.value:
                caller_id = node.getAttribute("caller_id")
                if caller_id is not None:
                    # Find the callee function by name
                    callee_name = node.getName()
                    for func in self.getFunctions():
                        if func.getName() == callee_name:
                            call_graph.addEdge(caller_id, func.getId())
                            break
        
        return call_graph
    
    def getLoopNestingDepth(self, loop_id: int) -> int:
        """
        Calculate the nesting depth of a loop.
        
        Args:
            loop_id: ID of the loop node
            
        Returns:
            Nesting depth (1 for outermost loops)
        """
        depth = 1
        # Traverse up to find parent loops
        for parent_id, children in self.m_edges.items():
            if loop_id in children:
                parent_node = self.getNode(parent_id)
                if parent_node and parent_node.getType() == NodeType.LOOP.value:
                    depth += self.getLoopNestingDepth(parent_id)
                    break
        return depth