"""
PerFlow Python Frontend for Performance Analysis

This package provides a Python interface to the PerFlow performance analysis
framework, including:

- Direct bindings to C++ backend (perflow._perflow_bindings)
- High-level Python API for analysis workflows
- Dataflow graph abstraction for composing analysis tasks
"""

__version__ = "0.1.0"

# Import C++ bindings
try:
    from ._perflow_bindings import *
except ImportError as e:
    import warnings
    warnings.warn(
        f"Failed to import C++ bindings: {e}\n"
        "Make sure the PerFlow C++ library is built and installed correctly.",
        ImportWarning
    )

# Import Python frontend modules
from .dataflow import Node, Edge, Graph, Task
from . import analysis
from . import workflows

__all__ = [
    # C++ bindings (exported via *)
    # Python frontend
    'Node', 'Edge', 'Graph', 'Task',
    'analysis', 'workflows',
]
