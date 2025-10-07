'''
@module tiletrace
'''

from typing import List, Optional, Any
from .trace import Trace

'''
@class TiledTrace
A tiled structure of event trace. This class extends Trace to provide
a tiled representation of traces, where events are organized into tiles
for efficient processing and analysis of large-scale parallel applications.

A tile is a logical grouping of events, which can represent time windows,
process groups, or other organizational structures useful for trace analysis.
'''


class TiledTrace(Trace):
    """
    TiledTrace extends Trace to provide a tiled structure for organizing events.
    
    This class is designed for analyzing large-scale parallel application traces
    by dividing them into manageable tiles. Tiles can represent various
    organizational structures such as time windows, process groups, or
    communication patterns.
    
    Attributes:
        m_tiles: A list of tiles, where each tile can contain a subset of events
                 or references to event collections.
    """
    
    def __init__(self) -> None:
        """
        Initialize a TiledTrace object.
        
        Calls the parent Trace constructor and initializes an empty list of tiles.
        """
        super().__init__()
        self.m_tiles: List[Any] = []
    
    def add_tile(self, tile: Any) -> None:
        """
        Add a tile to the tiled trace.
        
        Args:
            tile: A tile object to be added to the collection. The exact type
                  depends on the specific tile implementation being used.
        """
        self.m_tiles.append(tile)
    
    def get_tile(self, index: int) -> Optional[Any]:
        """
        Get a specific tile by index.
        
        Args:
            index: The index of the tile to retrieve.
            
        Returns:
            The tile at the specified index, or None if the index is out of bounds.
        """
        if 0 <= index < len(self.m_tiles):
            return self.m_tiles[index]
        return None
    
    def get_tiles(self) -> List[Any]:
        """
        Get all tiles in the tiled trace.
        
        Returns:
            A list of all tiles.
        """
        return self.m_tiles
    
    def get_tile_count(self) -> int:
        """
        Get the number of tiles in the tiled trace.
        
        Returns:
            The total number of tiles.
        """
        return len(self.m_tiles)
    
    def clear_tiles(self) -> None:
        """
        Remove all tiles from the tiled trace.
        """
        self.m_tiles.clear()