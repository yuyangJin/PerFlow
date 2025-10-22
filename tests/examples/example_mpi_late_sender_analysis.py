"""
Example: MPI-based Late Sender Analysis with 8 Replay Processes

This example demonstrates how to use PerFlow's MPI-based parallel trace analysis
to detect late sender patterns in large-scale MPI communication traces. The traces
from multiple execution processes are distributed across 8 replay processes for
efficient parallel analysis.

Usage:
    # Without MPI (sequential mode):
    python example_mpi_late_sender_analysis.py
    
    # With MPI (8 replay processes):
    mpirun -np 8 python example_mpi_late_sender_analysis.py

Note: This example works in both modes - with MPI for actual parallel execution,
or without MPI for demonstration and testing purposes.
"""

import sys
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.mpi_event import MpiSendEvent, MpiRecvEvent
from perflow.task.trace_analysis.low_level.mpi_trace_replayer import MPITraceReplayer
from perflow.utils.mpi_config import MPIConfig
from perflow.task.trace_analysis.low_level.trace_replayer import ReplayDirection


def create_large_scale_mpi_traces(num_execution_processes=16):
    """
    Create sample MPI traces simulating a large-scale parallel application.
    
    This creates traces for multiple execution processes with various
    communication patterns, including late senders.
    
    Args:
        num_execution_processes: Number of execution processes to simulate
        
    Returns:
        List of Trace objects, one per execution process
    """
    traces = []
    
    for ep_id in range(num_execution_processes):
        trace = Trace()
        trace_info = TraceInfo(pid=ep_id)
        trace.setTraceInfo(trace_info)
        
        # Simulate communication patterns
        # Each process communicates with its neighbors
        
        # Send to next process (if not last)
        if ep_id < num_execution_processes - 1:
            dest_pid = ep_id + 1
            
            # Some sends are late (every 3rd process)
            if ep_id % 3 == 0:
                # Late sender: send at t=10.0
                send_timestamp = 10.0 + ep_id * 0.1
            else:
                # On-time sender: send at t=2.0
                send_timestamp = 2.0 + ep_id * 0.1
            
            send_event = MpiSendEvent(
                idx=ep_id * 100 + 1,
                name="MPI_Send",
                pid=ep_id,
                tid=0,
                timestamp=send_timestamp,
                replay_pid=ep_id,
                communicator=1,
                tag=ep_id,
                dest_pid=dest_pid
            )
            trace.addEvent(send_event)
        
        # Receive from previous process (if not first)
        if ep_id > 0:
            src_pid = ep_id - 1
            
            # All receives ready at t=5.0
            recv_timestamp = 5.0 + ep_id * 0.1
            
            recv_event = MpiRecvEvent(
                idx=ep_id * 100 + 2,
                name="MPI_Recv",
                pid=ep_id,
                tid=0,
                timestamp=recv_timestamp,
                replay_pid=ep_id,
                communicator=1,
                tag=src_pid,
                src_pid=src_pid
            )
            trace.addEvent(recv_event)
            
            # Match send and receive events
            # Note: In a real scenario, this matching would be done by the trace reader
            # Here we simulate it by finding the corresponding send in the previous process
            if src_pid >= 0 and src_pid < len(traces):
                prev_trace = traces[src_pid]
                for event in prev_trace.getEvents():
                    if isinstance(event, MpiSendEvent) and event.getDestPid() == ep_id:
                        event.setRecvEvent(recv_event)
                        recv_event.setSendEvent(event)
                        break
        
        traces.append(trace)
    
    return traces


class MPILateSenderAnalyzer(MPITraceReplayer):
    """
    MPI-based late sender analyzer.
    
    This class extends MPITraceReplayer to detect late senders in
    distributed trace analysis.
    """
    
    def __init__(self):
        """Initialize the analyzer."""
        super().__init__()
        self.m_late_senders = []
        self.m_wait_times = {}
        
        # Register late sender detection callback
        self.registerCallback(
            "late_sender_detector",
            self._detect_late_sender,
            ReplayDirection.FWD
        )
    
    def _detect_late_sender(self, event):
        """
        Callback to detect late senders during replay.
        
        Args:
            event: Event being processed
        """
        if isinstance(event, MpiRecvEvent):
            send_event = event.getSendEvent()
            
            if send_event and isinstance(send_event, MpiSendEvent):
                recv_time = event.getTimestamp()
                send_time = send_event.getTimestamp()
                
                if send_time and recv_time and send_time > recv_time:
                    wait_time = send_time - recv_time
                    
                    self.m_late_senders.append(send_event)
                    event_idx = event.getIdx()
                    if event_idx is not None:
                        self.m_wait_times[event_idx] = wait_time
    
    def get_late_senders(self):
        """Get list of late send events."""
        return self.m_late_senders
    
    def get_wait_times(self):
        """Get wait times dictionary."""
        return self.m_wait_times
    
    def get_total_wait_time(self):
        """Calculate total wait time."""
        return sum(self.m_wait_times.values())


def print_header(mpi_config):
    """Print analysis header with MPI information."""
    print("\n" + "=" * 80)
    print("MPI-BASED LATE SENDER ANALYSIS")
    print("=" * 80)
    
    if mpi_config.is_enabled():
        print(f"\nMPI Configuration:")
        print(f"  Total Replay Processes: {mpi_config.get_size()}")
        print(f"  Current Process Rank: {mpi_config.get_rank()}")
    else:
        print("\nRunning in Sequential Mode (MPI not enabled)")
    print()


def print_distribution_info(analyzer, num_execution_processes):
    """Print trace distribution information."""
    mpi_config = MPIConfig.get_instance()
    
    if mpi_config.is_root():
        print("\nTrace Distribution:")
        print(f"  Total Execution Processes: {num_execution_processes}")
        
        if mpi_config.is_enabled():
            load_info = analyzer.get_load_balance_info()
            print(f"  Load Balance:")
            for rp_id, ep_count in sorted(load_info.items()):
                print(f"    RP {rp_id}: {ep_count} execution processes")
        
        print()


def print_results(analyzer):
    """Print analysis results."""
    mpi_config = MPIConfig.get_instance()
    rank = mpi_config.get_rank() if mpi_config.is_enabled() else 0
    
    late_senders = analyzer.get_late_senders()
    total_wait = analyzer.get_total_wait_time()
    
    print(f"\nProcess {rank} Results:")
    print(f"  Late Senders Found: {len(late_senders)}")
    print(f"  Total Wait Time: {total_wait:.6f} seconds")
    
    if len(late_senders) > 0:
        print(f"\n  Details:")
        for i, send_event in enumerate(late_senders, 1):
            recv_event = send_event.getRecvEvent()
            if recv_event:
                send_time = send_event.getTimestamp()
                recv_time = recv_event.getTimestamp()
                wait_time = send_time - recv_time if (send_time and recv_time) else 0
                
                print(f"    Late Send #{i}:")
                print(f"      Sender PID: {send_event.getPid()}")
                print(f"      Receiver PID: {send_event.getDestPid()}")
                print(f"      Tag: {send_event.getTag()}")
                print(f"      Wait Time: {wait_time:.6f} seconds")


def gather_results(analyzer):
    """Gather results from all processes to root."""
    mpi_config = MPIConfig.get_instance()
    
    if not mpi_config.is_enabled():
        return
    
    comm = mpi_config.get_comm()
    if comm is None:
        return
    
    # Gather counts from all processes
    local_count = len(analyzer.get_late_senders())
    local_wait = analyzer.get_total_wait_time()
    
    all_counts = comm.gather(local_count, root=0)
    all_waits = comm.gather(local_wait, root=0)
    
    if mpi_config.is_root():
        print("\n" + "=" * 80)
        print("GLOBAL RESULTS (All Replay Processes)")
        print("=" * 80)
        print(f"\nTotal Late Senders Detected: {sum(all_counts)}")
        print(f"Total Wait Time (All Processes): {sum(all_waits):.6f} seconds")
        
        print(f"\nPer-Process Breakdown:")
        for rp_id, (count, wait) in enumerate(zip(all_counts, all_waits)):
            print(f"  RP {rp_id}: {count} late senders, {wait:.6f}s wait time")
        print()


def main():
    """
    Main function demonstrating MPI-based late sender analysis.
    """
    # Get MPI configuration
    mpi_config = MPIConfig.get_instance()
    
    # Try to enable MPI (will succeed if mpirun is used)
    mpi_enabled = mpi_config.enable_mpi()
    
    # Print header
    if not mpi_config.is_enabled() or mpi_config.is_root():
        print_header(mpi_config)
    
    # Configuration
    NUM_EXECUTION_PROCESSES = 16
    NUM_REPLAY_PROCESSES = 8
    
    # Create traces (only on root in MPI mode, or always in sequential mode)
    if not mpi_config.is_enabled() or mpi_config.is_root():
        print(f"Creating sample traces for {NUM_EXECUTION_PROCESSES} execution processes...")
        traces = create_large_scale_mpi_traces(NUM_EXECUTION_PROCESSES)
        print(f"  Created {len(traces)} traces")
    else:
        traces = []
    
    # Broadcast traces to all processes in MPI mode
    if mpi_config.is_enabled():
        comm = mpi_config.get_comm()
        traces = comm.bcast(traces, root=0)
    
    # Create analyzer
    analyzer = MPILateSenderAnalyzer()
    
    # Enable MPI mode for the analyzer
    if mpi_enabled:
        analyzer.m_mpi_enabled = True
    
    # Distribute traces
    if not mpi_config.is_enabled() or mpi_config.is_root():
        print(f"\nDistributing traces to {NUM_REPLAY_PROCESSES} replay processes...")
    
    distributed_traces = analyzer.distribute_traces(
        traces,
        num_execution_processes=NUM_EXECUTION_PROCESSES,
        num_replay_processes=NUM_REPLAY_PROCESSES
    )
    
    rank = mpi_config.get_rank() if mpi_config.is_enabled() else 0
    print(f"Process {rank}: Received {len(distributed_traces)} traces to analyze")
    
    # Print distribution info
    print_distribution_info(analyzer, NUM_EXECUTION_PROCESSES)
    
    # Synchronize before analysis
    analyzer.barrier()
    
    # Perform analysis
    if not mpi_config.is_enabled() or mpi_config.is_root():
        print("Performing late sender analysis...")
    
    analyzer.forwardReplay()
    
    # Synchronize after analysis
    analyzer.barrier()
    
    # Print local results
    print_results(analyzer)
    
    # Gather and print global results
    if mpi_config.is_enabled():
        gather_results(analyzer)
    
    # Final synchronization
    analyzer.barrier()
    
    if not mpi_config.is_enabled() or mpi_config.is_root():
        print("\n" + "=" * 80)
        print("Analysis Complete!")
        print("=" * 80)


if __name__ == "__main__":
    main()
