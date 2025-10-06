'''
module nsight reader
'''

from typing import Optional
from ...flow.flow import FlowNode
from ...perf_data_struct.dynamic.trace.trace import Trace

'''
@class NsightReader
Read the performance data from NVIDIA Nsight Systems collected files
'''


class NsightReader(FlowNode):
    """
    NsightReader reads trace data from NVIDIA Nsight Systems files.
    
    This class implements a FlowNode that reads trace files from NVIDIA Nsight
    Systems (nsys) and converts them into Trace objects for analysis.
    
    Nsight Systems is a system-wide performance analysis tool that provides
    detailed profiling for CUDA applications, including GPU/CPU activity,
    kernel launches, memory transfers, and API calls.
    
    Attributes:
        m_file_path: Path to the Nsight Systems result file (.nsys-rep or .qdrep)
        m_trace: The loaded trace object
    """
    
    def __init__(self, file_path: Optional[str] = None) -> None:
        """
        Initialize a NsightReader.
        
        Args:
            file_path: Optional path to the Nsight Systems result file
        """
        super().__init__()
        self.m_file_path: Optional[str] = file_path
        self.m_trace: Optional[Trace] = None
    
    def setFilePath(self, file_path: str) -> None:
        """
        Set the path to the Nsight Systems result file.
        
        Args:
            file_path: Path to the Nsight Systems result file
        """
        self.m_file_path = file_path
    
    def getFilePath(self) -> Optional[str]:
        """
        Get the path to the Nsight Systems result file.
        
        Returns:
            Path to the Nsight Systems result file
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
        Load trace data from the Nsight Systems result file.
        
        This method reads and parses Nsight Systems results, creating a Trace object.
        In this initial implementation, it creates an empty trace as a placeholder.
        Full Nsight parsing would require the nsys CLI export tools or SQLite parsing.
        
        Nsight Systems stores results in SQLite database format (.nsys-rep) which
        can be exported to various formats or queried directly.
        
        Returns:
            Loaded Trace object
        """
        # Placeholder implementation
        # Full implementation would:
        # 1. Use nsys export to convert to JSON/CSV
        # 2. Or directly query the SQLite database
        # 3. Parse CUDA events, CPU threads, memory operations
        self.m_trace = Trace()
        return self.m_trace
    
    def run(self) -> None:
        """
        Execute the Nsight reading task.
        
        Loads the trace from the specified Nsight Systems file and adds it
        to the output flow data.
        """
        if self.m_file_path:
            trace = self.load()
            self.m_outputs.add_data(trace)