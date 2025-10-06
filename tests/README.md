# PerFlow Tests

This directory contains comprehensive tests for the PerFlow 2.0 framework.

## Test Structure

```
tests/
├── unit/                  # Unit tests for individual classes
│   ├── test_event.py              # Event and EventType tests
│   ├── test_trace.py              # Trace and TraceInfo tests
│   ├── test_mpi_event.py          # MPI event hierarchy tests
│   ├── test_tiledtrace.py         # TiledTrace tests
│   ├── test_flow.py               # Flow framework tests
│   ├── test_trace_replayer.py     # TraceReplayer and LateSender tests
│   └── test_reader_viewer.py      # OTF2Reader and TimelineViewer tests
│
├── integration/           # Integration tests
│   ├── test_trace_visualization.py  # Trace reading and visualization
│   └── test_trace_replay.py         # Trace replay and analysis
│
└── examples/              # End-to-end examples
    ├── example_late_sender_analysis.py      # Late sender detection
    ├── example_comm_pattern_analysis.py     # Communication patterns
    └── example_load_imbalance_analysis.py   # Load imbalance detection
```

## Running Tests

### Run all tests
```bash
pytest tests/
```

### Run only unit tests
```bash
pytest tests/unit/
```

### Run only integration tests
```bash
pytest tests/integration/
```

### Run with verbose output
```bash
pytest tests/ -v
```

### Run with coverage
```bash
pytest tests/ --cov=perflow
```

## Running Examples

Examples can be run directly as Python scripts:

```bash
# Late sender analysis
python tests/examples/example_late_sender_analysis.py

# Communication pattern analysis
python tests/examples/example_comm_pattern_analysis.py

# Load imbalance analysis
python tests/examples/example_load_imbalance_analysis.py
```

## Test Coverage

### Unit Tests (92 tests)
- **Event Classes** (6 tests): Event, EventType
- **Trace Classes** (11 tests): Trace, TraceInfo
- **MPI Events** (15 tests): MpiEvent, MpiP2PEvent, MpiSendEvent, MpiRecvEvent
- **TiledTrace** (8 tests): Tile management and operations
- **Flow Framework** (26 tests): FlowData, FlowNode, FlowGraph
- **TraceReplayer** (15 tests): Replay, callbacks, late sender analysis
- **Readers/Viewers** (11 tests): OTF2Reader, TimelineViewer

### Integration Tests (12 tests)
- **Trace Visualization**: Reading and visualizing traces in multiple formats
- **Trace Replay**: Forward/backward replay with analysis callbacks
- **Late Sender Detection**: Identifying communication delays
- **Workflow Pipelines**: End-to-end analysis workflows

### End-to-End Examples (3 examples)
- **Late Sender Analysis**: Complete workflow for detecting late senders
- **Communication Pattern Analysis**: Analyzing message patterns and topology
- **Load Imbalance Analysis**: Detecting and quantifying load imbalance

## Test Highlights

### Comprehensive Coverage
- All 15 implemented classes have dedicated unit tests
- Integration tests verify cross-component functionality
- Examples demonstrate real-world usage scenarios

### Well-Documented
- Each test has descriptive docstrings
- Examples include detailed explanations and output formatting
- Clear test names following convention: `test_<feature>_<scenario>`

### Best Practices
- Uses pytest framework
- Follows AAA pattern (Arrange, Act, Assert)
- Tests are isolated and independent
- Includes both positive and negative test cases

## Adding New Tests

When adding new functionality:

1. **Unit Tests**: Add tests in `tests/unit/` for individual components
2. **Integration Tests**: Add tests in `tests/integration/` for component interactions
3. **Examples**: Add examples in `tests/examples/` for complete workflows

Follow the existing patterns and conventions for consistency.
