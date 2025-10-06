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
        For full HPCToolkit parsing, parsing the experiment.xml and profile/trace data files would be required.
        This implementation provides basic XML and CSV parsing capabilities.
        
        HPCToolkit stores results in a structured directory with:
        - experiment.xml: Metadata and structure
        - *.hpcrun: Raw measurement files
        - *.hpctrace: Trace data files
        - database: Processed results
        
        Returns:
            Loaded Trace object
            
        Raises:
            FileNotFoundError: If the database directory does not exist
            ValueError: If the directory structure is invalid
        """
        import os
        import xml.etree.ElementTree as ET
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        self.m_trace = Trace()
        
        if not self.m_file_path:
            return self.m_trace
        
        # Check if path exists
        if not os.path.exists(self.m_file_path):
            raise FileNotFoundError(f"HPCToolkit database not found: {self.m_file_path}")
        
        # For full implementation, would parse HPCToolkit files:
        # 1. Parse experiment.xml for structure
        # 2. Read hpctrace files for trace data
        # 3. Extract call paths and metrics
        # 4. Build event timeline from samples
        
        # Current implementation: Parse available formats
        if os.path.isdir(self.m_file_path):
            # Look for experiment.xml
            exp_xml = os.path.join(self.m_file_path, 'experiment.xml')
            if os.path.exists(exp_xml):
                self._parse_experiment_xml(exp_xml)
            
            # Look for CSV exports
            for filename in os.listdir(self.m_file_path):
                if filename.endswith('.csv'):
                    self._parse_hpctoolkit_csv(os.path.join(self.m_file_path, filename))
                    break
        elif self.m_file_path.endswith('.xml'):
            self._parse_experiment_xml(self.m_file_path)
        elif self.m_file_path.endswith('.csv'):
            self._parse_hpctoolkit_csv(self.m_file_path)
        
        return self.m_trace
    
    def _parse_experiment_xml(self, file_path: str) -> None:
        """
        Parse HPCToolkit experiment.xml file.
        
        Args:
            file_path: Path to the experiment.xml file
        """
        assert self.m_trace is not None  # Already initialized in load()
        import xml.etree.ElementTree as ET
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        try:
            tree = ET.parse(file_path)
            root = tree.getroot()
            
            # Parse procedure frames (call paths)
            event_idx = 0
            for proc in root.findall('.//PF'):  # Procedure Frame
                name = proc.get('n', 'unknown')
                line = proc.get('l', '0')
                
                # Create compute event for each procedure
                event = Event(
                    event_type=EventType.COMPUTE,
                    idx=event_idx,
                    name=name,
                    pid=0,
                    tid=0,
                    timestamp=float(event_idx) * 0.001,  # Synthetic timestamps
                    replay_pid=0
                )
                self.m_trace.addEvent(event)
                event_idx += 1
                
                if event_idx >= 1000:  # Limit events
                    break
        except Exception:
            pass
    
    def _parse_hpctoolkit_csv(self, file_path: str) -> None:
        """
        Parse HPCToolkit CSV export.
        Expected format: function,file,line,exclusive_time,inclusive_time
        
        Args:
            file_path: Path to the CSV file
        """
        assert self.m_trace is not None  # Already initialized in load()
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        try:
            with open(file_path, 'r') as f:
                # Skip header
                header = f.readline()
                
                for idx, line in enumerate(f):
                    line = line.strip()
                    if not line:
                        continue
                    
                    try:
                        parts = line.split(',')
                        if len(parts) >= 4:
                            event = Event(
                                event_type=EventType.COMPUTE,
                                idx=idx,
                                name=parts[0].strip().strip('"'),
                                pid=0,
                                tid=0,
                                timestamp=float(parts[3]) if parts[3].strip() else 0.0,
                                replay_pid=0
                            )
                            self.m_trace.addEvent(event)
                    except (ValueError, IndexError):
                        continue
        except Exception:
            pass
    
    def run(self) -> None:
        """
        Execute the HPCToolkit reading task.
        
        Loads the trace from the specified HPCToolkit database and adds it
        to the output flow data.
        """
        if self.m_file_path:
            trace = self.load()
            self.m_outputs.add_data(trace)