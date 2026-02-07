# MPI Sampler Test Suite

This directory contains comprehensive tests for the PerFlow MPI performance sampler.

## Quick Start

```bash
# Build all tests
cd build
cmake .. -DPERFLOW_BUILD_TESTS=ON
make -j$(nproc)

# Run all tests
cd ..
./tests/run_all_tests.sh

# Run specific test
cd build
make run_test_compute_intensive
```

## Test Programs

| Test Program | Description | Focus Area | Duration |
|--------------|-------------|------------|----------|
| `mpi_sampler_test` | Basic validation | General functionality | ~5s |
| `test_compute_intensive` | CPU-bound workloads | Matrix ops, FFT, numerical methods | ~1s |
| `test_comm_intensive` | MPI communication | Various communication patterns | ~1s |
| `test_memory_intensive` | Memory operations | Allocation, access patterns, cache | ~2s |
| `test_hybrid` | Mixed workloads | Computation + communication overlap | ~1s |
| `test_stress` | High load conditions | Stability and performance | ~30s |

## Files

- **Test Programs**: `test_*.cpp` - Individual test programs
- **Test Script**: `run_all_tests.sh` - Automated test execution
- **Validation**: `validate_sampler_results.py` - Output validation
- **Documentation**: `../TESTING.md` - Comprehensive testing guide
- **Build Config**: `CMakeLists.txt` - CMake configuration

## Running Tests

### Using the Test Script

```bash
# Default (4 processes, 1000 Hz)
./tests/run_all_tests.sh

# Custom configuration
./tests/run_all_tests.sh -n 8 -f 10000 -v

# Options:
#   -n NUM    Number of MPI processes
#   -f FREQ   Sampling frequency (Hz)
#   -o DIR    Output directory
#   -v        Verbose mode
```

### Using CMake Targets

```bash
cd build

# Run single test
make run_test_compute_intensive

# Run all tests (currently builds, doesn't execute)
make run_all_mpi_tests
```

### Manual Execution

```bash
cd build
LD_PRELOAD=lib/libperflow_mpi_sampler.so \
PERFLOW_OUTPUT_DIR=/tmp \
PERFLOW_SAMPLING_FREQ=1000 \
mpirun -n 4 tests/test_compute_intensive
```

## Environment Variables

- `PERFLOW_OUTPUT_DIR`: Output directory (default: `/tmp`)
- `PERFLOW_SAMPLING_FREQ`: Sampling frequency in Hz (default: `1000`)

## Expected Output

Each test should:
- Complete successfully without errors
- Print timing statistics
- Generate `.pflw` (binary) and `.txt` (human-readable) output files per rank

## Validation

```bash
# Validate test results
python3 tests/validate_sampler_results.py /tmp/perflow_test_results
```

The validation script checks:
- Expected patterns in output
- Sample file generation
- Minimum sample counts
- Data validity

## Troubleshooting

### No output files generated

This is expected in containerized environments without PAPI permissions. The test programs themselves still validate the sampler's integration with MPI.

### Tests fail to build

Install dependencies:
```bash
sudo apt-get install libopenmpi-dev openmpi-bin libpapi-dev libunwind-dev
```

### MPI errors

- Use fewer processes: `-n 2`
- Add oversubscribe flag: `mpirun --oversubscribe ...`

## More Information

See `../TESTING.md` for comprehensive documentation including:
- Detailed test descriptions
- Validation criteria
- Performance considerations
- Troubleshooting guide

## Support

- GitHub: https://github.com/yuyangJin/PerFlow
- Issues: https://github.com/yuyangJin/PerFlow/issues
