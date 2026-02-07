# PerFlow Documentation

Welcome to the PerFlow documentation! This guide will help you understand, install, and use PerFlow for performance analysis of parallel MPI applications.

## Programming Interfaces

PerFlow provides two powerful interfaces for performance analysis:

1. **C++ API**: Direct access to the high-performance analysis engine
   - Online analysis with directory monitoring
   - Symbol resolution with function names and source locations
   - Performance tree generation and visualization
   - Suitable for integration into native C++ tools

2. **Python Dataflow API** (Recommended for analysis workflows): Intuitive dataflow-based programming
   - Compose analysis workflows as directed acyclic graphs (DAGs)
   - Automatic parallel execution and result caching
   - Fluent WorkflowBuilder API for rapid prototyping
   - Pre-built analysis nodes (LoadData, HotspotAnalysis, BalanceAnalysis, etc.)
   - Ideal for exploratory analysis and custom workflows

## Documentation Structure

### 📚 User Guides
Start here if you're new to PerFlow or want to use it for profiling your applications.

- **[Getting Started](user-guide/GETTING_STARTED.md)** - Installation, quick start, and first steps
- **[Configuration Guide](user-guide/CONFIGURATION.md)** - Environment variables and configuration options
- **[Timer Sampler Usage](user-guide/TIMER_SAMPLER_USAGE.md)** - Using the platform-independent timer-based sampler
- **[Timer Sampler Performance](user-guide/TIMER_BASED_SAMPLER.md)** - Overhead analysis and benchmarks
- **[Troubleshooting](user-guide/TROUBLESHOOTING.md)** - Common issues and solutions

### 🔧 Developer Guides
For those who want to extend PerFlow or understand its internals.

- **[Architecture Overview](developer-guide/ARCHITECTURE.md)** - High-level design and components
- **[Sampling Framework](developer-guide/sampling_framework.md)** - Core sampling infrastructure
- **[Fortran MPI Support](developer-guide/FORTRAN_MPI_SUPPORT.md)** - MPI name mangling and Fortran integration
- **[Implementation Summary](developer-guide/IMPLEMENTATION_SUMMARY.md)** - Symbol resolution feature details
- **[Contributing Guidelines](developer-guide/contributing.md)** - How to contribute to PerFlow
- **[Documentation Style Guide](STYLE_GUIDE.md)** - Style guide for documentation

### 📖 API Reference
Detailed API documentation for developers integrating PerFlow.

- **[Online Analysis API](api-reference/ONLINE_ANALYSIS_API.md)** - Complete API for performance tree analysis (C++)
- **[Symbol Resolution API](api-reference/SYMBOL_RESOLUTION.md)** - Function name and source location resolution (C++)
- **[Python API Reference](api-reference/python-api.md)** - Python bindings for PerFlow C++ library
- **[Dataflow API Reference](api-reference/dataflow-api.md)** - Python dataflow-based programming framework
- **[Call Stack Offset Conversion](api-reference/CALL_STACK_OFFSET_CONVERSION.md)** - Address-to-offset conversion details (C++)
- **[MPI Sampler API](api-reference/MPI_SAMPLER.md)** - Hardware PMU sampler documentation (C++)

### 📊 Testing & Validation

- **[Testing Guide](../TESTING.md)** - Comprehensive testing documentation
- **[NPB Benchmark Suite](../tests/npb_benchmark/README.md)** - NAS Parallel Benchmarks integration

### ❓ FAQ & Support

- **[Frequently Asked Questions](FAQ.md)** - Common questions and answers
- **[GitHub Issues](https://github.com/yuyangJin/PerFlow/issues)** - Report bugs and request features

## Quick Links

- **[Main README](../README.md)** - Project overview and quick start
- **[Examples](../examples/README.md)** - Working code examples
- **[Tests](../tests/README.md)** - Test programs and benchmarks

## Additional Resources

- **License**: Apache License 2.0
- **Repository**: [github.com/yuyangJin/PerFlow](https://github.com/yuyangJin/PerFlow)
- **Issues**: Report bugs and request features on [GitHub Issues](https://github.com/yuyangJin/PerFlow/issues)

## Documentation Conventions

Throughout this documentation:
- 💡 **Tips** - Helpful suggestions and best practices
- ⚠️ **Warnings** - Important information to avoid common pitfalls
- 📝 **Examples** - Code examples and usage patterns
- 🔍 **Advanced** - Advanced topics for experienced users
