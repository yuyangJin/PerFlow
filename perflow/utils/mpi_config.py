'''
module mpi_config

MPI configuration and utilities for parallel trace analysis.
This module provides configuration management and utilities for enabling
MPI-based parallel trace replay and analysis.
'''

from typing import Optional
import sys


class MPIConfig:
    """
    MPIConfig manages MPI configuration for parallel trace analysis.
    
    This class provides a singleton configuration object that tracks whether
    MPI mode is enabled and provides access to MPI communicator information.
    It handles both MPI-enabled and non-MPI environments gracefully.
    
    Attributes:
        m_enabled: Whether MPI mode is enabled
        m_rank: MPI rank of current process (None if MPI not enabled)
        m_size: Total number of MPI processes (None if MPI not enabled)
        m_comm: MPI communicator object (None if MPI not enabled)
    """
    
    _instance: Optional['MPIConfig'] = None
    
    def __init__(self) -> None:
        """
        Initialize MPIConfig.
        
        Note: Use get_instance() instead of creating instances directly.
        """
        self.m_enabled: bool = False
        self.m_rank: Optional[int] = None
        self.m_size: Optional[int] = None
        self.m_comm: Optional[any] = None
        self._mpi4py_available: bool = False
        
        # Check if mpi4py is available
        try:
            import mpi4py
            self._mpi4py_available = True
        except ImportError:
            self._mpi4py_available = False
    
    @classmethod
    def get_instance(cls) -> 'MPIConfig':
        """
        Get the singleton MPIConfig instance.
        
        Returns:
            The singleton MPIConfig instance
        """
        if cls._instance is None:
            cls._instance = MPIConfig()
        return cls._instance
    
    def is_mpi_available(self) -> bool:
        """
        Check if MPI (mpi4py) is available.
        
        Returns:
            True if mpi4py is installed, False otherwise
        """
        return self._mpi4py_available
    
    def enable_mpi(self) -> bool:
        """
        Enable MPI mode and initialize MPI communicator.
        
        This method attempts to initialize MPI using mpi4py. If mpi4py is not
        available or initialization fails, MPI mode remains disabled.
        
        Returns:
            True if MPI was successfully enabled, False otherwise
        """
        if not self._mpi4py_available:
            print("Warning: mpi4py is not available. MPI mode cannot be enabled.", file=sys.stderr)
            return False
        
        try:
            from mpi4py import MPI
            self.m_comm = MPI.COMM_WORLD
            self.m_rank = self.m_comm.Get_rank()
            self.m_size = self.m_comm.Get_size()
            self.m_enabled = True
            return True
        except Exception as e:
            print(f"Warning: Failed to initialize MPI: {e}", file=sys.stderr)
            self.m_enabled = False
            return False
    
    def disable_mpi(self) -> None:
        """
        Disable MPI mode.
        
        This resets all MPI-related state but does not finalize MPI.
        """
        self.m_enabled = False
        self.m_rank = None
        self.m_size = None
        self.m_comm = None
    
    def is_enabled(self) -> bool:
        """
        Check if MPI mode is enabled.
        
        Returns:
            True if MPI mode is enabled, False otherwise
        """
        return self.m_enabled
    
    def get_rank(self) -> Optional[int]:
        """
        Get the MPI rank of the current process.
        
        Returns:
            MPI rank, or None if MPI is not enabled
        """
        return self.m_rank
    
    def get_size(self) -> Optional[int]:
        """
        Get the total number of MPI processes.
        
        Returns:
            Number of MPI processes, or None if MPI is not enabled
        """
        return self.m_size
    
    def get_comm(self) -> Optional[any]:
        """
        Get the MPI communicator.
        
        Returns:
            MPI communicator object, or None if MPI is not enabled
        """
        return self.m_comm
    
    def is_root(self) -> bool:
        """
        Check if this is the root process (rank 0).
        
        Returns:
            True if this is rank 0 or MPI is not enabled, False otherwise
        """
        if not self.m_enabled:
            return True
        return self.m_rank == 0
    
    def barrier(self) -> None:
        """
        MPI barrier synchronization.
        
        If MPI is enabled, performs a barrier. Otherwise, does nothing.
        """
        if self.m_enabled and self.m_comm is not None:
            self.m_comm.Barrier()
