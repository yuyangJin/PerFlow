'''
module mpi_trace_replayer

MPI-enabled trace replayer for parallel trace analysis.
This module extends TraceReplayer to support distributed trace replay
across multiple MPI processes.
'''

from typing import List, Optional
from .trace_replayer import TraceReplayer, ReplayDirection
from .mpi_config import MPIConfig
from .trace_distributor import TraceDistributor
from .event_data_fetcher import EventDataFetcher
from ....perf_data_struct.dynamic.trace.trace import Trace


class MPITraceReplayer(TraceReplayer):
    """
    MPITraceReplayer extends TraceReplayer for parallel trace replay.
    
    This class enables distributed trace analysis by replaying traces
    across multiple MPI processes. Each process handles a subset of the
    execution process traces, and inter-process communication is used
    to access remote event data when needed.
    
    Attributes:
        m_mpi_enabled: Whether MPI mode is enabled for this replayer
        m_distributor: TraceDistributor for managing trace distribution
        m_event_fetcher: EventDataFetcher for accessing event data
        m_traces: List of traces assigned to this replay process
    """
    
    def __init__(
        self,
        trace: Optional[Trace] = None,
        enable_mpi: bool = False
    ) -> None:
        """
        Initialize MPITraceReplayer.
        
        Args:
            trace: Optional single trace to replay
            enable_mpi: Whether to enable MPI mode
        """
        super().__init__(trace)
        
        self.m_mpi_enabled: bool = False
        self.m_distributor: TraceDistributor = TraceDistributor()
        self.m_event_fetcher: EventDataFetcher = EventDataFetcher()
        self.m_traces: List[Trace] = []
        
        if enable_mpi:
            self.enable_mpi()
    
    def enable_mpi(self) -> bool:
        """
        Enable MPI mode for this replayer.
        
        Returns:
            True if MPI was successfully enabled, False otherwise
        """
        mpi_config = MPIConfig.get_instance()
        if mpi_config.enable_mpi():
            self.m_mpi_enabled = True
            return True
        else:
            self.m_mpi_enabled = False
            return False
    
    def disable_mpi(self) -> None:
        """Disable MPI mode for this replayer."""
        self.m_mpi_enabled = False
    
    def is_mpi_enabled(self) -> bool:
        """
        Check if MPI mode is enabled.
        
        Returns:
            True if MPI mode is enabled, False otherwise
        """
        return self.m_mpi_enabled
    
    def set_traces(self, traces: List[Trace]) -> None:
        """
        Set the traces to be replayed.
        
        Args:
            traces: List of all traces (from all execution processes)
        """
        self.m_traces = traces
    
    def get_traces(self) -> List[Trace]:
        """
        Get the traces assigned to this replay process.
        
        Returns:
            List of traces
        """
        return self.m_traces
    
    def distribute_traces(
        self,
        traces: List[Trace],
        num_execution_processes: int,
        num_replay_processes: Optional[int] = None
    ) -> List[Trace]:
        """
        Distribute traces to the current MPI process.
        
        This method computes the trace distribution and filters traces
        to only those assigned to the current replay process.
        
        Args:
            traces: List of all traces (one per execution process)
            num_execution_processes: Total number of execution processes
            num_replay_processes: Number of replay processes (defaults to MPI size)
            
        Returns:
            List of traces assigned to this replay process
        """
        mpi_config = MPIConfig.get_instance()
        
        # If MPI is not enabled, return all traces
        if not self.m_mpi_enabled or not mpi_config.is_enabled():
            self.m_traces = traces
            return traces
        
        # Default to MPI size if not specified
        if num_replay_processes is None:
            num_replay_processes = mpi_config.get_size()
            if num_replay_processes is None:
                num_replay_processes = 1
        
        # Configure distributor
        self.m_distributor.set_num_execution_processes(num_execution_processes)
        self.m_distributor.set_num_replay_processes(num_replay_processes)
        
        # Compute distribution
        ep_to_rp_mapping = self.m_distributor.compute_distribution()
        
        # Update trace info
        self.m_distributor.update_trace_info(traces)
        
        # Distribute traces
        self.m_traces = self.m_distributor.distribute_traces(traces)
        
        # Register traces with event fetcher
        self.m_event_fetcher.register_traces(self.m_traces, ep_to_rp_mapping)
        
        return self.m_traces
    
    def get_event_data(self, ep_id: int, event_idx: int):
        """
        Fetch event data from local or remote process.
        
        This is the main API for accessing event data during replay.
        It uses the EventDataFetcher to handle both local and remote access.
        
        Args:
            ep_id: Execution process ID where the event occurred
            event_idx: Index of the event in the trace
            
        Returns:
            Event data dictionary, or None if not found
        """
        return self.m_event_fetcher.fetch_event_data(ep_id, event_idx)
    
    def forwardReplay(self) -> None:
        """
        Replay traces in forward (chronological) order.
        
        In MPI mode, this replays only the traces assigned to the current
        replay process. Each trace is replayed in timeline order.
        """
        if self.m_traces:
            # Replay distributed traces
            for trace in self.m_traces:
                self.setTrace(trace)
                super().forwardReplay()
        elif self.m_trace is not None:
            # Replay single trace (non-MPI mode)
            super().forwardReplay()
    
    def backwardReplay(self) -> None:
        """
        Replay traces in backward (reverse chronological) order.
        
        In MPI mode, this replays only the traces assigned to the current
        replay process. Each trace is replayed in reverse timeline order.
        """
        if self.m_traces:
            # Replay distributed traces
            for trace in self.m_traces:
                self.setTrace(trace)
                super().backwardReplay()
        elif self.m_trace is not None:
            # Replay single trace (non-MPI mode)
            super().backwardReplay()
    
    def barrier(self) -> None:
        """
        Synchronize all MPI processes.
        
        This is a convenience method for MPI barrier synchronization.
        If MPI is not enabled, this does nothing.
        """
        if self.m_mpi_enabled:
            mpi_config = MPIConfig.get_instance()
            mpi_config.barrier()
    
    def get_rank(self) -> Optional[int]:
        """
        Get the MPI rank of the current process.
        
        Returns:
            MPI rank, or None if MPI is not enabled
        """
        if self.m_mpi_enabled:
            mpi_config = MPIConfig.get_instance()
            return mpi_config.get_rank()
        return None
    
    def get_size(self) -> Optional[int]:
        """
        Get the total number of MPI processes.
        
        Returns:
            Number of MPI processes, or None if MPI is not enabled
        """
        if self.m_mpi_enabled:
            mpi_config = MPIConfig.get_instance()
            return mpi_config.get_size()
        return None
    
    def is_root(self) -> bool:
        """
        Check if this is the root process (rank 0).
        
        Returns:
            True if this is rank 0 or MPI is not enabled, False otherwise
        """
        if self.m_mpi_enabled:
            mpi_config = MPIConfig.get_instance()
            return mpi_config.is_root()
        return True
    
    def get_load_balance_info(self):
        """
        Get load balance information for the current distribution.
        
        Returns:
            Dictionary mapping replay process IDs to count of assigned execution processes
        """
        return self.m_distributor.get_load_balance_info()
