"""
Unit tests for OTF2Reader and TimelineViewer classes
"""
import pytest
from perflow.task.reader.otf2_reader import OTF2Reader
from perflow.task.reporter.timeline_viewer import TimelineViewer
from perflow.perf_data_struct.dynamic.trace.trace import Trace, TraceInfo
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType
from perflow.flow.flow import FlowNode


class TestOTF2Reader:
    """Test OTF2Reader class"""
    
    def test_otf2reader_creation(self):
        """Test creating OTF2Reader"""
        reader = OTF2Reader()
        assert reader.getFilePath() is None
        assert reader.getTrace() is None
    
    def test_otf2reader_creation_with_path(self):
        """Test creating OTF2Reader with file path"""
        reader = OTF2Reader("/path/to/trace.otf2")
        assert reader.getFilePath() == "/path/to/trace.otf2"
    
    def test_otf2reader_inherits_from_flownode(self):
        """Test that OTF2Reader inherits from FlowNode"""
        reader = OTF2Reader()
        assert isinstance(reader, FlowNode)
    
    def test_otf2reader_set_file_path(self):
        """Test setting file path"""
        reader = OTF2Reader()
        reader.setFilePath("/test/trace.otf2")
        assert reader.getFilePath() == "/test/trace.otf2"
    
    def test_otf2reader_load(self):
        """Test loading trace"""
        import tempfile
        import os
        
        # Create a temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("ENTER,1,main,0,0,0.0,0\n")
            f.write("LEAVE,2,main,0,0,1.0,0\n")
            temp_file = f.name
        
        try:
            reader = OTF2Reader(temp_file)
            trace = reader.load()
            
            assert trace is not None
            assert isinstance(trace, Trace)
            assert reader.getTrace() == trace
        finally:
            os.unlink(temp_file)
    
    def test_otf2reader_run(self):
        """Test running OTF2Reader"""
        import tempfile
        import os
        
        # Create a temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write("ENTER,1,main,0,0,0.0,0\n")
            f.write("LEAVE,2,main,0,0,1.0,0\n")
            temp_file = f.name
        
        try:
            reader = OTF2Reader(temp_file)
            reader.run()
            
            # Should have trace in outputs
            outputs = reader.get_outputs().get_data()
            assert len(outputs) == 1
            
            # Output should be a Trace
            output_trace = list(outputs)[0]
            assert isinstance(output_trace, Trace)
        finally:
            os.unlink(temp_file)


class TestTimelineViewer:
    """Test TimelineViewer class"""
    
    def test_timelineviewer_creation(self):
        """Test creating TimelineViewer"""
        viewer = TimelineViewer()
        assert viewer.getOutputFormat() == "text"
        assert len(viewer.getTraces()) == 0
    
    def test_timelineviewer_creation_with_format(self):
        """Test creating TimelineViewer with format"""
        viewer = TimelineViewer("html")
        assert viewer.getOutputFormat() == "html"
    
    def test_timelineviewer_inherits_from_flownode(self):
        """Test that TimelineViewer inherits from FlowNode"""
        viewer = TimelineViewer()
        assert isinstance(viewer, FlowNode)
    
    def test_timelineviewer_set_output_format(self):
        """Test setting output format"""
        viewer = TimelineViewer()
        viewer.setOutputFormat("json")
        assert viewer.getOutputFormat() == "json"
    
    def test_timelineviewer_add_trace(self):
        """Test adding trace"""
        viewer = TimelineViewer()
        trace = Trace()
        
        viewer.addTrace(trace)
        assert len(viewer.getTraces()) == 1
        assert viewer.getTraces()[0] == trace
    
    def test_timelineviewer_clear(self):
        """Test clearing traces"""
        viewer = TimelineViewer()
        
        viewer.addTrace(Trace())
        viewer.addTrace(Trace())
        assert len(viewer.getTraces()) == 2
        
        viewer.clear()
        assert len(viewer.getTraces()) == 0
    
    def test_timelineviewer_text_visualization(self):
        """Test text visualization"""
        viewer = TimelineViewer("text")
        
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        trace.addEvent(Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0))
        trace.addEvent(Event(EventType.LEAVE, 2, "func1", 0, 0, 2.0, 0))
        
        viewer.addTrace(trace)
        visualization = viewer.visualize()
        
        assert "Timeline Visualization" in visualization
        assert "func1" in visualization
        assert "1.000000" in visualization
        assert "2.000000" in visualization
    
    def test_timelineviewer_html_visualization(self):
        """Test HTML visualization"""
        viewer = TimelineViewer("html")
        
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        trace.addEvent(Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0))
        
        viewer.addTrace(trace)
        visualization = viewer.visualize()
        
        assert "<html>" in visualization
        assert "<body>" in visualization
        assert "func1" in visualization
    
    def test_timelineviewer_json_visualization(self):
        """Test JSON visualization"""
        viewer = TimelineViewer("json")
        
        trace = Trace()
        trace_info = TraceInfo(pid=0, tid=0)
        trace.setTraceInfo(trace_info)
        
        trace.addEvent(Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0))
        
        viewer.addTrace(trace)
        visualization = viewer.visualize()
        
        # Should be valid JSON
        import json
        data = json.loads(visualization)
        
        assert isinstance(data, list)
        assert len(data) == 1
        assert data[0]["pid"] == 0
        assert data[0]["tid"] == 0
        assert len(data[0]["events"]) == 1
    
    def test_timelineviewer_run(self):
        """Test running TimelineViewer"""
        viewer = TimelineViewer("text")
        
        trace = Trace()
        trace.addEvent(Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0))
        
        # Add trace to inputs
        viewer.get_inputs().add_data(trace)
        
        viewer.run()
        
        # Should have visualization in outputs
        outputs = viewer.get_outputs().get_data()
        assert len(outputs) == 1
        
        output_tuple = list(outputs)[0]
        assert output_tuple[0] == "TimelineVisualization"  # Type marker
        assert isinstance(output_tuple[1], str)  # Visualization
        assert output_tuple[2] == "text"  # Format
        assert output_tuple[3] == 1  # Trace count
    
    def test_timelineviewer_multiple_traces(self):
        """Test visualizing multiple traces"""
        viewer = TimelineViewer("text")
        
        # Create two traces
        trace1 = Trace()
        trace1.setTraceInfo(TraceInfo(pid=0, tid=0))
        trace1.addEvent(Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0))
        
        trace2 = Trace()
        trace2.setTraceInfo(TraceInfo(pid=1, tid=0))
        trace2.addEvent(Event(EventType.ENTER, 2, "func2", 1, 0, 1.5, 1))
        
        viewer.addTrace(trace1)
        viewer.addTrace(trace2)
        
        visualization = viewer.visualize()
        
        assert "PID=0" in visualization
        assert "PID=1" in visualization
        assert "func1" in visualization
        assert "func2" in visualization
