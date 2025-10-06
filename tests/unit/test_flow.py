"""
Unit tests for Flow classes (FlowData, FlowNode, FlowGraph)
"""
import pytest
from perflow.flow.flow import FlowData, FlowNode, FlowGraph
from perflow.perf_data_struct.dynamic.trace.trace import Trace


class TestFlowData:
    """Test FlowData class"""
    
    def test_flowdata_creation(self):
        """Test creating empty FlowData"""
        flow_data = FlowData()
        assert flow_data.size() == 0
        assert len(flow_data.get_data()) == 0
    
    def test_flowdata_add_data(self):
        """Test adding data to FlowData"""
        flow_data = FlowData()
        
        trace1 = Trace()
        trace2 = Trace()
        
        flow_data.add_data(trace1)
        assert flow_data.size() == 1
        
        flow_data.add_data(trace2)
        assert flow_data.size() == 2
    
    def test_flowdata_get_data(self):
        """Test getting data from FlowData"""
        flow_data = FlowData()
        
        data1 = "test_data_1"
        data2 = "test_data_2"
        
        flow_data.add_data(data1)
        flow_data.add_data(data2)
        
        data_set = flow_data.get_data()
        assert data1 in data_set
        assert data2 in data_set
    
    def test_flowdata_remove_data(self):
        """Test removing data from FlowData"""
        flow_data = FlowData()
        
        data1 = "test_data_1"
        data2 = "test_data_2"
        
        flow_data.add_data(data1)
        flow_data.add_data(data2)
        assert flow_data.size() == 2
        
        flow_data.remove_data(data1)
        assert flow_data.size() == 1
        assert data1 not in flow_data.get_data()
        assert data2 in flow_data.get_data()
    
    def test_flowdata_clear(self):
        """Test clearing FlowData"""
        flow_data = FlowData()
        
        flow_data.add_data("data1")
        flow_data.add_data("data2")
        flow_data.add_data("data3")
        assert flow_data.size() == 3
        
        flow_data.clear()
        assert flow_data.size() == 0
        assert len(flow_data.get_data()) == 0
    
    def test_flowdata_set_behavior(self):
        """Test that FlowData uses set behavior (no duplicates)"""
        flow_data = FlowData()
        
        data = "test_data"
        flow_data.add_data(data)
        flow_data.add_data(data)  # Add same data twice
        
        # Set should only contain one instance
        assert flow_data.size() == 1


class ConcreteFlowNode(FlowNode):
    """Concrete implementation of FlowNode for testing"""
    
    def __init__(self):
        super().__init__()
        self.run_called = False
    
    def run(self):
        self.run_called = True
        # Copy inputs to outputs
        for data in self.m_inputs.get_data():
            self.m_outputs.add_data(data)


class TestFlowNode:
    """Test FlowNode class"""
    
    def test_flownode_creation(self):
        """Test creating FlowNode"""
        node = ConcreteFlowNode()
        assert node.get_inputs() is not None
        assert node.get_outputs() is not None
        assert node.get_inputs().size() == 0
        assert node.get_outputs().size() == 0
    
    def test_flownode_get_inputs(self):
        """Test getting inputs from FlowNode"""
        node = ConcreteFlowNode()
        inputs = node.get_inputs()
        assert isinstance(inputs, FlowData)
    
    def test_flownode_get_outputs(self):
        """Test getting outputs from FlowNode"""
        node = ConcreteFlowNode()
        outputs = node.get_outputs()
        assert isinstance(outputs, FlowData)
    
    def test_flownode_set_inputs(self):
        """Test setting inputs for FlowNode"""
        node = ConcreteFlowNode()
        new_inputs = FlowData()
        new_inputs.add_data("test_input")
        
        node.set_inputs(new_inputs)
        assert node.get_inputs().size() == 1
        assert "test_input" in node.get_inputs().get_data()
    
    def test_flownode_set_outputs(self):
        """Test setting outputs for FlowNode"""
        node = ConcreteFlowNode()
        new_outputs = FlowData()
        new_outputs.add_data("test_output")
        
        node.set_outputs(new_outputs)
        assert node.get_outputs().size() == 1
        assert "test_output" in node.get_outputs().get_data()
    
    def test_flownode_run(self):
        """Test running FlowNode"""
        node = ConcreteFlowNode()
        node.get_inputs().add_data("input_data")
        
        node.run()
        
        assert node.run_called
        assert "input_data" in node.get_outputs().get_data()


class TestFlowGraph:
    """Test FlowGraph class"""
    
    def test_flowgraph_creation(self):
        """Test creating empty FlowGraph"""
        graph = FlowGraph()
        assert len(graph.get_nodes()) == 0
    
    def test_flowgraph_add_node(self):
        """Test adding nodes to FlowGraph"""
        graph = FlowGraph()
        node1 = ConcreteFlowNode()
        node2 = ConcreteFlowNode()
        
        graph.add_node(node1)
        assert len(graph.get_nodes()) == 1
        
        graph.add_node(node2)
        assert len(graph.get_nodes()) == 2
    
    def test_flowgraph_add_duplicate_node(self):
        """Test adding duplicate node doesn't duplicate"""
        graph = FlowGraph()
        node = ConcreteFlowNode()
        
        graph.add_node(node)
        graph.add_node(node)  # Add same node twice
        
        assert len(graph.get_nodes()) == 1
    
    def test_flowgraph_add_edge(self):
        """Test adding edges between nodes"""
        graph = FlowGraph()
        node1 = ConcreteFlowNode()
        node2 = ConcreteFlowNode()
        
        graph.add_edge(node1, node2)
        
        # Both nodes should be in the graph
        assert node1 in graph.get_nodes()
        assert node2 in graph.get_nodes()
        
        # Node2 should be a successor of node1
        successors = graph.get_successors(node1)
        assert node2 in successors
    
    def test_flowgraph_get_successors(self):
        """Test getting successor nodes"""
        graph = FlowGraph()
        node1 = ConcreteFlowNode()
        node2 = ConcreteFlowNode()
        node3 = ConcreteFlowNode()
        
        graph.add_edge(node1, node2)
        graph.add_edge(node1, node3)
        
        successors = graph.get_successors(node1)
        assert len(successors) == 2
        assert node2 in successors
        assert node3 in successors
    
    def test_flowgraph_run(self):
        """Test running FlowGraph"""
        graph = FlowGraph()
        node1 = ConcreteFlowNode()
        node2 = ConcreteFlowNode()
        
        # Set up pipeline: node1 -> node2
        graph.add_edge(node1, node2)
        
        # Add input data to node1
        node1.get_inputs().add_data("input_data")
        
        # Run the graph
        graph.run()
        
        # Both nodes should have run
        assert node1.run_called
        assert node2.run_called
        
        # Data should flow from node1 to node2
        assert "input_data" in node2.get_outputs().get_data()
    
    def test_flowgraph_clear(self):
        """Test clearing FlowGraph"""
        graph = FlowGraph()
        node1 = ConcreteFlowNode()
        node2 = ConcreteFlowNode()
        
        graph.add_edge(node1, node2)
        assert len(graph.get_nodes()) == 2
        
        graph.clear()
        assert len(graph.get_nodes()) == 0
    
    def test_flowgraph_complex_pipeline(self):
        """Test complex pipeline with multiple branches"""
        graph = FlowGraph()
        
        # Create nodes
        source = ConcreteFlowNode()
        processor1 = ConcreteFlowNode()
        processor2 = ConcreteFlowNode()
        sink = ConcreteFlowNode()
        
        # Build pipeline:
        #   source -> processor1 -> sink
        #          -> processor2 -> sink
        graph.add_edge(source, processor1)
        graph.add_edge(source, processor2)
        graph.add_edge(processor1, sink)
        graph.add_edge(processor2, sink)
        
        # Add input
        source.get_inputs().add_data("test_data")
        
        # Run
        graph.run()
        
        # All nodes should have run
        assert source.run_called
        assert processor1.run_called
        assert processor2.run_called
        assert sink.run_called
