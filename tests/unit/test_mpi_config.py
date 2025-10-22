"""
Unit tests for MPIConfig class
"""
import pytest
from perflow.utils.mpi_config import MPIConfig


class TestMPIConfig:
    """Test MPIConfig class"""
    
    def test_mpiconfig_singleton(self):
        """Test that MPIConfig is a singleton"""
        config1 = MPIConfig.get_instance()
        config2 = MPIConfig.get_instance()
        assert config1 is config2
    
    def test_mpiconfig_initial_state(self):
        """Test initial state of MPIConfig"""
        # Create a fresh instance for testing
        config = MPIConfig()
        assert config.is_enabled() is False
        assert config.get_rank() is None
        assert config.get_size() is None
        assert config.get_comm() is None
    
    def test_mpiconfig_is_mpi_available(self):
        """Test checking if MPI is available"""
        config = MPIConfig()
        # Just check that the method works, result depends on environment
        result = config.is_mpi_available()
        assert isinstance(result, bool)
    
    def test_mpiconfig_disable_mpi(self):
        """Test disabling MPI"""
        config = MPIConfig()
        config.disable_mpi()
        
        assert config.is_enabled() is False
        assert config.get_rank() is None
        assert config.get_size() is None
        assert config.get_comm() is None
    
    def test_mpiconfig_is_root_without_mpi(self):
        """Test is_root returns True when MPI is not enabled"""
        config = MPIConfig()
        config.disable_mpi()
        assert config.is_root() is True
    
    def test_mpiconfig_barrier_without_mpi(self):
        """Test that barrier works without MPI (no-op)"""
        config = MPIConfig()
        config.disable_mpi()
        # Should not raise an exception
        config.barrier()
    
    def test_mpiconfig_enable_mpi_without_mpi4py(self):
        """Test enable_mpi when mpi4py is not available"""
        config = MPIConfig()
        # If mpi4py is not available, enable_mpi should return False
        if not config.is_mpi_available():
            result = config.enable_mpi()
            assert result is False
            assert config.is_enabled() is False
