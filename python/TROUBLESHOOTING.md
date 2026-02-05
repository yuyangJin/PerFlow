# Python Module Import Troubleshooting Guide

This guide helps you debug issues when importing the `perflow` Python module.

## Quick Diagnostic Script

Run this script to diagnose your setup:

```bash
#!/bin/bash
# Save as: check_perflow_setup.sh

echo "======================================"
echo "PerFlow Python Module Setup Diagnostic"
echo "======================================"
echo

# 1. Check if build directory exists
echo "1. Checking build directory..."
if [ -d "build" ]; then
    echo "   ✓ build/ directory exists"
else
    echo "   ✗ build/ directory NOT FOUND"
    echo "   → You need to build PerFlow first!"
    echo "   → Run: mkdir build && cd build && cmake .. && make"
    exit 1
fi
echo

# 2. Check if Python bindings were built
echo "2. Checking for compiled Python bindings..."
BINDING_FILE=$(find build -name "_perflow_bindings*.so" 2>/dev/null | head -1)
if [ -n "$BINDING_FILE" ]; then
    echo "   ✓ Found: $BINDING_FILE"
else
    echo "   ✗ _perflow_bindings.so NOT FOUND"
    echo "   → Python bindings were not built"
    echo "   → Check if Python development libraries are installed"
    echo "   → Ubuntu/Debian: sudo apt-get install python3-dev"
    echo "   → RHEL/CentOS: sudo dnf install python3-devel"
    echo "   → Then rebuild: cd build && cmake .. && make"
    exit 1
fi
echo

# 3. Check expected location
echo "3. Checking expected module location..."
if [ -f "build/python/perflow/_perflow_bindings.so" ]; then
    echo "   ✓ Module in expected location: build/python/perflow/_perflow_bindings.so"
    ls -lh build/python/perflow/_perflow_bindings.so
elif [ -f "build/python/perflow/_perflow_bindings.cpython-*.so" ]; then
    echo "   ✓ Module found with Python version suffix:"
    ls -lh build/python/perflow/_perflow_bindings.cpython-*.so
else
    echo "   ✗ Module not in expected location"
    echo "   → Found at: $BINDING_FILE"
    echo "   → This might cause import issues"
fi
echo

# 4. Check PYTHONPATH
echo "4. Checking PYTHONPATH..."
if [ -z "$PYTHONPATH" ]; then
    echo "   ⚠ PYTHONPATH is not set"
    echo "   → Set it with: export PYTHONPATH=\$PWD/build/python:\$PYTHONPATH"
else
    echo "   PYTHONPATH = $PYTHONPATH"
    if echo "$PYTHONPATH" | grep -q "build/python"; then
        echo "   ✓ PYTHONPATH includes build/python"
    else
        echo "   ⚠ PYTHONPATH does not include build/python"
        echo "   → Add it with: export PYTHONPATH=\$PWD/build/python:\$PYTHONPATH"
    fi
fi
echo

# 5. Check Python module structure
echo "5. Checking Python module structure..."
if [ -d "build/python/perflow" ]; then
    echo "   build/python/perflow/ contents:"
    ls -la build/python/perflow/ | grep -v "^total"
else
    echo "   ✗ build/python/perflow/ directory not found"
fi
echo

# 6. Check source Python files
echo "6. Checking source Python files..."
if [ -d "python/perflow" ]; then
    echo "   python/perflow/ contents:"
    ls -la python/perflow/*.py
else
    echo "   ✗ python/perflow/ directory not found"
fi
echo

# 7. Test Python import
echo "7. Testing Python import..."
export PYTHONPATH=$PWD/build/python:$PYTHONPATH
python3 << 'EOF'
import sys
import os

print("   Python version:", sys.version)
print("   Python path:")
for p in sys.path[:5]:
    print(f"     - {p}")

print("\n   Attempting to import perflow...")
try:
    import perflow
    print("   ✓ SUCCESS: perflow imported")
    print(f"   perflow version: {perflow.__version__}")
    print(f"   perflow location: {perflow.__file__}")
except ImportError as e:
    print(f"   ✗ FAILED: {e}")
    print("\n   Attempting to import _perflow_bindings directly...")
    try:
        import _perflow_bindings
        print("   ✓ _perflow_bindings found (but perflow package import failed)")
    except ImportError as e2:
        print(f"   ✗ _perflow_bindings also failed: {e2}")
EOF
echo
echo "======================================"
echo "Diagnostic complete!"
echo "======================================"
