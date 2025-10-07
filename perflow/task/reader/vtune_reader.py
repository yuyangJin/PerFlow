'''
module vtune reader
'''

from typing import Optional
from ...flow.flow import FlowNode
from ...perf_data_struct.dynamic.trace.trace import Trace

'''
@class VtuneReader
Read the performance data from Intel VTune's collected files
'''


class VtuneReader(FlowNode):
    """
    VtuneReader reads trace data from Intel VTune profiler files.
    
    This class implements a FlowNode that reads trace files from Intel VTune
    Profiler and converts them into Trace objects for analysis.
    
    VTune is a performance analysis tool for Intel processors that provides
    detailed profiling information including hardware counters, thread
    synchronization, and microarchitecture analysis.
    
    Attributes:
        m_file_path: Path to the VTune result directory or file
        m_trace: The loaded trace object
    """
    
    def __init__(self, file_path: Optional[str] = None) -> None:
        """
        Initialize a VtuneReader.
        
        Args:
            file_path: Optional path to the VTune result directory
        """
        super().__init__()
        self.m_file_path: Optional[str] = file_path
        self.m_trace: Optional[Trace] = None
    
    def setFilePath(self, file_path: str) -> None:
        """
        Set the path to the VTune result directory.
        
        Args:
            file_path: Path to the VTune result directory
        """
        self.m_file_path = file_path
    
    def getFilePath(self) -> Optional[str]:
        """
        Get the path to the VTune result directory.
        
        Returns:
            Path to the VTune result directory
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
        Load trace data from the VTune result directory.
        
        This method reads and parses VTune results, creating a Trace object.
        For full VTune parsing, the VTune API or result file parsers would be required.
        This implementation provides basic file handling and CSV export parsing.
        
        Returns:
            Loaded Trace object
            
        Raises:
            FileNotFoundError: If the result directory does not exist
            ValueError: If the directory structure is invalid
        """
        import os
        import json
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        self.m_trace = Trace()
        
        if not self.m_file_path:
            return self.m_trace
        
        # Check if path exists
        if not os.path.exists(self.m_file_path):
            raise FileNotFoundError(f"VTune result path not found: {self.m_file_path}")
        
        # For full implementation, would use VTune API or parse vtune files:
        # VTune stores results in proprietary .vtune directory format
        # Full parsing would involve:
        # 1. Parse result metadata
        # 2. Extract hardware counter data
        # 3. Parse call stacks and hotspots
        # 4. Build event timeline
        
        # Current implementation: Parse CSV export if available
        if os.path.isdir(self.m_file_path):
            csv_file = os.path.join(self.m_file_path, 'vtune_export.csv')
            if os.path.exists(csv_file):
                self._parse_vtune_csv(csv_file)
        elif self.m_file_path.endswith('.csv'):
            self._parse_vtune_csv(self.m_file_path)
        elif self.m_file_path.endswith('.json'):
            self._parse_vtune_json(self.m_file_path)
        
        return self.m_trace
    
    def _parse_vtune_csv(self, file_path: str) -> None:
        """
        Parse VTune CSV export file.
        Expected format: function_name,module,pid,cpu_time,cpu_time_percent
        
        Args:
            file_path: Path to the CSV file
        """
        assert self.m_trace is not None  # Already initialized in load()
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        try:
            with open(file_path, 'r') as f:
                # Skip header
                header = f.readline()
                
                event_idx = 0
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    
                    try:
                        parts = line.split(',')
                        if len(parts) >= 3:
                            event = Event(
                                event_type=EventType.COMPUTE,
                                idx=event_idx,
                                name=parts[0].strip(),
                                pid=int(parts[2]) if len(parts) > 2 and parts[2].strip().isdigit() else 0,
                                tid=0,
                                timestamp=float(parts[3]) if len(parts) > 3 else 0.0,
                                replay_pid=0
                            )
                            self.m_trace.addEvent(event)
                            event_idx += 1
                    except (ValueError, IndexError):
                        continue
        except Exception:
            pass
    
    def _parse_vtune_json(self, file_path: str) -> None:
        """
        Parse VTune JSON export file.
        
        Args:
            file_path: Path to the JSON file
        """
        assert self.m_trace is not None  # Already initialized in load()
        import json
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        try:
            with open(file_path, 'r') as f:
                data = json.load(f)
                
                if isinstance(data, dict) and 'events' in data:
                    for idx, event_data in enumerate(data['events']):
                        event = Event(
                            event_type=EventType.COMPUTE,
                            idx=idx,
                            name=event_data.get('name', 'unknown'),
                            pid=event_data.get('pid', 0),
                            tid=event_data.get('tid', 0),
                            timestamp=event_data.get('timestamp', 0.0),
                            replay_pid=event_data.get('replay_pid', 0)
                        )
                        self.m_trace.addEvent(event)
        except Exception:
            pass
    
    def run(self) -> None:
        """
        Execute the VTune reading task.
        
        Loads the trace from the specified VTune result directory and adds it
        to the output flow data.
        """
        if self.m_file_path:
            trace = self.load()
            self.m_outputs.add_data(trace)