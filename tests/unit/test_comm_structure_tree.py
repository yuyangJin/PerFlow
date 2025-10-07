"""
Unit tests for Communication Structure Tree (CST).
"""

import unittest
from perflow.perf_data_struct.static import ProgramStructureGraph, NodeType
from perflow.perf_data_struct.static.comm_structure_tree import CommStructureTree, CommType


class TestCommStructureTree(unittest.TestCase):
    """Test cases for CommStructureTree class."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.cst = CommStructureTree()
        self.psg = ProgramStructureGraph()
    
    def test_cst_creation(self):
        """Test creating an empty CST."""
        self.assertIsNotNone(self.cst)
        self.assertEqual(len(self.cst.getMPINodes()), 0)
        self.assertEqual(self.cst.getPointToPointCount(), 0)
        self.assertEqual(self.cst.getCollectiveCount(), 0)
    
    def test_add_mpi_node(self):
        """Test adding an MPI node directly."""
        node = self.cst.addMPINode(
            node_id=1,
            name="MPI_Send",
            comm_type=CommType.POINT_TO_POINT,
            source=0,
            dest=1,
            tag=0
        )
        
        self.assertIsNotNone(node)
        self.assertEqual(node.getName(), "MPI_Send")
        self.assertEqual(node.getAttribute("is_mpi"), True)
        self.assertEqual(node.getAttribute("comm_type"), CommType.POINT_TO_POINT.value)
        self.assertEqual(node.getAttribute("source"), 0)
        self.assertEqual(node.getAttribute("dest"), 1)
        self.assertEqual(len(self.cst.getMPINodes()), 1)
    
    def test_add_collective_mpi_node(self):
        """Test adding a collective MPI node."""
        node = self.cst.addMPINode(
            node_id=2,
            name="MPI_Bcast",
            comm_type=CommType.BROADCAST,
            root=0,
            communicator=0
        )
        
        self.assertIsNotNone(node)
        self.assertEqual(node.getName(), "MPI_Bcast")
        self.assertEqual(node.getAttribute("comm_type"), CommType.BROADCAST.value)
        self.assertEqual(node.getAttribute("root"), 0)
        self.assertEqual(self.cst.getCollectiveCount(), 1)
    
    def test_build_from_psg_simple(self):
        """Test building CST from a simple PSG."""
        # Create a simple PSG with function -> loop -> MPI call
        func = self.psg.addFunctionNode(1, "main", "main.c", 10)
        loop = self.psg.addLoopNode(2, "for_loop", parent_id=1, loop_type="for")
        call = self.psg.addCallSite(3, caller_id=2, callee_name="MPI_Send")
        
        # Build CST
        mpi_nodes = {3}
        comm_types = {3: CommType.POINT_TO_POINT}
        self.cst.buildFromPSG(self.psg, mpi_nodes, comm_types)
        
        # Verify all nodes are included
        self.assertEqual(len(self.cst.getNodes()), 3)
        self.assertIsNotNone(self.cst.getNode(1))
        self.assertIsNotNone(self.cst.getNode(2))
        self.assertIsNotNone(self.cst.getNode(3))
        
        # Verify MPI node is marked
        mpi_node = self.cst.getNode(3)
        self.assertEqual(mpi_node.getAttribute("is_mpi"), True)
        self.assertEqual(len(self.cst.getMPINodes()), 1)
    
    def test_build_from_psg_with_pruning(self):
        """Test that CST prunes nodes without MPI descendants."""
        # Create PSG with two branches: one with MPI, one without
        func = self.psg.addFunctionNode(1, "main", "main.c", 10)
        loop1 = self.psg.addLoopNode(2, "loop_with_mpi", parent_id=1, loop_type="for")
        loop2 = self.psg.addLoopNode(3, "loop_without_mpi", parent_id=1, loop_type="for")
        mpi_call = self.psg.addCallSite(4, caller_id=2, callee_name="MPI_Send")
        non_mpi_call = self.psg.addCallSite(5, caller_id=3, callee_name="compute")
        
        # Build CST - only node 4 is MPI
        mpi_nodes = {4}
        comm_types = {4: CommType.POINT_TO_POINT}
        self.cst.buildFromPSG(self.psg, mpi_nodes, comm_types)
        
        # Verify only MPI branch is included
        self.assertIsNotNone(self.cst.getNode(1))  # main function
        self.assertIsNotNone(self.cst.getNode(2))  # loop with MPI
        self.assertIsNone(self.cst.getNode(3))     # loop without MPI should be pruned
        self.assertIsNotNone(self.cst.getNode(4))  # MPI call
        self.assertIsNone(self.cst.getNode(5))     # non-MPI call should be pruned
    
    def test_build_from_psg_nested_structure(self):
        """Test building CST with nested loops and multiple MPI calls."""
        # Create nested structure: func -> loop1 -> loop2 -> MPI1, MPI2
        func = self.psg.addFunctionNode(1, "main", "main.c", 10)
        loop1 = self.psg.addLoopNode(2, "outer_loop", parent_id=1, loop_type="for")
        loop2 = self.psg.addLoopNode(3, "inner_loop", parent_id=2, loop_type="for")
        mpi1 = self.psg.addCallSite(4, caller_id=3, callee_name="MPI_Send")
        mpi2 = self.psg.addCallSite(5, caller_id=3, callee_name="MPI_Recv")
        
        # Build CST
        mpi_nodes = {4, 5}
        comm_types = {
            4: CommType.POINT_TO_POINT,
            5: CommType.POINT_TO_POINT
        }
        self.cst.buildFromPSG(self.psg, mpi_nodes, comm_types)
        
        # Verify all nodes are included
        self.assertEqual(len(self.cst.getNodes()), 5)
        self.assertEqual(len(self.cst.getMPINodes()), 2)
        
        # Verify structure is preserved
        self.assertIsNotNone(self.cst.getNode(1))
        self.assertIsNotNone(self.cst.getNode(2))
        self.assertIsNotNone(self.cst.getNode(3))
        self.assertIsNotNone(self.cst.getNode(4))
        self.assertIsNotNone(self.cst.getNode(5))
    
    def test_get_mpi_nodes_by_type(self):
        """Test filtering MPI nodes by communication type."""
        # Add different types of MPI nodes
        self.cst.addMPINode(1, "MPI_Send", CommType.POINT_TO_POINT, source=0, dest=1)
        self.cst.addMPINode(2, "MPI_Recv", CommType.POINT_TO_POINT, source=1, dest=0)
        self.cst.addMPINode(3, "MPI_Bcast", CommType.BROADCAST, root=0)
        self.cst.addMPINode(4, "MPI_Reduce", CommType.REDUCE, root=0)
        
        # Test filtering
        p2p_nodes = self.cst.getMPINodesByType(CommType.POINT_TO_POINT)
        self.assertEqual(len(p2p_nodes), 2)
        
        bcast_nodes = self.cst.getMPINodesByType(CommType.BROADCAST)
        self.assertEqual(len(bcast_nodes), 1)
        self.assertEqual(bcast_nodes[0].getName(), "MPI_Bcast")
        
        reduce_nodes = self.cst.getMPINodesByType(CommType.REDUCE)
        self.assertEqual(len(reduce_nodes), 1)
    
    def test_get_communication_pattern(self):
        """Test extracting communication patterns."""
        # Add point-to-point communications
        self.cst.addMPINode(1, "MPI_Send", CommType.POINT_TO_POINT, source=0, dest=1)
        self.cst.addMPINode(2, "MPI_Send", CommType.POINT_TO_POINT, source=1, dest=2)
        
        # Add collective
        self.cst.addMPINode(3, "MPI_Bcast", CommType.BROADCAST, root=0)
        
        patterns = self.cst.getCommunicationPattern()
        
        # Verify patterns
        self.assertIn("p2p", patterns)
        self.assertIn("collective", patterns)
        self.assertEqual(len(patterns["p2p"]), 2)
        self.assertIn((0, 1), patterns["p2p"])
        self.assertIn((1, 2), patterns["p2p"])
        self.assertEqual(len(patterns["collective"]), 1)
    
    def test_get_max_nesting_depth(self):
        """Test calculating maximum nesting depth."""
        # Create a deep nested structure
        func = self.psg.addFunctionNode(1, "main", "main.c", 10)
        loop1 = self.psg.addLoopNode(2, "loop1", parent_id=1, loop_type="for")
        loop2 = self.psg.addLoopNode(3, "loop2", parent_id=2, loop_type="for")
        loop3 = self.psg.addLoopNode(4, "loop3", parent_id=3, loop_type="for")
        mpi = self.psg.addCallSite(5, caller_id=4, callee_name="MPI_Send")
        
        # Build CST
        mpi_nodes = {5}
        comm_types = {5: CommType.POINT_TO_POINT}
        self.cst.buildFromPSG(self.psg, mpi_nodes, comm_types)
        
        # Depth should be 4 (main -> loop1 -> loop2 -> loop3 -> mpi)
        depth = self.cst.getMaxNestingDepth()
        self.assertEqual(depth, 4)
    
    def test_point_to_point_and_collective_counts(self):
        """Test counting point-to-point and collective operations."""
        self.cst.addMPINode(1, "MPI_Send", CommType.POINT_TO_POINT, source=0, dest=1)
        self.cst.addMPINode(2, "MPI_Recv", CommType.POINT_TO_POINT, source=1, dest=0)
        self.cst.addMPINode(3, "MPI_Bcast", CommType.BROADCAST, root=0)
        self.cst.addMPINode(4, "MPI_Reduce", CommType.REDUCE, root=0)
        self.cst.addMPINode(5, "MPI_Allreduce", CommType.ALLREDUCE)
        
        self.assertEqual(self.cst.getPointToPointCount(), 2)
        self.assertEqual(self.cst.getCollectiveCount(), 3)
    
    def test_empty_cst_operations(self):
        """Test operations on empty CST."""
        self.assertEqual(len(self.cst.getMPINodes()), 0)
        self.assertEqual(self.cst.getPointToPointCount(), 0)
        self.assertEqual(self.cst.getCollectiveCount(), 0)
        self.assertEqual(self.cst.getMaxNestingDepth(), 0)
        
        patterns = self.cst.getCommunicationPattern()
        self.assertEqual(len(patterns["p2p"]), 0)
        self.assertEqual(len(patterns["collective"]), 0)


if __name__ == '__main__':
    unittest.main()
