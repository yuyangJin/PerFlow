# PerFlow Tests

This directory contains comprehensive tests for the PerFlow 2.0 framework.

## Test Structure

```
tests/
├── unit/                  # Unit tests for individual classes
│   ├── test_event.py              # Event and EventType tests (6 tests)
│   ├── test_trace.py              # Trace and TraceInfo tests (18 tests)
│   ├── test_mpi_event.py          # MPI event hierarchy tests (15 tests)
│   ├── test_tiledtrace.py         # TiledTrace tests (8 tests)
│   ├── test_flow.py               # Flow framework tests (22 tests)
│   ├── test_trace_replayer.py     # TraceReplayer and LateSender tests (19 tests)
│   ├── test_otf2_reader.py        # OTF2Reader tests (6 tests)
│   ├── test_timeline_viewer.py    # TimelineViewer tests (11 tests)
│   ├── test_readers.py            # Multi-format trace readers tests (30 tests)
│   ├── test_sample_data.py        # SampleData tests (21 tests)
│   ├── test_perf_data.py          # PerfData and ProfileInfo tests (36 tests)
│   └── test_profile_analyzer.py   # ProfileAnalyzer and HotspotAnalyzer tests (26 tests)
│
├── integration/           # Integration tests
│   ├── test_trace_visualization.py  # Trace reading and visualization (5 tests)
│   ├── test_trace_replay.py         # Trace replay and analysis (7 tests)
│   └── test_profile_analysis.py     # Profile analysis workflow (13 tests)
│
└── examples/              # End-to-end examples
    ├── example_late_sender_analysis.py      # Late sender detection
    ├── example_comm_pattern_analysis.py     # Communication patterns
    ├── example_load_imbalance_analysis.py   # Load imbalance detection
    ├── example_multiple_trace_readers.py    # Multiple trace format readers
    └── example_hotspot_analysis.py          # Profile hotspot analysis
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

# Multiple trace readers
python tests/examples/example_multiple_trace_readers.py

# Hotspot analysis
python tests/examples/example_hotspot_analysis.py
```

## Test Coverage

### Unit Tests (218 tests)
- **Event Classes** (6 tests): Event, EventType (including LOOP and BRANCH types)
- **Trace Classes** (18 tests): Trace, TraceInfo with enhanced metadata
  - TraceInfo: EP/RP metadata, mapping, format, timing, application name
  - Trace: Event collection and management
- **MPI Events** (15 tests): MpiEvent, MpiP2PEvent, MpiSendEvent, MpiRecvEvent
  - Auto-setting event types in MpiSendEvent and MpiRecvEvent
- **TiledTrace** (8 tests): Tile management and operations
- **Flow Framework** (22 tests): FlowData, FlowNode, FlowGraph
  - FlowData: Creation, add/remove, clear, set behavior
  - FlowNode: Inputs/outputs, abstract methods
  - FlowGraph: Node/edge management, **topological sort execution**, **cycle detection**
- **TraceReplayer** (19 tests): Replay, **separate forward/backward callbacks**, late sender analysis
- **Trace Readers** (47 tests): 
  - **OTF2Reader** (6 tests): Text-based trace parsing with CSV format
  - **VtuneReader** (6 tests): JSON and CSV parsing for VTune exports
  - **NsightReader** (6 tests): JSON, CSV, and SQLite database parsing
  - **HpctoolkitReader** (6 tests): XML and CSV parsing
  - **CTFReader** (6 tests): Metadata and text export parsing
  - **ScalatraceReader** (6 tests): Text CSV and binary format parsing
  - **TimelineViewer** (11 tests): Text, HTML, and JSON visualization formats
- **Profile Data** (83 tests): 
  - **SampleData** (21 tests): Profiling sample data with metrics and call stacks
  - **PerfData and ProfileInfo** (36 tests): Profile data management and metadata
  - **ProfileAnalyzer and HotspotAnalyzer** (26 tests): Profile analysis and hotspot detection

### Integration Tests (25 tests)
- **Trace Visualization** (5 tests): Reading and visualizing traces in multiple formats
- **Trace Replay** (7 tests): Forward/backward replay with analysis callbacks
  - Late Sender Detection: Identifying communication delays across processes
  - Workflow Pipelines: End-to-end analysis workflows with topological execution
- **Profile Analysis** (13 tests): Complete profile analysis workflows
  - Basic workflow: ProfileAnalyzer with callbacks
  - Hotspot detection: HotspotAnalyzer with threshold filtering
  - Multi-profile analysis: Analyzing multiple profiles
  - Edge cases: Empty profiles, single samples, missing data

### End-to-End Examples (5 examples)
- **Late Sender Analysis**: Complete workflow for detecting late senders
- **Communication Pattern Analysis**: Analyzing message patterns and topology
- **Load Imbalance Analysis**: Detecting and quantifying load imbalance
- **Multiple Trace Readers**: Using different trace format readers
- **Hotspot Analysis**: Identifying performance hotspots from profiling data

## Test Highlights

### Comprehensive Coverage
- All 23 implemented classes have dedicated unit tests
- **218 unit tests** covering all functionality including:
  - Enhanced TraceInfo with large-scale parallel metadata
  - Separate forward/backward callbacks in TraceReplayer
  - Topological sort execution in FlowGraph
  - Full parsing implementation for 6 trace format readers
  - Complete profile data structures (PerfData, ProfileInfo, SampleData)
  - Profile analysis infrastructure (ProfileAnalyzer, HotspotAnalyzer)
- **25 integration tests** verify cross-component functionality
- **5 examples** demonstrate real-world usage scenarios

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
