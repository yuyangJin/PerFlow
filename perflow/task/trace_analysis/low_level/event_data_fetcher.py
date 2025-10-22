'''
module event_data_fetcher

Inter-process event data fetch API for parallel trace analysis.
This module provides functionality to fetch event data across MPI processes,
supporting both intra-process memory access and inter-process MPI communication.
'''

from typing import Optional, Dict, Any, List
from ....perf_data_struct.dynamic.trace.event import Event
from ....perf_data_struct.dynamic.trace.trace import Trace
from ....utils.mpi_config import MPIConfig


class EventDataFetcher:
    """
    EventDataFetcher provides API for fetching event data across processes.
    
    This class enables trace analysis tasks to access event data regardless of
    whether the event is in the current process (intra-process memory access)
    or in another process (inter-process MPI communication). It includes caching
    to minimize communication overhead.
    
    Attributes:
        m_local_traces: Dictionary of traces in current process (ep_id -> trace)
        m_ep_to_rp_mapping: Mapping from execution process ID to replay process ID
        m_event_cache: Cache for remote event data (cache_key -> event_data)
    """
    
    def __init__(self) -> None:
        """Initialize EventDataFetcher."""
        self.m_local_traces: Dict[int, Trace] = {}
        self.m_ep_to_rp_mapping: Dict[int, int] = {}
        self.m_event_cache: Dict[str, Dict[str, Any]] = {}
    
    def register_traces(self, traces: List[Trace], ep_to_rp_mapping: Dict[int, int]) -> None:
        """
        Register local traces for event data access.
        
        Args:
            traces: List of traces in the current process
            ep_to_rp_mapping: Mapping from execution process IDs to replay process IDs
        """
        self.m_local_traces = {}
        for trace in traces:
            trace_info = trace.getTraceInfo()
            ep_id = trace_info.getPid()
            if ep_id is not None:
                self.m_local_traces[ep_id] = trace
        
        self.m_ep_to_rp_mapping = ep_to_rp_mapping
    
    def is_local_event(self, ep_id: int) -> bool:
        """
        Check if events from an execution process are local to current process.
        
        Args:
            ep_id: Execution process ID
            
        Returns:
            True if events from this EP are in the current process, False otherwise
        """
        return ep_id in self.m_local_traces
    
    def get_local_event(self, ep_id: int, event_idx: int) -> Optional[Event]:
        """
        Get event from local traces (intra-process memory access).
        
        Args:
            ep_id: Execution process ID
            event_idx: Index of the event in the trace
            
        Returns:
            Event object if found locally, None otherwise
        """
        if ep_id not in self.m_local_traces:
            return None
        
        trace = self.m_local_traces[ep_id]
        events = trace.getEvents()
        
        # Search for event by index
        for event in events:
            if event.getIdx() == event_idx:
                return event
        
        return None
    
    def _get_event_data_dict(self, event: Event) -> Dict[str, Any]:
        """
        Convert event to a dictionary for serialization.
        
        Args:
            event: Event object to convert
            
        Returns:
            Dictionary containing event data
        """
        return {
            'type': event.getType().value if event.getType() else None,
            'idx': event.getIdx(),
            'name': event.getName(),
            'pid': event.getPid(),
            'tid': event.getTid(),
            'timestamp': event.getTimestamp(),
            'replay_pid': event.getReplayPid()
        }
    
    def _create_cache_key(self, ep_id: int, event_idx: int) -> str:
        """
        Create a cache key for an event.
        
        Args:
            ep_id: Execution process ID
            event_idx: Event index
            
        Returns:
            Cache key string
        """
        return f"{ep_id}:{event_idx}"
    
    def fetch_event_data(self, ep_id: int, event_idx: int) -> Optional[Dict[str, Any]]:
        """
        Fetch event data from local or remote process.
        
        This is the main API for fetching event data. It automatically handles:
        - Intra-process memory access for local events
        - Inter-process MPI communication for remote events
        - Caching to minimize communication overhead
        
        Args:
            ep_id: Execution process ID where the event occurred
            event_idx: Index of the event in the trace
            
        Returns:
            Dictionary containing event data, or None if not found
        """
        # Check cache first
        cache_key = self._create_cache_key(ep_id, event_idx)
        if cache_key in self.m_event_cache:
            return self.m_event_cache[cache_key]
        
        # Check if event is local
        if self.is_local_event(ep_id):
            event = self.get_local_event(ep_id, event_idx)
            if event is not None:
                event_data = self._get_event_data_dict(event)
                # Cache the result
                self.m_event_cache[cache_key] = event_data
                return event_data
            return None
        
        # Event is remote - need MPI communication
        return self._fetch_remote_event_data(ep_id, event_idx)
    
    def _fetch_remote_event_data(self, ep_id: int, event_idx: int) -> Optional[Dict[str, Any]]:
        """
        Fetch event data from a remote process via MPI.
        
        Args:
            ep_id: Execution process ID where the event occurred
            event_idx: Event index
            
        Returns:
            Dictionary containing event data, or None if not found
        """
        mpi_config = MPIConfig.get_instance()
        
        # If MPI is not enabled, we can't fetch remote data
        if not mpi_config.is_enabled():
            return None
        
        comm = mpi_config.get_comm()
        if comm is None:
            return None
        
        # Get the target replay process
        target_rp = self.m_ep_to_rp_mapping.get(ep_id)
        if target_rp is None:
            return None
        
        current_rank = mpi_config.get_rank()
        
        # Request format: ('fetch', ep_id, event_idx)
        request = ('fetch', ep_id, event_idx)
        
        try:
            # Send request to target process
            comm.send(request, dest=target_rp, tag=1)
            
            # Receive response
            response = comm.recv(source=target_rp, tag=2)
            
            if response is not None and isinstance(response, dict):
                # Cache the result
                cache_key = self._create_cache_key(ep_id, event_idx)
                self.m_event_cache[cache_key] = response
                return response
            
            return None
        except Exception:
            return None
    
    def process_fetch_requests(self) -> None:
        """
        Process incoming fetch requests from other processes.
        
        This method should be called periodically (or in a separate thread)
        to handle incoming requests for event data from other MPI processes.
        
        Note: This is a simplified implementation. A production version would
        need non-blocking communication and proper request queue management.
        """
        mpi_config = MPIConfig.get_instance()
        
        if not mpi_config.is_enabled():
            return
        
        comm = mpi_config.get_comm()
        if comm is None:
            return
        
        # Check for incoming messages (non-blocking)
        status = comm.iprobe(source=comm.ANY_SOURCE, tag=1)
        
        if status:
            # Receive the request
            request = comm.recv(source=status.Get_source(), tag=1)
            
            if isinstance(request, tuple) and len(request) == 3:
                cmd, ep_id, event_idx = request
                
                if cmd == 'fetch':
                    # Get the event data
                    event = self.get_local_event(ep_id, event_idx)
                    
                    if event is not None:
                        response = self._get_event_data_dict(event)
                    else:
                        response = None
                    
                    # Send response back
                    comm.send(response, dest=status.Get_source(), tag=2)
    
    def clear_cache(self) -> None:
        """Clear the event data cache."""
        self.m_event_cache.clear()
    
    def get_cache_size(self) -> int:
        """
        Get the number of cached event data entries.
        
        Returns:
            Number of entries in the cache
        """
        return len(self.m_event_cache)
