'''
module sample data
'''

from typing import Optional, Dict, List

'''
@class SampleData
Data structure for sampling data 
'''


class SampleData:
    """
    SampleData represents a single profiling sample from program execution.
    
    A sample typically contains information about where the program was executing
    at a particular point in time, along with performance counter values.
    
    Attributes:
        m_timestamp: Timestamp when the sample was collected
        m_pid: Process ID
        m_tid: Thread ID
        m_function_name: Name of the function being executed
        m_module_name: Name of the module/library
        m_source_file: Source file path
        m_line_number: Line number in source file
        m_instruction_pointer: Instruction pointer (address)
        m_metrics: Dictionary of performance metrics (e.g., {"cycles": 1000, "instructions": 500})
        m_call_stack: Call stack at the time of sampling
    """
    
    def __init__(
        self,
        timestamp: Optional[float] = None,
        pid: Optional[int] = None,
        tid: Optional[int] = None,
        function_name: Optional[str] = None,
        module_name: Optional[str] = None,
        source_file: Optional[str] = None,
        line_number: Optional[int] = None,
        instruction_pointer: Optional[int] = None,
        metrics: Optional[Dict[str, float]] = None,
        call_stack: Optional[List[str]] = None
    ) -> None:
        """
        Initialize a SampleData object.
        
        Args:
            timestamp: Timestamp when the sample was collected
            pid: Process ID
            tid: Thread ID
            function_name: Name of the function being executed
            module_name: Name of the module/library
            source_file: Source file path
            line_number: Line number in source file
            instruction_pointer: Instruction pointer (address)
            metrics: Dictionary of performance metrics
            call_stack: Call stack at the time of sampling
        """
        self.m_timestamp: Optional[float] = timestamp
        self.m_pid: Optional[int] = pid
        self.m_tid: Optional[int] = tid
        self.m_function_name: Optional[str] = function_name
        self.m_module_name: Optional[str] = module_name
        self.m_source_file: Optional[str] = source_file
        self.m_line_number: Optional[int] = line_number
        self.m_instruction_pointer: Optional[int] = instruction_pointer
        self.m_metrics: Dict[str, float] = metrics if metrics is not None else {}
        self.m_call_stack: List[str] = call_stack if call_stack is not None else []
    
    def getTimestamp(self) -> Optional[float]:
        """Get the timestamp."""
        return self.m_timestamp
    
    def setTimestamp(self, timestamp: float) -> None:
        """Set the timestamp."""
        self.m_timestamp = timestamp
    
    def getPid(self) -> Optional[int]:
        """Get the process ID."""
        return self.m_pid
    
    def setPid(self, pid: int) -> None:
        """Set the process ID."""
        self.m_pid = pid
    
    def getTid(self) -> Optional[int]:
        """Get the thread ID."""
        return self.m_tid
    
    def setTid(self, tid: int) -> None:
        """Set the thread ID."""
        self.m_tid = tid
    
    def getFunctionName(self) -> Optional[str]:
        """Get the function name."""
        return self.m_function_name
    
    def setFunctionName(self, function_name: str) -> None:
        """Set the function name."""
        self.m_function_name = function_name
    
    def getModuleName(self) -> Optional[str]:
        """Get the module name."""
        return self.m_module_name
    
    def setModuleName(self, module_name: str) -> None:
        """Set the module name."""
        self.m_module_name = module_name
    
    def getSourceFile(self) -> Optional[str]:
        """Get the source file path."""
        return self.m_source_file
    
    def setSourceFile(self, source_file: str) -> None:
        """Set the source file path."""
        self.m_source_file = source_file
    
    def getLineNumber(self) -> Optional[int]:
        """Get the line number."""
        return self.m_line_number
    
    def setLineNumber(self, line_number: int) -> None:
        """Set the line number."""
        self.m_line_number = line_number
    
    def getInstructionPointer(self) -> Optional[int]:
        """Get the instruction pointer."""
        return self.m_instruction_pointer
    
    def setInstructionPointer(self, instruction_pointer: int) -> None:
        """Set the instruction pointer."""
        self.m_instruction_pointer = instruction_pointer
    
    def getMetrics(self) -> Dict[str, float]:
        """Get all performance metrics."""
        return self.m_metrics
    
    def setMetrics(self, metrics: Dict[str, float]) -> None:
        """Set all performance metrics."""
        self.m_metrics = metrics
    
    def getMetric(self, metric_name: str) -> Optional[float]:
        """
        Get a specific metric value.
        
        Args:
            metric_name: Name of the metric
            
        Returns:
            Metric value, or None if not found
        """
        return self.m_metrics.get(metric_name)
    
    def setMetric(self, metric_name: str, value: float) -> None:
        """
        Set a specific metric value.
        
        Args:
            metric_name: Name of the metric
            value: Metric value
        """
        self.m_metrics[metric_name] = value
    
    def getCallStack(self) -> List[str]:
        """Get the call stack."""
        return self.m_call_stack
    
    def setCallStack(self, call_stack: List[str]) -> None:
        """Set the call stack."""
        self.m_call_stack = call_stack
    
    def addToCallStack(self, frame: str) -> None:
        """
        Add a frame to the call stack.
        
        Args:
            frame: Function name or frame description
        """
        self.m_call_stack.append(frame)
    
    def getCallStackDepth(self) -> int:
        """
        Get the depth of the call stack.
        
        Returns:
            Number of frames in the call stack
        """
        return len(self.m_call_stack)