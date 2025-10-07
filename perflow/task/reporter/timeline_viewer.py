'''
module timeline viewer
'''

from typing import Any, List, Optional
from ...flow.flow import FlowNode
from ...perf_data_struct.dynamic.trace.trace import Trace

'''
@class TimelineViewer
Visualize the results with a timeline/trace format
'''


class TimelineViewer(FlowNode):
    """
    TimelineViewer visualizes trace data in timeline format.
    
    This class implements a FlowNode that takes trace data and
    produces timeline visualizations. It can display events
    chronologically across different processes/threads.
    
    Attributes:
        m_traces: List of traces to visualize
        m_output_format: Output format for visualization (text, html, etc.)
    """
    
    def __init__(self, output_format: str = "text") -> None:
        """
        Initialize a TimelineViewer.
        
        Args:
            output_format: Format for visualization output (default: "text")
        """
        super().__init__()
        self.m_traces: List[Trace] = []
        self.m_output_format: str = output_format
    
    def setOutputFormat(self, output_format: str) -> None:
        """
        Set the output format for visualization.
        
        Args:
            output_format: Output format (e.g., "text", "html", "json")
        """
        self.m_output_format = output_format
    
    def getOutputFormat(self) -> str:
        """
        Get the output format.
        
        Returns:
            Current output format
        """
        return self.m_output_format
    
    def addTrace(self, trace: Trace) -> None:
        """
        Add a trace to visualize.
        
        Args:
            trace: Trace object to add
        """
        self.m_traces.append(trace)
    
    def getTraces(self) -> List[Trace]:
        """
        Get all traces to be visualized.
        
        Returns:
            List of Trace objects
        """
        return self.m_traces
    
    def clear(self) -> None:
        """Clear all traces."""
        self.m_traces.clear()
    
    def visualize(self) -> str:
        """
        Generate timeline visualization.
        
        Creates a timeline representation of the traces in the
        specified output format.
        
        Returns:
            String representation of the timeline
        """
        if self.m_output_format == "text":
            return self._visualize_text()
        elif self.m_output_format == "html":
            return self._visualize_html()
        elif self.m_output_format == "json":
            return self._visualize_json()
        else:
            return self._visualize_text()
    
    def _visualize_text(self) -> str:
        """
        Create a text-based timeline visualization.
        
        Returns:
            Text representation of the timeline
        """
        lines = ["Timeline Visualization", "=" * 50]
        
        for trace in self.m_traces:
            trace_info = trace.getTraceInfo()
            lines.append(f"\nTrace: PID={trace_info.getPid()}, TID={trace_info.getTid()}")
            lines.append("-" * 50)
            
            events = trace.getEvents()
            for event in events:
                timestamp = event.getTimestamp() or 0.0
                name = event.getName() or "unknown"
                lines.append(f"  {timestamp:10.6f}  {name}")
        
        return "\n".join(lines)
    
    def _visualize_html(self) -> str:
        """
        Create an HTML timeline visualization.
        
        Returns:
            HTML representation of the timeline
        """
        # Placeholder for HTML visualization
        html = ["<html><body>", "<h1>Timeline Visualization</h1>"]
        
        for trace in self.m_traces:
            trace_info = trace.getTraceInfo()
            html.append(f"<h2>Trace: PID={trace_info.getPid()}, TID={trace_info.getTid()}</h2>")
            html.append("<ul>")
            
            events = trace.getEvents()
            for event in events:
                timestamp = event.getTimestamp() or 0.0
                name = event.getName() or "unknown"
                html.append(f"<li>{timestamp:.6f} - {name}</li>")
            
            html.append("</ul>")
        
        html.extend(["</body></html>"])
        return "\n".join(html)
    
    def _visualize_json(self) -> str:
        """
        Create a JSON timeline visualization.
        
        Returns:
            JSON representation of the timeline
        """
        import json
        
        data = []
        for trace in self.m_traces:
            trace_info = trace.getTraceInfo()
            trace_data = {
                "pid": trace_info.getPid(),
                "tid": trace_info.getTid(),
                "events": []
            }
            
            events = trace.getEvents()
            for event in events:
                event_data = {
                    "timestamp": event.getTimestamp(),
                    "name": event.getName(),
                    "type": str(event.getType()) if event.getType() else None
                }
                trace_data["events"].append(event_data)
            
            data.append(trace_data)
        
        return json.dumps(data, indent=2)
    
    def run(self) -> None:
        """
        Execute the timeline visualization task.
        
        Processes input traces and generates timeline visualizations,
        adding the results to the output flow data.
        """
        # Clear previous traces
        self.clear()
        
        # Collect traces from input data
        for data in self.m_inputs.get_data():
            if isinstance(data, Trace):
                self.addTrace(data)
        
        # Generate visualization
        if self.m_traces:
            visualization = self.visualize()
            # Create output as tuple (hashable) instead of dict
            output = (
                "TimelineVisualization",
                visualization,
                self.m_output_format,
                len(self.m_traces)
            )
            self.m_outputs.add_data(output)