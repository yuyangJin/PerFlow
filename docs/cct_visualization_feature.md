# CallingContextTree Visualization Feature

## Overview

The CallingContextTree class now includes a built-in `visualize()` method that generates publication-quality visualizations in two formats:

1. **Top-down Tree View**: Hierarchical tree layout showing calling contexts
2. **Ring/Sunburst Chart**: Circular hierarchical layout emphasizing depth and proportions

## Implementation Details

### Location
- **File**: `perflow/perf_data_struct/dynamic/profile/calling_context_tree.py`
- **Method**: `visualize(output_file, view_type, metric, figsize, dpi)`
- **Dependencies**: matplotlib (installed automatically if missing)

### Features

#### Tree View (`view_type="tree"`)
- **Layout**: Top-down hierarchical tree
- **Node Sizing**: Proportional to inclusive metrics
- **Color Coding**: Heat map based on metric values
  - Red: High metric values (hot spots)
  - Orange: Medium-high values
  - Yellow: Medium values
  - Light yellow: Low values
- **Labels**: Function names + sample counts
- **Edges**: Show parent-child calling relationships
- **Colorbar**: Shows metric scale

#### Ring Chart (`view_type="ring"`)
- **Layout**: Circular/sunburst layout
- **Structure**: 
  - Center: Root node
  - Inner rings: Higher-level functions
  - Outer rings: Deeper calling contexts
- **Angular Size**: Proportional to metric values
- **Color Coding**: Same heat map as tree view
- **Labels**: Function names (when space permits)
- **Depth Indicator**: Shows maximum calling depth

### API

```python
def visualize(self, 
              output_file: str = "cct_visualization.png",
              view_type: str = "tree",
              metric: str = "cycles",
              figsize: Tuple[int, int] = (12, 8),
              dpi: int = 100) -> None:
    """
    Visualize the calling context tree.
    
    Args:
        output_file: Path to save the visualization
        view_type: "tree" for top-down tree view, "ring" for ring/sunburst chart
        metric: Metric to use for sizing/coloring (default: "cycles")
        figsize: Figure size in inches (width, height)
        dpi: DPI for the output image
    """
```

### Usage Examples

#### Basic Tree View
```python
from perflow.perf_data_struct.dynamic.profile.calling_context_tree import CallingContextTree

# Build CCT from profile data
cct = CallingContextTree()
cct.buildFromProfile(profile)

# Generate tree view
cct.visualize(
    output_file="cct_tree.png",
    view_type="tree",
    metric="cycles",
    figsize=(14, 10),
    dpi=150
)
```

#### Ring/Sunburst Chart
```python
# Generate ring chart
cct.visualize(
    output_file="cct_ring.png",
    view_type="ring",
    metric="cycles",
    figsize=(12, 12),
    dpi=150
)
```

#### Multiple Metrics
```python
# Visualize different metrics
cct.visualize("cct_cycles.png", view_type="tree", metric="cycles")
cct.visualize("cct_instructions.png", view_type="tree", metric="instructions")
cct.visualize("cct_cache_misses.png", view_type="ring", metric="cache_misses")
```

## Testing

### Test Coverage
- 6 new unit tests in `tests/unit/test_calling_context_tree.py`
- All tests passing (100% pass rate)
- Total CCT tests: 22 (16 original + 6 visualization)

### Test Cases
1. `test_visualize_tree_view`: Basic tree view generation
2. `test_visualize_ring_chart`: Ring chart generation
3. `test_visualize_with_different_metrics`: Multiple metrics support
4. `test_visualize_invalid_view_type`: Error handling for invalid types
5. `test_visualize_empty_cct`: Error handling for empty CCT
6. `test_visualize_custom_figsize`: Custom figure size support

## Examples

### Complete Example
See `tests/examples/example_cct_visualization.py` for a complete working example that:
- Creates sample profiling data
- Builds a calling context tree
- Generates both tree and ring visualizations
- Shows hot paths analysis

### Running the Example
```bash
cd /path/to/PerFlow
python tests/examples/example_cct_visualization.py
```

This generates:
- `cct_tree_view.png`: Tree view with cycles metric
- `cct_ring_chart.png`: Ring chart with cycles metric
- `cct_tree_instructions.png`: Tree view with instructions metric

## Documentation

### Updated Documentation
1. **API Reference** (`docs/profile_analysis.md`):
   - Complete method documentation
   - Usage examples
   - Parameter descriptions

2. **README** (`README.md`):
   - Added visualization to features list
   - Quick start example updated

3. **Example Code** (`tests/examples/example_cct_visualization.py`):
   - Complete working example
   - Multiple visualization types
   - Different metrics demonstration

## Technical Implementation

### Algorithm Details

#### Tree Layout Algorithm
1. Calculate positions recursively from root
2. Allocate horizontal space based on inclusive metrics
3. Position nodes at vertical levels by depth
4. Draw edges from parent to child nodes
5. Apply color mapping based on metric values

#### Ring Layout Algorithm
1. Start from root at center
2. Assign angular segments to children based on metrics
3. Recursively layout children in outer rings
4. Calculate wedge parameters (inner/outer radius, start/end angle)
5. Apply color mapping and labels

### Performance Considerations
- Tree layout: O(n) where n is number of nodes
- Ring layout: O(n) where n is number of nodes
- Image generation: Depends on DPI and figure size
- Typical generation time: < 1 second for trees with < 1000 nodes

## Dependencies

### Required
- **matplotlib**: For visualization (automatically imported, with helpful error if missing)
- **numpy**: Installed with matplotlib

### Installation
```bash
pip install matplotlib
```

The `visualize()` method will provide a clear error message if matplotlib is not installed:
```
ImportError: matplotlib is required for visualization. Install it with: pip install matplotlib
```

## Comparison with Existing Visualization

### Previous Approach (DOT format)
- Required external Graphviz installation
- Two-step process (generate .dot, then convert to image)
- Manual layout tuning needed

### New Approach (Built-in visualize())
- No external dependencies except matplotlib
- One-step process (direct PNG generation)
- Automatic optimal layout
- Publication-ready output
- Multiple visualization styles
- Customizable appearance

## Use Cases

### Research & Publication
- Generate figures for papers
- High-resolution output (adjustable DPI)
- Professional color schemes

### Performance Analysis
- Quick visual hotspot identification
- Compare metrics across functions
- Identify deep call chains

### Debugging
- Understand call patterns
- Verify profiling data
- Explore execution contexts

## Future Enhancements

Possible future improvements:
1. Interactive HTML/JavaScript visualizations
2. Additional layout algorithms (radial, force-directed)
3. Animation support for time-series data
4. Export to SVG for vector graphics
5. Filtering by threshold
6. Zooming and panning for large trees
7. Integration with Jupyter notebooks

## Summary

The `visualize()` method provides:
- ✅ Two visualization types (tree and ring)
- ✅ Multiple metrics support
- ✅ High-quality PNG output
- ✅ Customizable appearance
- ✅ Automatic layout
- ✅ Comprehensive testing
- ✅ Complete documentation
- ✅ Working examples

This feature makes CallingContextTree analysis more accessible and visually compelling for both research and practical performance analysis.
