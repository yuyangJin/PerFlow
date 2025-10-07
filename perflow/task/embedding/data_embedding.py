'''
module data embedding
'''

from typing import List, Dict, Optional, Any
import numpy as np


'''
@class DataEmbedding
Base class for data embedding techniques
'''


class DataEmbedding:
    """
    DataEmbedding provides base functionality for embedding performance data.
    
    This class serves as a foundation for various embedding techniques that
    transform performance data (traces, profiles, program structures) into
    vector representations for analysis, visualization, or machine learning.
    
    Attributes:
        m_embedding_dim: Dimensionality of the embedding space
        m_embeddings: Dictionary mapping data IDs to embedding vectors
    """
    
    def __init__(self, embedding_dim: int = 128) -> None:
        """
        Initialize a DataEmbedding.
        
        Args:
            embedding_dim: Dimensionality of embedding vectors
        """
        self.m_embedding_dim: int = embedding_dim
        self.m_embeddings: Dict[Any, np.ndarray] = {}
    
    def embed(self, data: Any) -> np.ndarray:
        """
        Embed data into a vector representation.
        
        Args:
            data: Data to embed
            
        Returns:
            Embedding vector
        """
        raise NotImplementedError("Subclasses must implement embed()")
    
    def getEmbedding(self, data_id: Any) -> Optional[np.ndarray]:
        """
        Get the embedding for a data item.
        
        Args:
            data_id: Identifier for the data item
            
        Returns:
            Embedding vector or None if not found
        """
        return self.m_embeddings.get(data_id)
    
    def setEmbedding(self, data_id: Any, embedding: np.ndarray) -> None:
        """
        Set the embedding for a data item.
        
        Args:
            data_id: Identifier for the data item
            embedding: Embedding vector
        """
        if len(embedding) != self.m_embedding_dim:
            raise ValueError(f"Embedding dimension mismatch: expected {self.m_embedding_dim}, got {len(embedding)}")
        self.m_embeddings[data_id] = embedding
    
    def getEmbeddingDim(self) -> int:
        """Get the embedding dimensionality."""
        return self.m_embedding_dim
    
    def getAllEmbeddings(self) -> Dict[Any, np.ndarray]:
        """Get all embeddings."""
        return self.m_embeddings
    
    def clear(self) -> None:
        """Clear all embeddings."""
        self.m_embeddings.clear()


'''
@class FunctionEmbedding
Embedding for functions based on their characteristics
'''


class FunctionEmbedding(DataEmbedding):
    """
    FunctionEmbedding creates vector representations of functions.
    
    Embeds functions based on their structural and performance characteristics
    including instruction counts, call patterns, loop nesting, etc.
    """
    
    def __init__(self, embedding_dim: int = 128) -> None:
        """
        Initialize a FunctionEmbedding.
        
        Args:
            embedding_dim: Dimensionality of embedding vectors
        """
        super().__init__(embedding_dim)
    
    def embedFromFeatures(self, function_id: str, features: Dict[str, float]) -> np.ndarray:
        """
        Embed a function from its feature vector.
        
        Args:
            function_id: Identifier for the function
            features: Dictionary of feature names to values
            
        Returns:
            Embedding vector
        """
        # Create feature vector
        feature_names = [
            "instruction_count", "basic_block_count", "loop_count",
            "call_count", "branch_count", "max_loop_depth",
            "cyclomatic_complexity", "avg_execution_time",
            "total_cycles", "cache_miss_rate"
        ]
        
        feature_vector = []
        for name in feature_names:
            feature_vector.append(features.get(name, 0.0))
        
        # Pad or truncate to embedding dimension
        if len(feature_vector) < self.m_embedding_dim:
            feature_vector.extend([0.0] * (self.m_embedding_dim - len(feature_vector)))
        else:
            feature_vector = feature_vector[:self.m_embedding_dim]
        
        embedding = np.array(feature_vector, dtype=np.float32)
        self.setEmbedding(function_id, embedding)
        
        return embedding
    
    def embed(self, data: Dict[str, Any]) -> np.ndarray:
        """
        Embed function data.
        
        Args:
            data: Dictionary containing function_id and features
            
        Returns:
            Embedding vector
        """
        function_id = data.get("function_id", "unknown")
        features = data.get("features", {})
        return self.embedFromFeatures(function_id, features)


'''
@class TraceEmbedding
Embedding for execution traces
'''


class TraceEmbedding(DataEmbedding):
    """
    TraceEmbedding creates vector representations of execution traces.
    
    Embeds traces based on their event sequences, timing patterns,
    and communication behaviors.
    """
    
    def __init__(self, embedding_dim: int = 128) -> None:
        """
        Initialize a TraceEmbedding.
        
        Args:
            embedding_dim: Dimensionality of embedding vectors
        """
        super().__init__(embedding_dim)
    
    def embedEventSequence(self, trace_id: str, event_types: List[str],
                          event_durations: List[float]) -> np.ndarray:
        """
        Embed a trace from its event sequence.
        
        Args:
            trace_id: Identifier for the trace
            event_types: List of event type names
            event_durations: List of event durations
            
        Returns:
            Embedding vector
        """
        # Create histogram of event types
        event_histogram = {}
        for event_type in event_types:
            event_histogram[event_type] = event_histogram.get(event_type, 0) + 1
        
        # Calculate statistics
        total_duration = sum(event_durations) if event_durations else 0.0
        avg_duration = total_duration / len(event_durations) if event_durations else 0.0
        max_duration = max(event_durations) if event_durations else 0.0
        min_duration = min(event_durations) if event_durations else 0.0
        
        # Build feature vector
        features = [
            float(len(event_types)),  # Total events
            total_duration,
            avg_duration,
            max_duration,
            min_duration,
        ]
        
        # Add event type frequencies (top 10 most common)
        sorted_types = sorted(event_histogram.items(), key=lambda x: x[1], reverse=True)
        for i in range(min(10, len(sorted_types))):
            features.append(float(sorted_types[i][1]))
        
        # Pad or truncate to embedding dimension
        if len(features) < self.m_embedding_dim:
            features.extend([0.0] * (self.m_embedding_dim - len(features)))
        else:
            features = features[:self.m_embedding_dim]
        
        embedding = np.array(features, dtype=np.float32)
        self.setEmbedding(trace_id, embedding)
        
        return embedding
    
    def embed(self, data: Dict[str, Any]) -> np.ndarray:
        """
        Embed trace data.
        
        Args:
            data: Dictionary containing trace_id, event_types, and event_durations
            
        Returns:
            Embedding vector
        """
        trace_id = data.get("trace_id", "unknown")
        event_types = data.get("event_types", [])
        event_durations = data.get("event_durations", [])
        return self.embedEventSequence(trace_id, event_types, event_durations)


'''
@class ProfileEmbedding
Embedding for profiling data
'''


class ProfileEmbedding(DataEmbedding):
    """
    ProfileEmbedding creates vector representations of profiling data.
    
    Embeds profiles based on performance metrics, hotspot distributions,
    and resource utilization patterns.
    """
    
    def __init__(self, embedding_dim: int = 128) -> None:
        """
        Initialize a ProfileEmbedding.
        
        Args:
            embedding_dim: Dimensionality of embedding vectors
        """
        super().__init__(embedding_dim)
    
    def embedFromMetrics(self, profile_id: str, 
                        function_metrics: Dict[str, Dict[str, float]]) -> np.ndarray:
        """
        Embed a profile from its metrics.
        
        Args:
            profile_id: Identifier for the profile
            function_metrics: Dictionary mapping function names to their metrics
            
        Returns:
            Embedding vector
        """
        # Calculate aggregate statistics
        all_cycles = [metrics.get("cycles", 0.0) for metrics in function_metrics.values()]
        all_instructions = [metrics.get("instructions", 0.0) for metrics in function_metrics.values()]
        all_cache_misses = [metrics.get("cache_misses", 0.0) for metrics in function_metrics.values()]
        
        total_cycles = sum(all_cycles)
        total_instructions = sum(all_instructions)
        total_cache_misses = sum(all_cache_misses)
        
        # Calculate IPC and other derived metrics
        ipc = total_instructions / total_cycles if total_cycles > 0 else 0.0
        cache_miss_rate = total_cache_misses / total_instructions if total_instructions > 0 else 0.0
        
        # Build feature vector
        features = [
            float(len(function_metrics)),  # Number of functions
            total_cycles,
            total_instructions,
            total_cache_misses,
            ipc,
            cache_miss_rate,
            max(all_cycles) if all_cycles else 0.0,  # Max cycles per function
            np.std(all_cycles) if len(all_cycles) > 1 else 0.0,  # Cycle variance
        ]
        
        # Add top function contributions
        sorted_funcs = sorted(function_metrics.items(), 
                            key=lambda x: x[1].get("cycles", 0.0), 
                            reverse=True)
        for i in range(min(10, len(sorted_funcs))):
            features.append(sorted_funcs[i][1].get("cycles", 0.0))
        
        # Pad or truncate to embedding dimension
        if len(features) < self.m_embedding_dim:
            features.extend([0.0] * (self.m_embedding_dim - len(features)))
        else:
            features = features[:self.m_embedding_dim]
        
        embedding = np.array(features, dtype=np.float32)
        self.setEmbedding(profile_id, embedding)
        
        return embedding
    
    def embed(self, data: Dict[str, Any]) -> np.ndarray:
        """
        Embed profile data.
        
        Args:
            data: Dictionary containing profile_id and function_metrics
            
        Returns:
            Embedding vector
        """
        profile_id = data.get("profile_id", "unknown")
        function_metrics = data.get("function_metrics", {})
        return self.embedFromMetrics(profile_id, function_metrics)
