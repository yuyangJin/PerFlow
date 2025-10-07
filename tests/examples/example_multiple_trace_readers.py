"""
Example: Using Multiple TraceReader Classes

This example demonstrates how to use different TraceReader classes
to read traces from various profiling tools and formats.
"""

from perflow.task.reader.otf2_reader import OTF2Reader
from perflow.task.reader.vtune_reader import VtuneReader
from perflow.task.reader.nsight_reader import NsightReader
from perflow.task.reader.hpctoolkit_reader import HpctoolkitReader
from perflow.task.reader.ctf_reader import CTFReader
from perflow.task.reader.scalatrace_reader import ScalatraceReader
from perflow.task.reporter.timeline_viewer import TimelineViewer
from perflow.flow.flow import FlowGraph


def demonstrate_reader(reader_class, reader_name, file_path):
    """
    Demonstrate using a specific trace reader.
    
    Args:
        reader_class: The reader class to instantiate
        reader_name: Human-readable name of the reader
        file_path: Path to the trace file
    """
    print(f"\n{reader_name}:")
    print("-" * 60)
    
    # Create reader
    reader = reader_class(file_path)
    print(f"  Created reader for: {reader.getFilePath()}")
    
    # Load trace
    trace = reader.load()
    print(f"  Loaded trace: {type(trace).__name__}")
    print(f"  Event count: {trace.getEventCount()}")
    
    # Show reader is a FlowNode
    print(f"  Can be used in workflows: {hasattr(reader, 'run')}")
    
    return reader, trace


def demonstrate_workflow_integration():
    """
    Demonstrate integrating different readers in a workflow.
    """
    print("\n" + "=" * 60)
    print("WORKFLOW INTEGRATION EXAMPLE")
    print("=" * 60)
    
    # Create a workflow
    workflow = FlowGraph()
    
    # Create readers for different formats
    otf2_reader = OTF2Reader("/traces/mpi_app.otf2")
    vtune_reader = VtuneReader("/traces/cpu_profile.vtune")
    
    # Create visualization node
    viewer = TimelineViewer("text")
    
    # Build workflow
    workflow.add_edge(otf2_reader, viewer)
    workflow.add_edge(vtune_reader, viewer)
    
    print("\nWorkflow created with:")
    print(f"  - {len(workflow.get_nodes())} nodes")
    print(f"  - Multiple trace format readers")
    print(f"  - Unified visualization output")
    
    print("\nThis workflow can:")
    print("  1. Read traces from OTF2 format")
    print("  2. Read profiles from VTune")
    print("  3. Combine them for analysis")
    print("  4. Visualize results in timeline format")


def compare_trace_formats():
    """
    Compare different trace format characteristics.
    """
    print("\n" + "=" * 60)
    print("TRACE FORMAT COMPARISON")
    print("=" * 60)
    
    formats = [
        {
            "name": "OTF2 (Open Trace Format 2)",
            "reader": "OTF2Reader",
            "file_ext": ".otf2",
            "characteristics": [
                "Standard format for MPI/OpenMP traces",
                "Hierarchical event structure",
                "Used by Score-P, Vampir, TAU"
            ]
        },
        {
            "name": "Intel VTune",
            "reader": "VtuneReader",
            "file_ext": ".vtune",
            "characteristics": [
                "Hardware performance counters",
                "Microarchitecture analysis",
                "Intel-specific optimizations"
            ]
        },
        {
            "name": "NVIDIA Nsight Systems",
            "reader": "NsightReader",
            "file_ext": ".nsys-rep, .qdrep",
            "characteristics": [
                "GPU/CPU activity traces",
                "CUDA kernel profiling",
                "System-wide timeline"
            ]
        },
        {
            "name": "HPCToolkit",
            "reader": "HpctoolkitReader",
            "file_ext": "database directory",
            "characteristics": [
                "Call path profiling",
                "Sampling-based analysis",
                "Scalable to supercomputers"
            ]
        },
        {
            "name": "CTF (Common Trace Format)",
            "reader": "CTFReader",
            "file_ext": "directory with metadata",
            "characteristics": [
                "Binary trace format",
                "Used by LTTng",
                "Very fast to write"
            ]
        },
        {
            "name": "Scalatrace",
            "reader": "ScalatraceReader",
            "file_ext": ".sct",
            "characteristics": [
                "Scalable for large traces",
                "Compressed storage",
                "Efficient random access"
            ]
        }
    ]
    
    for fmt in formats:
        print(f"\n{fmt['name']}")
        print(f"  Reader Class: {fmt['reader']}")
        print(f"  File Extension: {fmt['file_ext']}")
        print("  Characteristics:")
        for char in fmt['characteristics']:
            print(f"    • {char}")


def main():
    """
    Main function demonstrating trace reader usage.
    """
    print("=" * 60)
    print("PERFLOW EXAMPLE: MULTIPLE TRACE READERS")
    print("=" * 60)
    print("\nPerFlow supports reading traces from multiple formats,")
    print("enabling analysis of data from different profiling tools.")
    
    # Demonstrate each reader
    readers = [
        (OTF2Reader, "OTF2 Reader", "/traces/mpi_application.otf2"),
        (VtuneReader, "Intel VTune Reader", "/traces/cpu_hotspots.vtune"),
        (NsightReader, "NVIDIA Nsight Reader", "/traces/gpu_trace.nsys-rep"),
        (HpctoolkitReader, "HPCToolkit Reader", "/traces/hpctoolkit-database"),
        (CTFReader, "CTF Reader", "/traces/lttng-trace"),
        (ScalatraceReader, "Scalatrace Reader", "/traces/parallel_app.sct")
    ]
    
    for reader_class, reader_name, file_path in readers:
        demonstrate_reader(reader_class, reader_name, file_path)
    
    # Show workflow integration
    demonstrate_workflow_integration()
    
    # Compare formats
    compare_trace_formats()
    
    print("\n" + "=" * 60)
    print("KEY BENEFITS")
    print("=" * 60)
    print("\n✓ Unified Interface: All readers follow the same pattern")
    print("✓ Workflow Integration: Readers work seamlessly with FlowGraph")
    print("✓ Format Flexibility: Support for multiple profiling tools")
    print("✓ Extensible: Easy to add new trace format readers")
    print("✓ Type Safe: Full type hints for better tooling support")
    
    print("\n" + "=" * 60)
    print("Example complete!")
    print("=" * 60)


if __name__ == "__main__":
    main()
