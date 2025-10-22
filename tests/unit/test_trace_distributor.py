"""
Unit tests for TraceDistributor class
"""
import pytest
from perflow.task.trace_analysis.low_level.trace_distributor import TraceDistributor
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo


class TestTraceDistributor:
    """Test TraceDistributor class"""
    
    def test_tracedistributor_creation(self):
        """Test creating TraceDistributor"""
        distributor = TraceDistributor()
        assert distributor.m_num_execution_processes is None
        assert distributor.m_num_replay_processes is None
        assert len(distributor.m_ep_to_rp_mapping) == 0
    
    def test_tracedistributor_creation_with_params(self):
        """Test creating TraceDistributor with parameters"""
        distributor = TraceDistributor(num_execution_processes=8, num_replay_processes=4)
        assert distributor.m_num_execution_processes == 8
        assert distributor.m_num_replay_processes == 4
    
    def test_tracedistributor_set_num_processes(self):
        """Test setting number of processes"""
        distributor = TraceDistributor()
        distributor.set_num_execution_processes(16)
        distributor.set_num_replay_processes(8)
        
        assert distributor.m_num_execution_processes == 16
        assert distributor.m_num_replay_processes == 8
    
    def test_tracedistributor_compute_distribution(self):
        """Test computing distribution mapping"""
        distributor = TraceDistributor(num_execution_processes=8, num_replay_processes=4)
        mapping = distributor.compute_distribution()
        
        # Check that all EPs are mapped
        assert len(mapping) == 8
        
        # Check round-robin distribution
        for ep_id in range(8):
            expected_rp = ep_id % 4
            assert mapping[ep_id] == expected_rp
    
    def test_tracedistributor_compute_distribution_uneven(self):
        """Test distribution with uneven division"""
        distributor = TraceDistributor(num_execution_processes=10, num_replay_processes=3)
        mapping = distributor.compute_distribution()
        
        # Check that all EPs are mapped
        assert len(mapping) == 10
        
        # EP 0, 3, 6, 9 -> RP 0 (4 EPs)
        # EP 1, 4, 7 -> RP 1 (3 EPs)
        # EP 2, 5, 8 -> RP 2 (3 EPs)
        assert mapping[0] == 0
        assert mapping[1] == 1
        assert mapping[2] == 2
        assert mapping[3] == 0
        assert mapping[9] == 0
    
    def test_tracedistributor_compute_distribution_error(self):
        """Test compute_distribution with missing parameters"""
        distributor = TraceDistributor()
        
        with pytest.raises(ValueError):
            distributor.compute_distribution()
    
    def test_tracedistributor_compute_distribution_invalid_num_rp(self):
        """Test compute_distribution with invalid number of replay processes"""
        distributor = TraceDistributor(num_execution_processes=8, num_replay_processes=0)
        
        with pytest.raises(ValueError):
            distributor.compute_distribution()
    
    def test_tracedistributor_get_mapping(self):
        """Test getting the mapping"""
        distributor = TraceDistributor(num_execution_processes=4, num_replay_processes=2)
        distributor.compute_distribution()
        
        mapping = distributor.get_mapping()
        assert len(mapping) == 4
        assert mapping[0] == 0
        assert mapping[1] == 1
    
    def test_tracedistributor_set_mapping(self):
        """Test setting a custom mapping"""
        distributor = TraceDistributor()
        custom_mapping = {0: 0, 1: 1, 2: 0, 3: 1}
        distributor.set_mapping(custom_mapping)
        
        assert distributor.get_mapping() == custom_mapping
    
    def test_tracedistributor_get_replay_process_for_ep(self):
        """Test getting replay process for an execution process"""
        distributor = TraceDistributor(num_execution_processes=8, num_replay_processes=4)
        distributor.compute_distribution()
        
        assert distributor.get_replay_process_for_ep(0) == 0
        assert distributor.get_replay_process_for_ep(1) == 1
        assert distributor.get_replay_process_for_ep(4) == 0
        assert distributor.get_replay_process_for_ep(99) is None
    
    def test_tracedistributor_get_eps_for_rp(self):
        """Test getting execution processes for a replay process"""
        distributor = TraceDistributor(num_execution_processes=8, num_replay_processes=4)
        distributor.compute_distribution()
        
        # RP 0 should have EPs 0, 4
        eps_rp0 = distributor.get_eps_for_rp(0)
        assert len(eps_rp0) == 2
        assert 0 in eps_rp0
        assert 4 in eps_rp0
        
        # RP 1 should have EPs 1, 5
        eps_rp1 = distributor.get_eps_for_rp(1)
        assert len(eps_rp1) == 2
        assert 1 in eps_rp1
        assert 5 in eps_rp1
    
    def test_tracedistributor_distribute_traces_without_mpi(self):
        """Test distributing traces without MPI enabled"""
        distributor = TraceDistributor(num_execution_processes=4, num_replay_processes=2)
        distributor.compute_distribution()
        
        # Create traces
        traces = []
        for i in range(4):
            trace = Trace()
            trace_info = TraceInfo(pid=i)
            trace.setTraceInfo(trace_info)
            traces.append(trace)
        
        # Without MPI, should return all traces
        distributed = distributor.distribute_traces(traces)
        assert len(distributed) == 4
    
    def test_tracedistributor_update_trace_info(self):
        """Test updating trace info with distribution metadata"""
        distributor = TraceDistributor(num_execution_processes=4, num_replay_processes=2)
        mapping = distributor.compute_distribution()
        
        # Create traces
        traces = []
        for i in range(4):
            trace = Trace()
            trace_info = TraceInfo(pid=i)
            trace.setTraceInfo(trace_info)
            traces.append(trace)
        
        # Update trace info
        distributor.update_trace_info(traces)
        
        # Check that metadata was added
        for trace in traces:
            trace_info = trace.getTraceInfo()
            assert trace_info.getNumExecutionProcesses() == 4
            assert trace_info.getNumReplayProcesses() == 2
            assert trace_info.getEpToRpMapping() == mapping
    
    def test_tracedistributor_get_load_balance_info(self):
        """Test getting load balance information"""
        distributor = TraceDistributor(num_execution_processes=10, num_replay_processes=3)
        distributor.compute_distribution()
        
        load_info = distributor.get_load_balance_info()
        
        # Check that all RPs are in the result
        assert len(load_info) == 3
        
        # RP 0 gets EPs 0, 3, 6, 9 (4 EPs)
        assert load_info[0] == 4
        # RP 1 gets EPs 1, 4, 7 (3 EPs)
        assert load_info[1] == 3
        # RP 2 gets EPs 2, 5, 8 (3 EPs)
        assert load_info[2] == 3
    
    def test_tracedistributor_get_load_balance_info_without_num_rp(self):
        """Test load balance info without setting num_replay_processes"""
        distributor = TraceDistributor()
        load_info = distributor.get_load_balance_info()
        
        assert len(load_info) == 0
