'''
module hotspot analyzer
'''

from typing import Dict, List, Optional, Tuple
from .profile_analyzer import ProfileAnalyzer
from ...perf_data_struct.dynamic.profile.perf_data import PerfData
from ...perf_data_struct.dynamic.profile.sample_data import SampleData


'''
@class HotspotAnalyzer
Hotspot analysis identifies functions or code regions that consume the most
execution time or resources based on profiling samples.
'''


class HotspotAnalyzer(ProfileAnalyzer):
    """
    HotspotAnalyzer identifies performance hotspots in profiling data.
    
    This class extends ProfileAnalyzer to identify functions or code regions
    that consume significant execution resources. It aggregates samples by
    function and ranks them by various metrics (e.g., cycles, instructions).
    
    Attributes:
        m_hotspots: Dictionary mapping functions to aggregated metrics
        m_threshold: Minimum threshold for considering a function as a hotspot
    """
    
    def __init__(self, profile: Optional[PerfData] = None, threshold: float = 0.0) -> None:
        """
        Initialize a HotspotAnalyzer.
        
        Args:
            profile: Optional profile data to analyze
            threshold: Minimum metric value threshold for hotspot detection
        """
        super().__init__(profile)
        self.m_hotspots: Dict[str, Dict[str, float]] = {}
        self.m_threshold: float = threshold
        
        # Register callback for hotspot detection
        self.registerCallback("hotspot_detector", self._detect_hotspots)
    
    def _detect_hotspots(self, sample: SampleData) -> None:
        """
        Callback to detect hotspots during profile analysis.
        
        Args:
            sample: Sample being processed during analysis
        """
        func_name = sample.getFunctionName()
        
        if func_name:
            # Initialize hotspot entry if needed
            if func_name not in self.m_hotspots:
                self.m_hotspots[func_name] = {
                    "sample_count": 0,
                    "total_cycles": 0.0,
                    "total_instructions": 0.0
                }
            
            # Aggregate metrics
            self.m_hotspots[func_name]["sample_count"] += 1
            
            # Add cycle count if available
            cycles = sample.getMetric("cycles")
            if cycles is not None:
                self.m_hotspots[func_name]["total_cycles"] += cycles
            
            # Add instruction count if available
            instructions = sample.getMetric("instructions")
            if instructions is not None:
                self.m_hotspots[func_name]["total_instructions"] += instructions
            
            # Add any other metrics
            for metric, value in sample.getMetrics().items():
                if metric not in ["cycles", "instructions"]:
                    metric_key = f"total_{metric}"
                    if metric_key not in self.m_hotspots[func_name]:
                        self.m_hotspots[func_name][metric_key] = 0.0
                    self.m_hotspots[func_name][metric_key] += value
    
    def getHotspots(self) -> Dict[str, Dict[str, float]]:
        """
        Get all identified hotspots.
        
        Returns:
            Dictionary mapping function names to their aggregated metrics
        """
        return self.m_hotspots
    
    def getTopHotspots(self, metric: str = "total_cycles", top_n: int = 10) -> List[Tuple[str, Dict[str, float]]]:
        """
        Get the top N hotspots sorted by a specific metric.
        
        Args:
            metric: Metric to sort by (default: "total_cycles")
            top_n: Number of top hotspots to return
            
        Returns:
            List of tuples (function_name, metrics_dict) sorted by metric value
        """
        # Filter hotspots that have the specified metric
        filtered_hotspots = [
            (func_name, metrics)
            for func_name, metrics in self.m_hotspots.items()
            if metric in metrics and metrics[metric] >= self.m_threshold
        ]
        
        # Sort by metric value in descending order
        sorted_hotspots = sorted(
            filtered_hotspots,
            key=lambda x: x[1][metric],
            reverse=True
        )
        
        return sorted_hotspots[:top_n]
    
    def getHotspotsByThreshold(self, metric: str = "total_cycles", threshold: Optional[float] = None) -> List[Tuple[str, Dict[str, float]]]:
        """
        Get hotspots that exceed a threshold for a specific metric.
        
        Args:
            metric: Metric to filter by (default: "total_cycles")
            threshold: Threshold value (uses instance threshold if not specified)
            
        Returns:
            List of tuples (function_name, metrics_dict) that exceed threshold
        """
        if threshold is None:
            threshold = self.m_threshold
        
        # Filter hotspots exceeding threshold
        filtered_hotspots = [
            (func_name, metrics)
            for func_name, metrics in self.m_hotspots.items()
            if metric in metrics and metrics[metric] >= threshold
        ]
        
        # Sort by metric value in descending order
        sorted_hotspots = sorted(
            filtered_hotspots,
            key=lambda x: x[1][metric],
            reverse=True
        )
        
        return sorted_hotspots
    
    def getTotalSamples(self) -> int:
        """
        Get the total number of samples processed.
        
        Returns:
            Total sample count across all functions
        """
        return sum(metrics["sample_count"] for metrics in self.m_hotspots.values())
    
    def getHotspotPercentage(self, function_name: str) -> float:
        """
        Calculate the percentage of samples for a specific function.
        
        Args:
            function_name: Name of the function
            
        Returns:
            Percentage of samples (0.0 to 100.0), or 0.0 if function not found
        """
        if function_name not in self.m_hotspots:
            return 0.0
        
        total_samples = self.getTotalSamples()
        if total_samples == 0:
            return 0.0
        
        func_samples = self.m_hotspots[function_name]["sample_count"]
        return (func_samples / total_samples) * 100.0
    
    def clear(self) -> None:
        """Clear all hotspot analysis results."""
        self.m_hotspots.clear()
    
    def run(self) -> None:
        """
        Execute hotspot analysis.
        
        Processes input profiles and identifies performance hotspots.
        Results are stored in m_hotspots.
        """
        # Clear previous results
        self.clear()
        
        # Process profiles from input data
        for data in self.m_inputs.get_data():
            if isinstance(data, PerfData):
                self.setProfile(data)
                self.analyze()
                
                # Add analysis results to outputs
                output = (
                    "HotspotAnalysis",
                    data,
                    dict(self.m_hotspots),  # Copy of hotspots
                    self.getTotalSamples()
                )
                self.m_outputs.add_data(output)
