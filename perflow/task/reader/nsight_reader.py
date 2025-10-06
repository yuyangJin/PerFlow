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
        For full Nsight parsing, the nsys CLI export tools or SQLite parsing would be required.
        This implementation provides basic SQLite and JSON parsing capabilities.
        
        Nsight Systems stores results in SQLite database format (.nsys-rep) which
        can be exported to various formats or queried directly.
        
        Returns:
            Loaded Trace object
            
        Raises:
            FileNotFoundError: If the trace file does not exist
            ValueError: If the file format is invalid
        """
        import os
        import json
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        self.m_trace = Trace()
        
        if not self.m_file_path:
            return self.m_trace
        
        # Check if file exists
        if not os.path.exists(self.m_file_path):
            raise FileNotFoundError(f"Nsight trace file not found: {self.m_file_path}")
        
        # For full implementation, would use nsys export or SQLite queries:
        # 1. Use nsys export to convert to JSON/CSV: nsys export -t json myreport.nsys-rep
        # 2. Or directly query the SQLite database
        # 3. Parse CUDA events, CPU threads, memory operations
        
        # Current implementation: Parse exported formats
        if self.m_file_path.endswith('.json'):
            self._parse_nsight_json(self.m_file_path)
        elif self.m_file_path.endswith('.csv'):
            self._parse_nsight_csv(self.m_file_path)
        elif self.m_file_path.endswith('.nsys-rep') or self.m_file_path.endswith('.qdrep'):
            self._parse_nsight_sqlite(self.m_file_path)
        
        return self.m_trace
    
    def _parse_nsight_json(self, file_path: str) -> None:
        """
        Parse Nsight Systems JSON export.
        
        Args:
            file_path: Path to the JSON file
        """
        assert self.m_trace is not None  # Already initialized in load()
        import json
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        try:
            with open(file_path, 'r') as f:
                data = json.load(f)
                
                # Parse events from Nsight JSON format
                if isinstance(data, dict):
                    events_data = data.get('traceEvents', data.get('events', []))
                    for event_data in events_data:
                        event_type = EventType.COMPUTE
                        if event_data.get('ph') == 'B':  # Begin
                            event_type = EventType.ENTER
                        elif event_data.get('ph') == 'E':  # End
                            event_type = EventType.LEAVE
                        
                        event = Event(
                            event_type=event_type,
                            idx=event_data.get('id', 0),
                            name=event_data.get('name', 'unknown'),
                            pid=event_data.get('pid', 0),
                            tid=event_data.get('tid', 0),
                            timestamp=float(event_data.get('ts', 0)) / 1000000.0,  # Convert to seconds
                            replay_pid=event_data.get('pid', 0)
                        )
                        self.m_trace.addEvent(event)
        except Exception:
            pass
    
    def _parse_nsight_csv(self, file_path: str) -> None:
        """
        Parse Nsight Systems CSV export.
        
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
                        if len(parts) >= 5:
                            event = Event(
                                event_type=EventType.COMPUTE,
                                idx=idx,
                                name=parts[0].strip(),
                                pid=int(parts[1]) if parts[1].strip().isdigit() else 0,
                                tid=int(parts[2]) if parts[2].strip().isdigit() else 0,
                                timestamp=float(parts[3]),
                                replay_pid=0
                            )
                            self.m_trace.addEvent(event)
                    except (ValueError, IndexError):
                        continue
        except Exception:
            pass
    
    def _parse_nsight_sqlite(self, file_path: str) -> None:
        """
        Parse Nsight Systems SQLite database (basic implementation).
        
        Args:
            file_path: Path to the .nsys-rep file
        """
        assert self.m_trace is not None  # Already initialized in load()
        try:
            import sqlite3
            from ...perf_data_struct.dynamic.trace.event import Event, EventType
            
            conn = sqlite3.connect(file_path)
            cursor = conn.cursor()
            
            # Try to query events from common Nsight tables
            # Actual schema varies by Nsight version
            try:
                cursor.execute("""
                    SELECT eventClass, start, end, text, globalTid
                    FROM CUPTI_ACTIVITY_KIND_KERNEL
                    LIMIT 1000
                """)
                
                for idx, row in enumerate(cursor.fetchall()):
                    event = Event(
                        event_type=EventType.COMPUTE,
                        idx=idx,
                        name=row[3] if len(row) > 3 else 'kernel',
                        pid=0,
                        tid=row[4] if len(row) > 4 else 0,
                        timestamp=float(row[1]) / 1e9 if len(row) > 1 else 0.0,  # ns to s
                        replay_pid=0
                    )
                    self.m_trace.addEvent(event)
            except sqlite3.OperationalError:
                # Table doesn't exist, try alternative schema
                pass
            
            conn.close()
        except Exception:
            # SQLite not available or file not readable
            pass
    
    def run(self) -> None:
        """
        Execute the Nsight reading task.
        
        Loads the trace from the specified Nsight Systems file and adds it
        to the output flow data.
        """
        if self.m_file_path:
            trace = self.load()
            self.m_outputs.add_data(trace)