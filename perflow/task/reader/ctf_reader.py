'''
module CTF reader
'''

from typing import Optional
from ...flow.flow import FlowNode
from ...perf_data_struct.dynamic.trace.trace import Trace

'''
@class CTFReader
Read the performance data from Common Trace Format (CTF) files
'''


class CTFReader(FlowNode):
    """
    CTFReader reads trace data from Common Trace Format (CTF) files.
    
    This class implements a FlowNode that reads trace files in CTF format
    and converts them into Trace objects for analysis.
    
    CTF (Common Trace Format) is a binary trace format designed to be very
    fast to write without compromising trace data quality. It is used by
    LTTng (Linux Trace Toolkit Next Generation) and other tracing tools.
    
    Attributes:
        m_file_path: Path to the CTF trace directory
        m_trace: The loaded trace object
    """
    
    def __init__(self, file_path: Optional[str] = None) -> None:
        """
        Initialize a CTFReader.
        
        Args:
            file_path: Optional path to the CTF trace directory
        """
        super().__init__()
        self.m_file_path: Optional[str] = file_path
        self.m_trace: Optional[Trace] = None
    
    def setFilePath(self, file_path: str) -> None:
        """
        Set the path to the CTF trace directory.
        
        Args:
            file_path: Path to the CTF trace directory
        """
        self.m_file_path = file_path
    
    def getFilePath(self) -> Optional[str]:
        """
        Get the path to the CTF trace directory.
        
        Returns:
            Path to the CTF trace directory
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
        Load trace data from CTF files.
        
        This method reads and parses CTF traces, creating a Trace object.
        In this initial implementation, it creates an empty trace as a placeholder.
        Full CTF parsing would require the Babeltrace2 library.
        
        CTF traces typically contain:
        - metadata: Stream and event type definitions
        - Binary stream files: Actual trace data
        - Packet headers and contexts
        
        Returns:
            Loaded Trace object
        """
        # Placeholder implementation
        # Full implementation would:
        # 1. Use Babeltrace2 Python bindings
        # 2. Parse metadata for event definitions
        # 3. Read binary stream files
        # 4. Reconstruct event timeline
        self.m_trace = Trace()
        return self.m_trace
    
    def run(self) -> None:
        """
        Execute the CTF reading task.
        
        Loads the trace from the specified CTF directory and adds it to
        the output flow data.
        """
        if self.m_file_path:
            trace = self.load()
            self.m_outputs.add_data(trace)
