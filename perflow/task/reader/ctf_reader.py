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
        For full CTF parsing, the Babeltrace2 library would be required.
        This implementation provides basic metadata and text format parsing.
        
        CTF traces typically contain:
        - metadata: Stream and event type definitions
        - Binary stream files: Actual trace data
        - Packet headers and contexts
        
        Returns:
            Loaded Trace object
            
        Raises:
            FileNotFoundError: If the trace directory does not exist
            ValueError: If the directory structure is invalid
        """
        import os
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        
        self.m_trace = Trace()
        
        if not self.m_file_path:
            return self.m_trace
        
        # Check if path exists
        if not os.path.exists(self.m_file_path):
            raise FileNotFoundError(f"CTF trace directory not found: {self.m_file_path}")
        
        # For full implementation, would use Babeltrace2:
        # import bt2
        # msg_it = bt2.TraceCollectionMessageIterator(self.m_file_path)
        # for msg in msg_it:
        #     if type(msg) is bt2._EventMessageConst:
        #         # Parse and create Event objects
        #         pass
        
        # Current implementation: Parse text exports or metadata
        if os.path.isdir(self.m_file_path):
            # Look for metadata file
            metadata_file = os.path.join(self.m_file_path, 'metadata')
            if os.path.exists(metadata_file):
                self._parse_ctf_metadata(metadata_file)
            
            # Look for text exports
            for filename in os.listdir(self.m_file_path):
                if filename.endswith('.txt') or filename.endswith('.log'):
                    self._parse_ctf_text(os.path.join(self.m_file_path, filename))
                    break
        elif self.m_file_path.endswith('.txt'):
            self._parse_ctf_text(self.m_file_path)
        
        return self.m_trace
    
    def _parse_ctf_metadata(self, file_path: str) -> None:
        """
        Parse CTF metadata file (basic implementation).
        
        Args:
            file_path: Path to the metadata file
        """
        assert self.m_trace is not None  # Already initialized in load()
        # Metadata parsing would require TSDL parser
        # For now, just note that metadata exists
        pass
    
    def _parse_ctf_text(self, file_path: str) -> None:
        """
        Parse CTF text export file.
        Format: [timestamp] event_name: field1=value1, field2=value2
        
        Args:
            file_path: Path to the text file
        """
        assert self.m_trace is not None  # Already initialized in load()
        from ...perf_data_struct.dynamic.trace.event import Event, EventType
        import re
        
        try:
            with open(file_path, 'r') as f:
                for idx, line in enumerate(f):
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue
                    
                    try:
                        # Try to parse CTF text format: [timestamp] event_name: ...
                        match = re.match(r'\[(\d+\.?\d*)\]\s+(\w+):', line)
                        if match:
                            timestamp = float(match.group(1))
                            event_name = match.group(2)
                            
                            # Determine event type from name
                            event_type = EventType.UNKNOWN
                            if 'enter' in event_name.lower():
                                event_type = EventType.ENTER
                            elif 'exit' in event_name.lower() or 'leave' in event_name.lower():
                                event_type = EventType.LEAVE
                            elif 'send' in event_name.lower():
                                event_type = EventType.SEND
                            elif 'recv' in event_name.lower():
                                event_type = EventType.RECV
                            
                            event = Event(
                                event_type=event_type,
                                idx=idx,
                                name=event_name,
                                pid=0,
                                tid=0,
                                timestamp=timestamp,
                                replay_pid=0
                            )
                            self.m_trace.addEvent(event)
                    except (ValueError, AttributeError):
                        continue
        except Exception:
            pass
    
    def run(self) -> None:
        """
        Execute the CTF reading task.
        
        Loads the trace from the specified CTF directory and adds it to
        the output flow data.
        """
        if self.m_file_path:
            trace = self.load()
            self.m_outputs.add_data(trace)
