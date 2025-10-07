'''
module cache analysis
'''

from typing import Dict, List, Optional
from ..profile_analysis.profile_analyzer import ProfileAnalyzer
from ...perf_data_struct.dynamic.profile.perf_data import PerfData
from ...perf_data_struct.dynamic.profile.sample_data import SampleData


'''
@class CacheAnalyzer
Analysis pass for cache behavior analysis
'''


class CacheAnalyzer(ProfileAnalyzer):
    """
    CacheAnalyzer analyzes cache behavior and identifies cache-related bottlenecks.
    
    Examines cache miss rates, cache efficiency, and identifies functions
    with poor cache behavior.
    
    Attributes:
        m_cache_stats: Dictionary of cache statistics
        m_function_cache_stats: Per-function cache statistics
    """
    
    def __init__(self, profile: Optional[PerfData] = None) -> None:
        """
        Initialize a CacheAnalyzer.
        
        Args:
            profile: Optional profile data to analyze
        """
        super().__init__(profile)
        self.m_cache_stats: Dict[str, float] = {}
        self.m_function_cache_stats: Dict[str, Dict[str, float]] = {}
        
        # Register callback
        self.registerCallback("collect_cache_stats", self._collect_stats)
    
    def _collect_stats(self, sample: SampleData) -> None:
        """Collect cache statistics from samples."""
        func_name = sample.getFunctionName()
        if not func_name:
            return
        
        if func_name not in self.m_function_cache_stats:
            self.m_function_cache_stats[func_name] = {
                "sample_count": 0,
                "total_cache_misses": 0.0,
                "total_cache_references": 0.0,
                "total_instructions": 0.0
            }
        
        stats = self.m_function_cache_stats[func_name]
        stats["sample_count"] += 1
        
        cache_misses = sample.getMetric("cache_misses")
        if cache_misses is not None:
            stats["total_cache_misses"] += cache_misses
        
        cache_refs = sample.getMetric("cache_references")
        if cache_refs is not None:
            stats["total_cache_references"] += cache_refs
        
        instructions = sample.getMetric("instructions")
        if instructions is not None:
            stats["total_instructions"] += instructions
    
    def calculateCacheMetrics(self) -> None:
        """Calculate overall cache metrics."""
        total_misses = 0.0
        total_refs = 0.0
        total_instructions = 0.0
        
        for stats in self.m_function_cache_stats.values():
            total_misses += stats["total_cache_misses"]
            total_refs += stats["total_cache_references"]
            total_instructions += stats["total_instructions"]
        
        # Calculate miss rate
        miss_rate = total_misses / total_refs if total_refs > 0 else 0.0
        
        # Calculate misses per kilo-instruction (MPKI)
        mpki = (total_misses / total_instructions) * 1000 if total_instructions > 0 else 0.0
        
        self.m_cache_stats = {
            "total_cache_misses": total_misses,
            "total_cache_references": total_refs,
            "miss_rate": miss_rate,
            "mpki": mpki
        }
    
    def getCacheStats(self) -> Dict[str, float]:
        """Get overall cache statistics."""
        return self.m_cache_stats
    
    def getFunctionCacheStats(self) -> Dict[str, Dict[str, float]]:
        """Get per-function cache statistics."""
        return self.m_function_cache_stats
    
    def getWorstCacheFunctions(self, top_n: int = 10) -> List[tuple]:
        """
        Get functions with worst cache behavior.
        
        Args:
            top_n: Number of top functions to return
            
        Returns:
            List of (function_name, miss_rate) tuples
        """
        func_miss_rates = []
        
        for func_name, stats in self.m_function_cache_stats.items():
            total_refs = stats["total_cache_references"]
            if total_refs > 0:
                miss_rate = stats["total_cache_misses"] / total_refs
                func_miss_rates.append((func_name, miss_rate))
        
        func_miss_rates.sort(key=lambda x: x[1], reverse=True)
        return func_miss_rates[:top_n]
    
    def clear(self) -> None:
        """Clear analysis results."""
        self.m_cache_stats.clear()
        self.m_function_cache_stats.clear()
    
    def run(self) -> None:
        """Execute cache analysis."""
        self.clear()
        
        for data in self.m_inputs.get_data():
            if isinstance(data, PerfData):
                self.setProfile(data)
                self.analyze()
                self.calculateCacheMetrics()
                
                output = (
                    "CacheAnalysis",
                    data,
                    dict(self.m_cache_stats),
                    dict(self.m_function_cache_stats)
                )
                self.m_outputs.add_data(output)
