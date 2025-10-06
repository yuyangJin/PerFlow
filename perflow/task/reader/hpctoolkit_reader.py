'''
module hpctoolkit reader
'''

from typing import Optional
from ...flow.flow import FlowNode
from ...perf_data_struct.dynamic.trace.trace import Trace

'''
@class HpctoolkitReader
Read the performance data from HPCToolkit's collected files
'''


class HpctoolkitReader(FlowNode):
    """
    HpctoolkitReader reads trace data from HPCToolkit database files.
    
    This class implements a FlowNode that reads trace and profile files from
    HPCToolkit and converts them into Trace objects for analysis.
    
    HPCToolkit is an integrated suite of tools for measurement and analysis
    of program performance on computers ranging from multicore desktops to
    supercomputers. It provides call path profiling, sampling-based analysis,
    and source code correlation.
    
    Attributes:
        m_file_path: Path to the HPCToolkit database directory
        m_trace: The loaded trace object
    """
    
    def __init__(self, file_path: Optional[str] = None) -> None:
        """
        Initialize a HpctoolkitReader.
        
        Args:
            file_path: Optional path to the HPCToolkit database directory
        """
        super().__init__()
        self.m_file_path: Optional[str] = file_path
        self.m_trace: Optional[Trace] = None
    
    def setFilePath(self, file_path: str) -> None:
        """
        Set the path to the HPCToolkit database directory.
        
        Args:
            file_path: Path to the HPCToolkit database directory
        """
        self.m_file_path = file_path
    
    def getFilePath(self) -> Optional[str]:
        """
        Get the path to the HPCToolkit database directory.
        
        Returns:
            Path to the HPCToolkit database directory
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
        Load trace data from the HPCToolkit database.
        
        This method reads and parses HPCToolkit results, creating a Trace object.
        In this initial implementation, it creates an empty trace as a placeholder.
        Full HPCToolkit parsing would require parsing the experiment.xml and
        profile/trace data files.
        
        HPCToolkit stores results in a structured directory with:
        - experiment.xml: Metadata and structure
        - *.hpcrun: Raw measurement files
        - *.hpctrace: Trace data files
        - database: Processed results
        
        Returns:
            Loaded Trace object
        """
        # Placeholder implementation
        # Full implementation would:
        # 1. Parse experiment.xml for structure
        # 2. Read hpctrace files for trace data
        # 3. Extract call paths and metrics
        # 4. Build event timeline from samples
        self.m_trace = Trace()
        return self.m_trace
    
    def run(self) -> None:
        """
        Execute the HPCToolkit reading task.
        
        Loads the trace from the specified HPCToolkit database and adds it
        to the output flow data.
        """
        if self.m_file_path:
            trace = self.load()
            self.m_outputs.add_data(trace)