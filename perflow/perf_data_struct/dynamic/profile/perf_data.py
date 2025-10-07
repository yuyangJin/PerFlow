'''
module perf data
'''

from typing import List, Optional, Dict
from .sample_data import SampleData

'''
@class ProfileInfo
Basic information of the collected profile
'''


class ProfileInfo:
    """
    ProfileInfo contains metadata about a profile collection.
    
    This class stores comprehensive information about profiling data collection
    from parallel applications. It includes both local profile metadata
    (process/thread IDs) and global metadata (profiling configuration, 
    timing information, and metrics).
    
    Attributes:
        m_pid: Process ID that this profile belongs to (local)
        m_tid: Thread ID that this profile belongs to (local)
        m_num_processes: Total number of processes in the profile collection
        m_num_threads: Total number of threads per process
        m_profile_format: Format of the profile data (e.g., "VTune", "Nsight", "perf")
        m_sample_rate: Sampling rate in Hz (samples per second)
        m_profile_start_time: Global start time of the profile collection
        m_profile_end_time: Global end time of the profile collection
        m_application_name: Name of the application that generated the profile
        m_metrics: List of performance metrics collected (e.g., "cycles", "instructions")
    """
    
    def __init__(
        self,
        pid: Optional[int] = None,
        tid: Optional[int] = None,
        num_processes: Optional[int] = None,
        num_threads: Optional[int] = None,
        profile_format: Optional[str] = None,
        sample_rate: Optional[float] = None,
        profile_start_time: Optional[float] = None,
        profile_end_time: Optional[float] = None,
        application_name: Optional[str] = None,
        metrics: Optional[List[str]] = None
    ) -> None:
        """
        Initialize ProfileInfo object.
        
        Args:
            pid: Process ID that this profile belongs to (local)
            tid: Thread ID that this profile belongs to (local)
            num_processes: Total number of processes
            num_threads: Total number of threads per process
            profile_format: Format of the profile data
            sample_rate: Sampling rate in Hz
            profile_start_time: Global start timestamp of the profile collection
            profile_end_time: Global end timestamp of the profile collection
            application_name: Name of the application that generated the profile
            metrics: List of performance metrics collected
        """
        # Local profile metadata
        self.m_pid: Optional[int] = pid
        self.m_tid: Optional[int] = tid
        
        # Global profile collection metadata
        self.m_num_processes: Optional[int] = num_processes
        self.m_num_threads: Optional[int] = num_threads
        
        # Profiling configuration
        self.m_profile_format: Optional[str] = profile_format
        self.m_sample_rate: Optional[float] = sample_rate
        
        # Timing information
        self.m_profile_start_time: Optional[float] = profile_start_time
        self.m_profile_end_time: Optional[float] = profile_end_time
        
        # Additional metadata
        self.m_application_name: Optional[str] = application_name
        self.m_metrics: List[str] = metrics if metrics is not None else []
    
    def getPid(self) -> Optional[int]:
        """Get the process ID."""
        return self.m_pid
    
    def getTid(self) -> Optional[int]:
        """Get the thread ID."""
        return self.m_tid
    
    def setPid(self, pid: int) -> None:
        """Set the process ID."""
        self.m_pid = pid
    
    def setTid(self, tid: int) -> None:
        """Set the thread ID."""
        self.m_tid = tid
    
    def getNumProcesses(self) -> Optional[int]:
        """Get the total number of processes."""
        return self.m_num_processes
    
    def setNumProcesses(self, num_processes: int) -> None:
        """Set the total number of processes."""
        self.m_num_processes = num_processes
    
    def getNumThreads(self) -> Optional[int]:
        """Get the total number of threads per process."""
        return self.m_num_threads
    
    def setNumThreads(self, num_threads: int) -> None:
        """Set the total number of threads per process."""
        self.m_num_threads = num_threads
    
    def getProfileFormat(self) -> Optional[str]:
        """Get the profile format."""
        return self.m_profile_format
    
    def setProfileFormat(self, profile_format: str) -> None:
        """Set the profile format."""
        self.m_profile_format = profile_format
    
    def getSampleRate(self) -> Optional[float]:
        """Get the sampling rate in Hz."""
        return self.m_sample_rate
    
    def setSampleRate(self, sample_rate: float) -> None:
        """Set the sampling rate in Hz."""
        self.m_sample_rate = sample_rate
    
    def getProfileStartTime(self) -> Optional[float]:
        """Get the global start time of the profile collection."""
        return self.m_profile_start_time
    
    def setProfileStartTime(self, start_time: float) -> None:
        """Set the global start time of the profile collection."""
        self.m_profile_start_time = start_time
    
    def getProfileEndTime(self) -> Optional[float]:
        """Get the global end time of the profile collection."""
        return self.m_profile_end_time
    
    def setProfileEndTime(self, end_time: float) -> None:
        """Set the global end time of the profile collection."""
        self.m_profile_end_time = end_time
    
    def getProfileDuration(self) -> Optional[float]:
        """
        Calculate the total duration of the profile collection.
        
        Returns:
            Duration in seconds, or None if start/end times are not set
        """
        if self.m_profile_start_time is not None and self.m_profile_end_time is not None:
            return self.m_profile_end_time - self.m_profile_start_time
        return None
    
    def getApplicationName(self) -> Optional[str]:
        """Get the application name."""
        return self.m_application_name
    
    def setApplicationName(self, app_name: str) -> None:
        """Set the application name."""
        self.m_application_name = app_name
    
    def getMetrics(self) -> List[str]:
        """Get the list of performance metrics collected."""
        return self.m_metrics
    
    def setMetrics(self, metrics: List[str]) -> None:
        """Set the list of performance metrics collected."""
        self.m_metrics = metrics
    
    def addMetric(self, metric: str) -> None:
        """
        Add a performance metric to the list.
        
        Args:
            metric: Name of the metric (e.g., "cycles", "instructions")
        """
        if metric not in self.m_metrics:
            self.m_metrics.append(metric)


'''
@class PerfData
Data structure for performance data 
'''


class PerfData:
    """
    PerfData represents a collection of profiling samples from program execution.
    
    This class serves as a container for sampling-based profiling data and their
    associated metadata. It provides methods to add, access, and manage samples
    within the profile.
    
    Attributes:
        m_samples: List of profiling samples
        m_profileinfo: Metadata about this profile (process/thread info, metrics)
        m_aggregated_data: Dictionary for aggregated metrics by function/module
    """
    
    def __init__(self) -> None:
        """
        Initialize a PerfData object.
        
        Creates an empty profile with no samples and default profile info.
        """
        self.m_samples: List[SampleData] = []
        self.m_profileinfo: ProfileInfo = ProfileInfo()
        self.m_aggregated_data: Dict[str, Dict[str, float]] = {}
    
    def addSample(self, sample: SampleData) -> None:
        """
        Add a sample to the profile.
        
        Args:
            sample: The sample to add to this profile
        """
        self.m_samples.append(sample)
    
    def getSamples(self) -> List[SampleData]:
        """
        Get all samples in the profile.
        
        Returns:
            List of all samples in this profile
        """
        return self.m_samples
    
    def getSample(self, index: int) -> Optional[SampleData]:
        """
        Get a specific sample by index.
        
        Args:
            index: Index of the sample to retrieve
            
        Returns:
            The sample at the specified index, or None if out of bounds
        """
        if 0 <= index < len(self.m_samples):
            return self.m_samples[index]
        return None
    
    def getSampleCount(self) -> int:
        """
        Get the total number of samples in the profile.
        
        Returns:
            Number of samples
        """
        return len(self.m_samples)
    
    def getProfileInfo(self) -> ProfileInfo:
        """
        Get the profile metadata.
        
        Returns:
            ProfileInfo object containing metadata about this profile
        """
        return self.m_profileinfo
    
    def setProfileInfo(self, profile_info: ProfileInfo) -> None:
        """
        Set the profile metadata.
        
        Args:
            profile_info: ProfileInfo object to set for this profile
        """
        self.m_profileinfo = profile_info
    
    def clear(self) -> None:
        """Clear all samples from the profile."""
        self.m_samples.clear()
        self.m_aggregated_data.clear()
    
    def aggregateByFunction(self) -> Dict[str, Dict[str, float]]:
        """
        Aggregate samples by function name.
        
        Returns:
            Dictionary mapping function names to aggregated metrics
        """
        self.m_aggregated_data.clear()
        
        for sample in self.m_samples:
            func_name = sample.getFunctionName()
            if func_name:
                if func_name not in self.m_aggregated_data:
                    self.m_aggregated_data[func_name] = {}
                
                # Aggregate metric values
                for metric, value in sample.getMetrics().items():
                    if metric not in self.m_aggregated_data[func_name]:
                        self.m_aggregated_data[func_name][metric] = 0.0
                    self.m_aggregated_data[func_name][metric] += value
        
        return self.m_aggregated_data
    
    def getAggregatedData(self) -> Dict[str, Dict[str, float]]:
        """
        Get the aggregated data.
        
        Returns:
            Dictionary containing aggregated metrics by function
        """
        return self.m_aggregated_data
    
    def getTopFunctions(self, metric: str, top_n: int = 10) -> List[tuple]:
        """
        Get the top N functions by a specific metric.
        
        Args:
            metric: Name of the metric to sort by
            top_n: Number of top functions to return
            
        Returns:
            List of tuples (function_name, metric_value) sorted by metric value
        """
        if not self.m_aggregated_data:
            self.aggregateByFunction()
        
        # Extract functions with the specified metric
        func_metrics = []
        for func_name, metrics in self.m_aggregated_data.items():
            if metric in metrics:
                func_metrics.append((func_name, metrics[metric]))
        
        # Sort by metric value in descending order
        func_metrics.sort(key=lambda x: x[1], reverse=True)
        
        return func_metrics[:top_n]