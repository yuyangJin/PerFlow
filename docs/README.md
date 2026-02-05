# PerFlow Documentation

Welcome to the PerFlow documentation! This guide will help you understand, install, and use PerFlow for performance analysis of parallel MPI applications.

## Documentation Structure

### 📚 User Guides
Start here if you're new to PerFlow or want to use it for profiling your applications.

- **[Getting Started](user-guide/GETTING_STARTED.md)** - Installation, quick start, and first steps
- **[Timer Sampler Usage](TIMER_SAMPLER_USAGE.md)** - Using the platform-independent timer-based sampler
- **[Configuration Guide](user-guide/CONFIGURATION.md)** - Environment variables and configuration options
- **[Troubleshooting](user-guide/TROUBLESHOOTING.md)** - Common issues and solutions

### 🔧 Developer Guides
For those who want to extend PerFlow or understand its internals.

- **[Architecture Overview](developer-guide/ARCHITECTURE.md)** - High-level design and components
- **[Sampling Framework](sampling_framework.md)** - Core sampling infrastructure
- **[Symbol Resolution](SYMBOL_RESOLUTION.md)** - Function name and source location resolution
- **[Fortran MPI Support](FORTRAN_MPI_SUPPORT.md)** - MPI name mangling and Fortran integration
- **[Contributing Guidelines](contributing.md)** - How to contribute to PerFlow

### 📖 API Reference
Detailed API documentation for developers integrating PerFlow.

- **[Online Analysis API](ONLINE_ANALYSIS_API.md)** - Complete API for performance tree analysis
- **[Call Stack Offset Conversion](CALL_STACK_OFFSET_CONVERSION.md)** - Address-to-offset conversion details
- **[MPI Sampler API](MPI_SAMPLER.md)** - Hardware PMU sampler documentation

### 📊 Performance Analysis
Understanding and optimizing PerFlow's performance.

- **[Timer-Based Sampler Performance](TIMER_BASED_SAMPLER.md)** - Overhead analysis and benchmarks
- **[Testing Guide](../TESTING.md)** - Comprehensive testing documentation

## Quick Links

- [Main README](../README.md) - Project overview and quick start
- [Examples](../examples/README.md) - Working code examples
- [Tests](../tests/README.md) - Test programs and benchmarks

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
