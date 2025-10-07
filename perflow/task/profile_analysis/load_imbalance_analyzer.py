'''
module load imbalance analysis
'''

from typing import Dict, List, Optional
from ..profile_analysis.profile_analyzer import ProfileAnalyzer
from ...perf_data_struct.dynamic.profile.perf_data import PerfData
from ...perf_data_struct.dynamic.profile.sample_data import SampleData


'''
@class LoadImbalanceAnalyzer
Analysis pass for detecting load imbalance in parallel applications
'''


class LoadImbalanceAnalyzer(ProfileAnalyzer):
    """
    LoadImbalanceAnalyzer detects and quantifies load imbalance.
    
    Load imbalance occurs when work is not evenly distributed across processes
    or threads, leading to idle time and reduced efficiency.
    
    Attributes:
        m_process_metrics: Dictionary mapping process IDs to metrics
        m_imbalance_score: Overall imbalance score (0=balanced, 1=highly imbalanced)
    """
    
    def __init__(self, profile: Optional[PerfData] = None) -> None:
        """
        Initialize a LoadImbalanceAnalyzer.
        
        Args:
            profile: Optional profile data to analyze
        """
        super().__init__(profile)
        self.m_process_metrics: Dict[int, Dict[str, float]] = {}
        self.m_imbalance_score: float = 0.0
        
        # Register callback for collecting per-process metrics
        self.registerCallback("collect_process_metrics", self._collect_metrics)
    
    def _collect_metrics(self, sample: SampleData) -> None:
        """
        Callback to collect per-process metrics.
        
        Args:
            sample: Sample being processed
        """
        pid = sample.getPid()
        if pid is not None:
            if pid not in self.m_process_metrics:
                self.m_process_metrics[pid] = {
                    "sample_count": 0,
                    "total_cycles": 0.0,
                    "total_instructions": 0.0
                }
            
            self.m_process_metrics[pid]["sample_count"] += 1
            
            cycles = sample.getMetric("cycles")
            if cycles is not None:
                self.m_process_metrics[pid]["total_cycles"] += cycles
            
            instructions = sample.getMetric("instructions")
            if instructions is not None:
                self.m_process_metrics[pid]["total_instructions"] += instructions
    
    def calculateImbalance(self) -> float:
        """
        Calculate load imbalance score.
        
        Returns:
            Imbalance score between 0 (perfectly balanced) and 1 (highly imbalanced)
        """
        if not self.m_process_metrics:
            return 0.0
        
        # Get sample counts per process
        sample_counts = [metrics["sample_count"] for metrics in self.m_process_metrics.values()]
        
        if not sample_counts:
            return 0.0
        
        # Calculate coefficient of variation
        import math
        mean_samples = sum(sample_counts) / len(sample_counts)
        if mean_samples == 0:
            return 0.0
        
        variance = sum((x - mean_samples) ** 2 for x in sample_counts) / len(sample_counts)
        std_dev = math.sqrt(variance)
        
        # Coefficient of variation as imbalance score
        self.m_imbalance_score = std_dev / mean_samples if mean_samples > 0 else 0.0
        
        return self.m_imbalance_score
    
    def getProcessMetrics(self) -> Dict[int, Dict[str, float]]:
        """Get metrics for each process."""
        return self.m_process_metrics
    
    def getImbalanceScore(self) -> float:
        """Get the imbalance score."""
        return self.m_imbalance_score
    
    def getMaxLoadProcess(self) -> Optional[int]:
        """
        Get the process with maximum load.
        
        Returns:
            Process ID with maximum load
        """
        if not self.m_process_metrics:
            return None
        
        max_pid = max(self.m_process_metrics.keys(),
                     key=lambda pid: self.m_process_metrics[pid]["sample_count"])
        return max_pid
    
    def getMinLoadProcess(self) -> Optional[int]:
        """
        Get the process with minimum load.
        
        Returns:
            Process ID with minimum load
        """
        if not self.m_process_metrics:
            return None
        
        min_pid = min(self.m_process_metrics.keys(),
                     key=lambda pid: self.m_process_metrics[pid]["sample_count"])
        return min_pid
    
    def getLoadRatio(self) -> float:
        """
        Get the ratio of maximum to minimum load.
        
        Returns:
            Load ratio (higher values indicate more imbalance)
        """
        max_pid = self.getMaxLoadProcess()
        min_pid = self.getMinLoadProcess()
        
        if max_pid is None or min_pid is None:
            return 1.0
        
        max_load = self.m_process_metrics[max_pid]["sample_count"]
        min_load = self.m_process_metrics[min_pid]["sample_count"]
        
        return max_load / min_load if min_load > 0 else float('inf')
    
    def clear(self) -> None:
        """Clear analysis results."""
        self.m_process_metrics.clear()
        self.m_imbalance_score = 0.0
    
    def run(self) -> None:
        """Execute load imbalance analysis."""
        self.clear()
        
        for data in self.m_inputs.get_data():
            if isinstance(data, PerfData):
                self.setProfile(data)
                self.analyze()
                self.calculateImbalance()
                
                # Add results to outputs
                output = (
                    "LoadImbalanceAnalysis",
                    data,
                    dict(self.m_process_metrics),
                    self.m_imbalance_score
                )
                self.m_outputs.add_data(output)
