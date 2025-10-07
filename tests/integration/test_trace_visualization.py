"""
Integration test: Read traces and visualize them
"""
import pytest
from perflow.task.reader.otf2_reader import OTF2Reader
from perflow.task.reporter.timeline_viewer import TimelineViewer
from perflow.flow.flow import FlowGraph
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType


class TestTraceVisualizationPipeline:
    """Integration test for trace reading and visualization"""
    
    def test_simple_trace_visualization(self):
        """Test creating, reading and visualizing a simple trace"""
        # Create a trace manually (simulating read operation)
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        # Add sample events
        trace.addEvent(Event(EventType.ENTER, 1, "main", 0, 0, 0.0, 0))
        trace.addEvent(Event(EventType.ENTER, 2, "compute", 0, 0, 1.0, 0))
        trace.addEvent(Event(EventType.LEAVE, 3, "compute", 0, 0, 5.0, 0))
        trace.addEvent(Event(EventType.LEAVE, 4, "main", 0, 0, 6.0, 0))
        
        # Visualize the trace
        viewer = TimelineViewer("text")
        viewer.addTrace(trace)
        visualization = viewer.visualize()
        
        # Verify visualization contains expected data
        assert "Timeline Visualization" in visualization
        assert "main" in visualization
        assert "compute" in visualization
        assert "0.000000" in visualization
        assert "6.000000" in visualization
    
    def test_multiple_traces_visualization(self):
        """Test visualizing traces from multiple processes"""
        traces = []
        
        # Create traces for 3 processes
        for pid in range(3):
            trace = Trace()
            trace_info = TraceInfo(pid=pid, tid=0)
            trace.setTraceInfo(trace_info)
            
            # Add events
            trace.addEvent(Event(EventType.ENTER, pid*10+1, f"func_{pid}", pid, 0, float(pid), pid))
            trace.addEvent(Event(EventType.LEAVE, pid*10+2, f"func_{pid}", pid, 0, float(pid+1), pid))
            
            traces.append(trace)
        
        # Visualize all traces
        viewer = TimelineViewer("text")
        for trace in traces:
            viewer.addTrace(trace)
        
        visualization = viewer.visualize()
        
        # Verify all processes are represented
        assert "PID=0" in visualization
        assert "PID=1" in visualization
        assert "PID=2" in visualization
        assert "func_0" in visualization
        assert "func_1" in visualization
        assert "func_2" in visualization
    
    def test_json_visualization_format(self):
        """Test JSON visualization format"""
        import json
        
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        trace.addEvent(Event(EventType.SEND, 1, "mpi_send", 0, 0, 1.0, 0))
        trace.addEvent(Event(EventType.RECV, 2, "mpi_recv", 0, 0, 2.0, 0))
        
        viewer = TimelineViewer("json")
        viewer.addTrace(trace)
        visualization = viewer.visualize()
        
        # Parse and validate JSON
        data = json.loads(visualization)
        assert isinstance(data, list)
        assert len(data) == 1
        assert data[0]["pid"] == 0
        assert len(data[0]["events"]) == 2
    
    def test_html_visualization_format(self):
        """Test HTML visualization format"""
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        trace.addEvent(Event(EventType.ENTER, 1, "function", 0, 0, 1.0, 0))
        
        viewer = TimelineViewer("html")
        viewer.addTrace(trace)
        visualization = viewer.visualize()
        
        # Verify HTML structure
        assert visualization.startswith("<html>")
        assert "</html>" in visualization
        assert "function" in visualization
    
    def test_workflow_pipeline(self):
        """Test complete workflow with FlowGraph"""
        # Create workflow
        workflow = FlowGraph()
        
        # Create and configure reader (with mock trace)
        reader = OTF2Reader()
        trace = Trace()
        trace.addEvent(Event(EventType.COMPUTE, 1, "compute", 0, 0, 1.0, 0))
        reader.get_outputs().add_data(trace)
        
        # Create viewer
        viewer = TimelineViewer("text")
        
        # Build workflow
        workflow.add_edge(reader, viewer)
        
        # Run workflow
        workflow.run()
        
        # Verify output
        outputs = viewer.get_outputs().get_data()
        assert len(outputs) == 1
        
        output_tuple = list(outputs)[0]
        assert output_tuple[0] == "TimelineVisualization"
