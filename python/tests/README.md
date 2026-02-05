# PerFlow Python Frontend Tests

This directory contains unit and integration tests for the PerFlow Python frontend module.

## Test Structure

```
tests/
├── __init__.py              # Package initialization
├── test_dataflow.py         # Dataflow graph system tests
├── test_bindings.py         # C++ Python bindings tests (NEW)
├── test_analysis.py         # Analysis task tests
├── test_workflows.py        # Workflow tests
├── run_tests.py             # Test runner script
└── README.md                # This file
```

## Running Tests

### Run All Tests

```bash
cd /path/to/PerFlow/python/tests
python3 run_tests.py
```

### Run Specific Test File

```bash
python3 test_dataflow.py
python3 test_bindings.py
python3 test_analysis.py
python3 test_workflows.py
```

### Run with Different Verbosity

```bash
# Quiet mode
python3 run_tests.py --quiet

# Verbose mode
python3 run_tests.py -v

# Very verbose mode
python3 run_tests.py -vv
```

### Run Specific Test Class or Method

```bash
# Run a specific test class
python3 -m unittest test_dataflow.TestGraph

# Run a specific test method
python3 -m unittest test_dataflow.TestGraph.test_topological_sort
```

## Test Coverage

### test_dataflow.py (16 tests)

Tests for the dataflow graph system:
- **TestTask**: Task base class functionality
- **TestNode**: Node creation and properties
- **TestEdge**: Edge creation and connections
- **TestGraph**: Graph operations including:
  - Node and edge addition
  - Topological sorting
  - Parallel execution grouping
  - Sequential and parallel execution
  - Result caching and invalidation
  - Error handling
- **TestGraphVisualization**: DOT format generation

**Dependencies:** None (pure Python tests)

### test_bindings.py (37 tests) - NEW

Tests for the C++ Python bindings layer:
- **TestEnums**: Enum bindings (TreeBuildMode, SampleCountMode, ColorScheme)
- **TestResolvedFrame**: ResolvedFrame class binding and attributes
- **TestTreeNode**: TreeNode class binding, frame access, sample counts
- **TestPerformanceTree**: PerformanceTree class binding, root access, node count
- **TestTreeBuilder**: TreeBuilder class binding with different modes
- **TestTreeVisitor**: TreeVisitor abstract class and subclassing
- **TestTreeTraversal**: TreeTraversal static methods (DFS, BFS, find)
- **TestBalanceAnalysisResult**: BalanceAnalysisResult class binding
- **TestHotspotInfo**: HotspotInfo class binding
- **TestBalanceAnalyzer**: BalanceAnalyzer class binding
- **TestHotspotAnalyzer**: HotspotAnalyzer class binding
- **TestOnlineAnalysis**: OnlineAnalysis class binding
- **TestTypeConversions**: Python-C++ type conversions (string, int, list, enum)
- **TestErrorHandling**: Error handling in bindings (invalid enums, wrong arguments)

**Dependencies:** Requires C++ bindings (auto-skipped if unavailable)

### test_analysis.py (13 tests)

Tests for analysis tasks:
- **TestAnalysisTaskStructure**: Verifies all task classes exist
- **TestLoadTreeTask**: Tree loading with parallel support
- **TestBalanceAnalysisTask**: Load balance analysis
- **TestHotspotAnalysisTask**: Hotspot detection (self and total)
- **TestReportTask**: Report generation (text, JSON, HTML)
- **TestFilterNodesTask**: Tree node filtering
- **TestTraverseTreeTask**: Tree traversal strategies
- **TestExportVisualizationTask**: Visualization export
- **TestAggregateResultsTask**: Result aggregation

**Dependencies:** Requires C++ bindings (auto-skipped if unavailable)

### test_workflows.py (16 tests)

Tests for pre-built workflows:
- **TestWorkflowsStructure**: Verifies workflow functions exist
- **TestBasicAnalysisWorkflow**: Basic analysis workflow
- **TestComparativeAnalysisWorkflow**: Comparative analysis
- **TestHotspotFocusedWorkflow**: Hotspot-focused analysis

**Dependencies:** Requires C++ bindings (auto-skipped if unavailable)

## Total Test Count

- **82 total tests** (16 + 37 + 13 + 16)
- **16 tests** can run without C++ bindings
- **66 tests** require C++ bindings (automatically skipped if unavailable)

## Test Requirements

### Pure Python Tests

The dataflow graph system tests (`test_dataflow.py`) do not require C++ bindings and can run independently.

### Tests Requiring C++ Bindings

Analysis and workflow tests require the C++ bindings to be built. To build them:

```bash
cd /path/to/PerFlow
mkdir build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)

# Set PYTHONPATH
export PYTHONPATH=$PWD/build/python:$PYTHONPATH

# Run tests
cd ../python/tests
python3 run_tests.py
```

Tests that require C++ bindings will be automatically skipped if bindings are not available.

## Continuous Integration

These tests can be integrated into CI/CD pipelines:

```bash
#!/bin/bash
# CI test script

set -e

# Build project
mkdir -p build && cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)
cd ..

# Run C++ tests
cd build
ctest --output-on-failure
cd ..

# Run Python tests
export PYTHONPATH=$PWD/build/python:$PYTHONPATH
cd python/tests
python3 run_tests.py --verbose

# Check exit code
if [ $? -eq 0 ]; then
    echo "All tests passed!"
else
    echo "Tests failed!"
    exit 1
fi
```

## Adding New Tests

To add new tests:

1. Create a new test file following the naming convention `test_*.py`
2. Import necessary modules and create test classes inheriting from `unittest.TestCase`
3. Write test methods with names starting with `test_`
4. Use `@unittest.skipUnless` decorator for tests requiring C++ bindings
5. Run the test runner to verify

Example:

```python
import unittest
from perflow.dataflow import Task

class TestMyFeature(unittest.TestCase):
    """Test my new feature."""
    
    def test_feature_works(self):
        """Test that my feature works."""
        # Test code here
        self.assertTrue(True)

if __name__ == '__main__':
    unittest.main()
```

## Troubleshooting

### Import Errors

If you get import errors:

```bash
# Make sure PYTHONPATH includes the build directory
export PYTHONPATH=/path/to/PerFlow/build/python:$PYTHONPATH

# Verify
python3 -c "import perflow; print('Success')"
```

### Skipped Tests

If many tests are skipped:

```
SKIPPED: Requires C++ bindings
```

This means the C++ bindings are not built or not in PYTHONPATH. Follow the build instructions above.

### Test Failures

If tests fail, check:
1. C++ bindings are built correctly
2. PYTHONPATH is set correctly
3. All dependencies are installed
4. You're running from the correct directory

For detailed output:

```bash
python3 run_tests.py -vv
```

## License

Apache License 2.0 - See LICENSE file for details
