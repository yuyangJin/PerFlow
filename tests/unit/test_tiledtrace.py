"""
Unit tests for TiledTrace class
"""
import pytest
from perflow.perf_data_struct.dynamic.trace.tiletrace import TiledTrace
from perflow.perf_data_struct.dynamic.trace.trace import Trace
from perflow.perf_data_struct.dynamic.trace.event import Event, EventType


class TestTiledTrace:
    """Test TiledTrace class"""
    
    def test_tiledtrace_creation(self):
        """Test creating empty tiled trace"""
        tiled_trace = TiledTrace()
        assert tiled_trace.get_tile_count() == 0
        assert len(tiled_trace.get_tiles()) == 0
    
    def test_tiledtrace_inherits_from_trace(self):
        """Test that TiledTrace inherits from Trace"""
        tiled_trace = TiledTrace()
        assert isinstance(tiled_trace, Trace)
    
    def test_tiledtrace_add_tile(self):
        """Test adding tiles"""
        tiled_trace = TiledTrace()
        
        tile1 = {"start": 0, "end": 100, "events": []}
        tile2 = {"start": 100, "end": 200, "events": []}
        
        tiled_trace.add_tile(tile1)
        assert tiled_trace.get_tile_count() == 1
        
        tiled_trace.add_tile(tile2)
        assert tiled_trace.get_tile_count() == 2
    
    def test_tiledtrace_get_tile(self):
        """Test getting tile by index"""
        tiled_trace = TiledTrace()
        
        tile1 = {"id": 1, "data": "tile_1"}
        tile2 = {"id": 2, "data": "tile_2"}
        tile3 = {"id": 3, "data": "tile_3"}
        
        tiled_trace.add_tile(tile1)
        tiled_trace.add_tile(tile2)
        tiled_trace.add_tile(tile3)
        
        assert tiled_trace.get_tile(0) == tile1
        assert tiled_trace.get_tile(1) == tile2
        assert tiled_trace.get_tile(2) == tile3
        assert tiled_trace.get_tile(3) is None
        assert tiled_trace.get_tile(-1) is None
    
    def test_tiledtrace_get_tiles(self):
        """Test getting all tiles"""
        tiled_trace = TiledTrace()
        
        tiles = [
            {"id": 1, "range": (0, 50)},
            {"id": 2, "range": (50, 100)},
            {"id": 3, "range": (100, 150)}
        ]
        
        for tile in tiles:
            tiled_trace.add_tile(tile)
        
        retrieved_tiles = tiled_trace.get_tiles()
        assert len(retrieved_tiles) == 3
        assert retrieved_tiles == tiles
    
    def test_tiledtrace_clear_tiles(self):
        """Test clearing all tiles"""
        tiled_trace = TiledTrace()
        
        tiled_trace.add_tile({"id": 1})
        tiled_trace.add_tile({"id": 2})
        tiled_trace.add_tile({"id": 3})
        
        assert tiled_trace.get_tile_count() == 3
        
        tiled_trace.clear_tiles()
        assert tiled_trace.get_tile_count() == 0
        assert len(tiled_trace.get_tiles()) == 0
    
    def test_tiledtrace_with_events_and_tiles(self):
        """Test TiledTrace with both events and tiles"""
        tiled_trace = TiledTrace()
        
        # Add events (inherited from Trace)
        event1 = Event(EventType.ENTER, 1, "func1", 0, 0, 1.0, 0)
        event2 = Event(EventType.LEAVE, 2, "func1", 0, 0, 2.0, 0)
        tiled_trace.addEvent(event1)
        tiled_trace.addEvent(event2)
        
        # Add tiles
        tile1 = {"start": 0.0, "end": 1.5, "event_count": 1}
        tile2 = {"start": 1.5, "end": 3.0, "event_count": 1}
        tiled_trace.add_tile(tile1)
        tiled_trace.add_tile(tile2)
        
        # Verify both functionalities work
        assert tiled_trace.getEventCount() == 2
        assert tiled_trace.get_tile_count() == 2
    
    def test_tiledtrace_tile_types(self):
        """Test that tiles can be various types"""
        tiled_trace = TiledTrace()
        
        # Tiles can be dictionaries
        tiled_trace.add_tile({"type": "time_window", "start": 0, "end": 100})
        
        # Tiles can be lists
        tiled_trace.add_tile([0, 100, "events"])
        
        # Tiles can be custom objects
        class CustomTile:
            def __init__(self, id):
                self.id = id
        
        tiled_trace.add_tile(CustomTile(1))
        
        assert tiled_trace.get_tile_count() == 3
        assert isinstance(tiled_trace.get_tile(0), dict)
        assert isinstance(tiled_trace.get_tile(1), list)
        assert isinstance(tiled_trace.get_tile(2), CustomTile)
