'''
module trace_distributor

Trace distribution functionality for parallel trace analysis.
This module provides utilities to distribute traces across MPI processes
by dividing them at the rank dimension.
'''

from typing import List, Dict, Optional
from ....perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from ....perf_data_struct.dynamic.trace.event import Event
from .mpi_config import MPIConfig


class TraceDistributor:
    """
    TraceDistributor handles distribution of traces across MPI processes.
    
    This class divides traces by execution process rank and distributes them
    evenly across replay processes. Each replay process receives a subset of
    the execution process traces.
    
    Attributes:
        m_num_execution_processes: Total number of execution processes
        m_num_replay_processes: Total number of replay processes
        m_ep_to_rp_mapping: Mapping from execution process ID to replay process ID
    """
    
    def __init__(
        self,
        num_execution_processes: Optional[int] = None,
        num_replay_processes: Optional[int] = None
    ) -> None:
        """
        Initialize TraceDistributor.
        
        Args:
            num_execution_processes: Total number of execution processes
            num_replay_processes: Total number of replay processes
        """
        self.m_num_execution_processes: Optional[int] = num_execution_processes
        self.m_num_replay_processes: Optional[int] = num_replay_processes
        self.m_ep_to_rp_mapping: Dict[int, int] = {}
    
    def set_num_execution_processes(self, num_ep: int) -> None:
        """
        Set the number of execution processes.
        
        Args:
            num_ep: Number of execution processes
        """
        self.m_num_execution_processes = num_ep
    
    def set_num_replay_processes(self, num_rp: int) -> None:
        """
        Set the number of replay processes.
        
        Args:
            num_rp: Number of replay processes
        """
        self.m_num_replay_processes = num_rp
    
    def compute_distribution(self) -> Dict[int, int]:
        """
        Compute the distribution of execution processes to replay processes.
        
        This method evenly distributes execution processes across replay processes
        using a round-robin strategy. Each execution process is assigned to a
        replay process by: rp_id = ep_id % num_replay_processes.
        
        Returns:
            Dictionary mapping execution process IDs to replay process IDs
        """
        if self.m_num_execution_processes is None or self.m_num_replay_processes is None:
            raise ValueError("Number of execution and replay processes must be set")
        
        if self.m_num_replay_processes <= 0:
            raise ValueError("Number of replay processes must be positive")
        
        self.m_ep_to_rp_mapping = {}
        for ep_id in range(self.m_num_execution_processes):
            rp_id = ep_id % self.m_num_replay_processes
            self.m_ep_to_rp_mapping[ep_id] = rp_id
        
        return self.m_ep_to_rp_mapping
    
    def get_mapping(self) -> Dict[int, int]:
        """
        Get the current EP to RP mapping.
        
        Returns:
            Dictionary mapping execution process IDs to replay process IDs
        """
        return self.m_ep_to_rp_mapping
    
    def set_mapping(self, mapping: Dict[int, int]) -> None:
        """
        Set a custom EP to RP mapping.
        
        Args:
            mapping: Dictionary mapping execution process IDs to replay process IDs
        """
        self.m_ep_to_rp_mapping = mapping
    
    def get_replay_process_for_ep(self, ep_id: int) -> Optional[int]:
        """
        Get the replay process ID for a given execution process ID.
        
        Args:
            ep_id: Execution process ID
            
        Returns:
            Replay process ID, or None if not found
        """
        return self.m_ep_to_rp_mapping.get(ep_id)
    
    def get_eps_for_rp(self, rp_id: int) -> List[int]:
        """
        Get all execution process IDs assigned to a replay process.
        
        Args:
            rp_id: Replay process ID
            
        Returns:
            List of execution process IDs assigned to this replay process
        """
        return [ep_id for ep_id, assigned_rp in self.m_ep_to_rp_mapping.items() 
                if assigned_rp == rp_id]
    
    def distribute_traces(self, traces: List[Trace]) -> List[Trace]:
        """
        Distribute traces to the current MPI process.
        
        This method filters the input traces to only include those that should
        be processed by the current MPI process based on the EP to RP mapping.
        
        Args:
            traces: List of all traces (one per execution process)
            
        Returns:
            List of traces assigned to the current replay process
        """
        mpi_config = MPIConfig.get_instance()
        
        # If MPI is not enabled, return all traces
        if not mpi_config.is_enabled():
            return traces
        
        current_rp = mpi_config.get_rank()
        if current_rp is None:
            return traces
        
        # Get EPs assigned to this RP
        assigned_eps = self.get_eps_for_rp(current_rp)
        
        # Filter traces
        distributed_traces = []
        for trace in traces:
            trace_info = trace.getTraceInfo()
            ep_id = trace_info.getPid()
            
            if ep_id is not None and ep_id in assigned_eps:
                distributed_traces.append(trace)
        
        return distributed_traces
    
    def update_trace_info(self, traces: List[Trace]) -> None:
        """
        Update TraceInfo in traces with distribution metadata.
        
        This method updates each trace's TraceInfo to include the total number
        of execution/replay processes and the EP to RP mapping.
        
        Args:
            traces: List of traces to update
        """
        for trace in traces:
            trace_info = trace.getTraceInfo()
            trace_info.setNumExecutionProcesses(self.m_num_execution_processes)
            trace_info.setNumReplayProcesses(self.m_num_replay_processes)
            trace_info.setEpToRpMapping(self.m_ep_to_rp_mapping)
    
    def get_load_balance_info(self) -> Dict[int, int]:
        """
        Get load balance information showing how many EPs are assigned to each RP.
        
        Returns:
            Dictionary mapping replay process IDs to count of assigned execution processes
        """
        if self.m_num_replay_processes is None:
            return {}
        
        load_info = {rp_id: 0 for rp_id in range(self.m_num_replay_processes)}
        for ep_id, rp_id in self.m_ep_to_rp_mapping.items():
            if rp_id in load_info:
                load_info[rp_id] += 1
        
        return load_info
