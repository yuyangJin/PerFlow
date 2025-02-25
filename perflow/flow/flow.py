'''
module flow data/node/graph
'''

'''
@class FlowData
The data flowing through the edges between FlowNodes
'''
class FlowData:
    def __init__(self):
        # To be implemented
        pass

'''
@class FlowNode
The sub-task node
'''
class FlowNode:
    def __init__(self, name, id, inputs):
        self.m_name = name
        self.m_id = id
        self.m_inputs = inputs
        self.m_outputs = None
    
    def __str__(self):
        return f"FlowNode({self.m_name})"

    def set_inputs(self, inputs):
        self.m_inputs = inputs

    def set_outputs(self, outputs):
        self.m_outputs = outputs

    def get_inputs(self):
        return self.m_inputs

    def get_outputs(self):
        return self.m_outputs

    # @abstractmethod
    def run(self):
        print('FlowNode runs virtially.')
        pass

'''
@class FlowGraph
The entire workflow of analysis
'''
class FlowGraph:
    def __init__(self):
        # To be implemented
        pass