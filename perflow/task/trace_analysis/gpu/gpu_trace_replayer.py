"""
GPU Trace Replayer - Python interface for GPU-accelerated trace analysis.

This module provides a Python interface to CUDA-accelerated trace replay
and analysis functionality.
"""

import ctypes
import os
import sys
from typing import Optional, List, Dict, Callable, Any
from enum import Enum
import numpy as np

from ....perf_data_struct.dynamic.trace.trace import Trace
from ....perf_data_struct.dynamic.trace.event import Event, EventType
from ....perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from ..low_level.trace_replayer import TraceReplayer, ReplayDirection

# Check if GPU support is available
GPUAvailable = False
_gpu_lib = None

def _load_gpu_library():
    """Load the compiled CUDA library."""
    global _gpu_lib, GPUAvailable
    
    if _gpu_lib is not None:
        return _gpu_lib
    
    # Try to find the compiled library
    lib_paths = [
        os.path.join(os.path.dirname(__file__), 'libtrace_replay_gpu.so'),
        os.path.join(os.path.dirname(__file__), 'trace_replay_gpu.dll'),
        os.path.join(os.path.dirname(__file__), 'libtrace_replay_gpu.dylib'),
    ]
    
    for lib_path in lib_paths:
        if os.path.exists(lib_path):
            try:
                _gpu_lib = ctypes.CDLL(lib_path)
                GPUAvailable = True
                return _gpu_lib
            except Exception as e:
                print(f"Warning: Could not load GPU library from {lib_path}: {e}")
                continue
    
    # GPU library not found
    GPUAvailable = False
    return None


class DataDependence(Enum):
    """
    Enumeration of data dependence types for trace replay.
    
    Defines how events can be parallelized during replay based on
    their dependencies.
    """
    NO_DEPS = "no_dependencies"  # All events independent, full parallelism
    INTRA_PROCS_DEPS = "intra_process_dependencies"  # Events between processes independent
    INTER_PROCS_DEPS = "inter_process_dependencies"  # Events within process independent
    FULL_DEPS = "full_dependencies"  # Sequential replay required


class GPUEventData:
    """
    GPU-friendly representation of trace events.
    
    Converts Python event objects to Structure of Arrays (SoA) format
    for efficient GPU processing.
    """
    
    def __init__(self, trace: Optional[Trace] = None):
        """Initialize GPU event data structure."""
        self.num_events = 0
        self.types = None
        self.indices = None
        self.pids = None
        self.tids = None
        self.timestamps = None
        self.replay_pids = None
        
        # MPI-specific arrays
        self.communicators = None
        self.tags = None
        self.partner_pids = None
        self.partner_indices = None
        
        if trace is not None:
            self.from_trace(trace)
    
    def from_trace(self, trace: Trace) -> None:
        """
        Convert a Trace object to GPU-friendly arrays.
        
        Args:
            trace: Input trace to convert
        """
        events = trace.getEvents()
        self.num_events = len(events)
        
        if self.num_events == 0:
            return
        
        # Allocate arrays
        self.types = np.zeros(self.num_events, dtype=np.int32)
        self.indices = np.zeros(self.num_events, dtype=np.int32)
        self.pids = np.zeros(self.num_events, dtype=np.int32)
        self.tids = np.zeros(self.num_events, dtype=np.int32)
        self.timestamps = np.zeros(self.num_events, dtype=np.float64)
        self.replay_pids = np.zeros(self.num_events, dtype=np.int32)
        
        # MPI-specific arrays
        self.communicators = np.zeros(self.num_events, dtype=np.int32)
        self.tags = np.zeros(self.num_events, dtype=np.int32)
        self.partner_pids = np.zeros(self.num_events, dtype=np.int32)
        self.partner_indices = np.full(self.num_events, -1, dtype=np.int32)
        
        # Build index mapping
        event_to_idx = {id(event): i for i, event in enumerate(events)}
        
        # Fill arrays
        for i, event in enumerate(events):
            # Base event fields
            event_type = event.getType()
            if event_type is not None:
                self.types[i] = event_type.value
            
            idx = event.getIdx()
            if idx is not None:
                self.indices[i] = idx
            else:
                self.indices[i] = i
            
            pid = event.getPid()
            if pid is not None:
                self.pids[i] = pid
            
            tid = event.getTid()
            if tid is not None:
                self.tids[i] = tid
            
            timestamp = event.getTimestamp()
            if timestamp is not None:
                self.timestamps[i] = timestamp
            
            replay_pid = event.getReplayPid()
            if replay_pid is not None:
                self.replay_pids[i] = replay_pid
            
            # MPI-specific fields
            if isinstance(event, (MpiSendEvent, MpiRecvEvent)):
                comm = event.getCommunicator()
                if comm is not None:
                    self.communicators[i] = comm
                
                tag = event.getTag()
                if tag is not None:
                    self.tags[i] = tag
                
                if isinstance(event, MpiSendEvent):
                    dest_pid = event.getDestPid()
                    if dest_pid is not None:
                        self.partner_pids[i] = dest_pid
                    
                    recv_event = event.getRecvEvent()
                    if recv_event is not None and id(recv_event) in event_to_idx:
                        self.partner_indices[i] = event_to_idx[id(recv_event)]
                
                elif isinstance(event, MpiRecvEvent):
                    src_pid = event.getSrcPid()
                    if src_pid is not None:
                        self.partner_pids[i] = src_pid
                    
                    send_event = event.getSendEvent()
                    if send_event is not None and id(send_event) in event_to_idx:
                        self.partner_indices[i] = event_to_idx[id(send_event)]
    
    def get_event_count(self) -> int:
        """Get the number of events."""
        return self.num_events


class GPUTraceReplayer(TraceReplayer):
    """
    GPU-accelerated trace replayer.
    
    This class extends TraceReplayer to provide GPU-accelerated parallel
    trace replay and analysis. It automatically converts Python trace objects
    to GPU-friendly data structures and executes analysis kernels on the GPU.
    
    The replayer supports callback registration for custom analysis tasks,
    with automatic handling of data dependencies for optimal parallelization.
    
    Attributes:
        gpu_data: GPU-friendly event data
        use_gpu: Whether GPU acceleration is enabled
        use_shared_mem: Whether to use shared memory optimization
        gpu_callbacks: Dictionary of GPU callback functions
        callback_data_deps: Dictionary mapping callback names to their data dependencies
    """
    
    def __init__(self, trace: Optional[Trace] = None, use_gpu: bool = True):
        """
        Initialize GPU trace replayer.
        
        Args:
            trace: Optional trace to analyze
            use_gpu: Whether to enable GPU acceleration (default: True)
        """
        super().__init__(trace)
        self.gpu_data: Optional[GPUEventData] = None
        self.use_gpu = use_gpu and GPUAvailable
        self.use_shared_mem = True  # Enable shared memory optimization by default
        
        # GPU-specific callback management
        self.gpu_callbacks: Dict[str, Callable[[np.ndarray, np.ndarray, np.ndarray], Any]] = {}
        self.callback_data_deps: Dict[str, DataDependence] = {}
        
        if not self.use_gpu and use_gpu:
            print("Warning: GPU acceleration requested but not available. "
                  "Falling back to CPU implementation.")
    
    def setTrace(self, trace: Trace) -> None:
        """
        Set the trace and convert to GPU format.
        
        Args:
            trace: Trace object to replay
        """
        super().setTrace(trace)
        
        if self.use_gpu and trace is not None:
            self.gpu_data = GPUEventData(trace)
    
    def enableSharedMemory(self, enable: bool = True) -> None:
        """
        Enable or disable shared memory optimization.
        
        Args:
            enable: Whether to use shared memory
        """
        self.use_shared_mem = enable
    
    def registerGPUCallback(
        self, 
        name: str, 
        callback: Callable[[np.ndarray, np.ndarray, np.ndarray], Any],
        data_dependence: DataDependence = DataDependence.NO_DEPS
    ) -> None:
        """
        Register a GPU callback function for trace analysis.
        
        The callback function receives GPU-formatted event data and performs
        custom analysis. Data dependence information guides the parallelization
        strategy.
        
        Args:
            name: Name identifier for the callback
            callback: Function that processes GPU event arrays
                     Signature: callback(types, timestamps, partner_indices) -> results
            data_dependence: Type of data dependence for this callback
        """
        self.gpu_callbacks[name] = callback
        self.callback_data_deps[name] = data_dependence
    
    def unregisterGPUCallback(self, name: str) -> None:
        """
        Unregister a GPU callback function.
        
        Args:
            name: Name identifier of the callback to remove
        """
        if name in self.gpu_callbacks:
            del self.gpu_callbacks[name]
        if name in self.callback_data_deps:
            del self.callback_data_deps[name]
    
    def _execute_gpu_callbacks(self, direction: ReplayDirection) -> Dict[str, Any]:
        """
        Execute all registered GPU callbacks on the GPU.
        
        This method launches GPU kernels based on the registered callbacks
        and their data dependencies.
        
        Args:
            direction: Replay direction (forward or backward)
            
        Returns:
            Dictionary mapping callback names to their results
        """
        if self.gpu_data is None or self.gpu_data.num_events == 0:
            return {}
        
        results = {}
        
        for name, callback in self.gpu_callbacks.items():
            data_dep = self.callback_data_deps.get(name, DataDependence.NO_DEPS)
            
            try:
                # Execute callback on GPU arrays
                if direction == ReplayDirection.FWD:
                    # Forward replay
                    result = callback(
                        self.gpu_data.types,
                        self.gpu_data.timestamps,
                        self.gpu_data.partner_indices
                    )
                else:
                    # Backward replay - reverse arrays
                    result = callback(
                        self.gpu_data.types[::-1],
                        self.gpu_data.timestamps[::-1],
                        self.gpu_data.partner_indices[::-1]
                    )
                
                results[name] = result
                
            except Exception as e:
                print(f"Warning: GPU callback '{name}' failed: {e}. Skipping.")
                continue
        
        return results
    
    def forwardReplay(self) -> None:
        """
        Replay the trace in forward order on GPU.
        
        Executes all registered GPU callbacks in parallel based on their
        data dependencies. Falls back to CPU if GPU is unavailable.
        """
        if self.use_gpu and self.gpu_data is not None and len(self.gpu_callbacks) > 0:
            # Execute GPU callbacks
            self._execute_gpu_callbacks(ReplayDirection.FWD)
        elif len(self.m_forward_callbacks) > 0:
            # Fall back to CPU implementation for registered CPU callbacks
            super().forwardReplay()
    
    def backwardReplay(self) -> None:
        """
        Replay the trace in backward order on GPU.
        
        Executes all registered GPU callbacks in parallel based on their
        data dependencies. Falls back to CPU if GPU is unavailable.
        """
        if self.use_gpu and self.gpu_data is not None and len(self.gpu_callbacks) > 0:
            # Execute GPU callbacks
            self._execute_gpu_callbacks(ReplayDirection.BWD)
        elif len(self.m_backward_callbacks) > 0:
            # Fall back to CPU implementation for registered CPU callbacks
            super().backwardReplay()
    
    def isGPUEnabled(self) -> bool:
        """Check if GPU acceleration is enabled."""
        return self.use_gpu
    
    def getGPUData(self) -> Optional[GPUEventData]:
        """Get the GPU-friendly event data."""
        return self.gpu_data
