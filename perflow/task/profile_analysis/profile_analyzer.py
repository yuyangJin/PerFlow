'''
module profile analyzer
'''

from typing import Callable, Optional, Dict, List
from ...flow.flow import FlowNode
from ...perf_data_struct.dynamic.profile.perf_data import PerfData
from ...perf_data_struct.dynamic.profile.sample_data import SampleData


'''
@class ProfileAnalyzer
Base class for profile analysis. Provides functionality to iterate through samples
and perform analysis using callback functions.
'''


class ProfileAnalyzer(FlowNode):
    """
    ProfileAnalyzer provides base functionality for analyzing profiling data.
    
    This class iterates through profiling samples and invokes registered callback
    functions for each sample, enabling various profile analysis tasks such as
    hotspot detection, call stack analysis, and performance metric aggregation.
    
    Attributes:
        m_profile: The profile data to be analyzed
        m_callbacks: Dictionary of callback functions
    """
    
    def __init__(self, profile: Optional[PerfData] = None) -> None:
        """
        Initialize a ProfileAnalyzer.
        
        Args:
            profile: Optional profile data to analyze
        """
        super().__init__()
        self.m_profile: Optional[PerfData] = profile
        self.m_callbacks: Dict[str, Callable[[SampleData], None]] = {}
    
    def setProfile(self, profile: PerfData) -> None:
        """
        Set the profile data to be analyzed.
        
        Args:
            profile: PerfData object to analyze
        """
        self.m_profile = profile
    
    def getProfile(self) -> Optional[PerfData]:
        """
        Get the profile data being analyzed.
        
        Returns:
            The PerfData object
        """
        return self.m_profile
    
    def registerCallback(self, name: str, callback: Callable[[SampleData], None]) -> None:
        """
        Register a callback function to be invoked for each sample.
        
        Callbacks are invoked during analysis for each sample, allowing
        custom analysis logic to be executed.
        
        Args:
            name: Name identifier for the callback
            callback: Function that takes a SampleData and returns None
        """
        self.m_callbacks[name] = callback
    
    def unregisterCallback(self, name: str) -> None:
        """
        Unregister a callback function.
        
        Args:
            name: Name identifier of the callback to remove
        """
        if name in self.m_callbacks:
            del self.m_callbacks[name]
    
    def clearCallbacks(self) -> None:
        """Clear all registered callbacks."""
        self.m_callbacks.clear()
    
    def getCallbacks(self) -> Dict[str, Callable[[SampleData], None]]:
        """
        Get all registered callbacks.
        
        Returns:
            Dictionary of callback functions
        """
        return self.m_callbacks
    
    def analyze(self) -> None:
        """
        Analyze the profile by iterating through all samples.
        
        For each sample, all registered callbacks are invoked in the order
        they were registered.
        """
        if self.m_profile is None:
            return
        
        samples = self.m_profile.getSamples()
        
        for sample in samples:
            for callback in self.m_callbacks.values():
                callback(sample)
    
    def run(self) -> None:
        """
        Execute profile analysis.
        
        Processes input profiles and invokes analysis callbacks.
        Results should be stored by derived classes or callbacks.
        """
        # Process profiles from input data
        for data in self.m_inputs.get_data():
            if isinstance(data, PerfData):
                self.setProfile(data)
                self.analyze()
                
                # Add analysis results to outputs
                output = ("ProfileAnalysis", data)
                self.m_outputs.add_data(output)
