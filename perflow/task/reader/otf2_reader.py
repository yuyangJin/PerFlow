'''
module OTF2 reader
'''

from typing import Optional
from ...flow.flow import FlowNode
from ...perf_data_struct.dynamic.trace.trace import Trace

'''
@class OTF2Reader
Read the performance data from OTF2 format files
'''


class OTF2Reader(FlowNode):
    """
    OTF2Reader reads trace data from OTF2 format files.
    
    This class implements a FlowNode that reads trace files in the
    Open Trace Format 2 (OTF2) and converts them into Trace objects
    for analysis.
    
    Attributes:
        m_file_path: Path to the OTF2 trace file
        m_trace: The loaded trace object
    """
    
    def __init__(self, file_path: Optional[str] = None) -> None:
        """
        Initialize an OTF2Reader.
        
        Args:
            file_path: Optional path to the OTF2 trace file
        """
        super().__init__()
        self.m_file_path: Optional[str] = file_path
        self.m_trace: Optional[Trace] = None
    
    def setFilePath(self, file_path: str) -> None:
        """
        Set the path to the OTF2 trace file.
        
        Args:
            file_path: Path to the OTF2 trace file
        """
        self.m_file_path = file_path
    
    def getFilePath(self) -> Optional[str]:
        """
        Get the path to the OTF2 trace file.
        
        Returns:
            Path to the OTF2 trace file
        """
        return self.m_file_path
    
    def getTrace(self) -> Optional[Trace]:
        """
        Get the loaded trace.
        
        Returns:
            The loaded Trace object
        """
        return self.m_trace
    
    def load(self) -> Trace:
        """
        Load trace data from the OTF2 file.
        
        This method reads and parses the OTF2 file, creating a Trace object.
        In this initial implementation, it creates an empty trace as a placeholder.
        Full OTF2 parsing would require the OTF2 library integration.
        
        Returns:
            Loaded Trace object
        """
        # Placeholder implementation
        # Full implementation would use OTF2 library to parse the file
        self.m_trace = Trace()
        return self.m_trace
    
    def run(self) -> None:
        """
        Execute the OTF2 reading task.
        
        Loads the trace from the specified OTF2 file and adds it to
        the output flow data.
        """
        if self.m_file_path:
            trace = self.load()
            self.m_outputs.add_data(trace)