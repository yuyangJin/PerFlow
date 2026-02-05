#!/usr/bin/env python3
"""
Test runner for PerFlow Python frontend

Runs all test suites and reports results.
"""

import unittest
import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))


def run_tests(verbosity=2):
    """
    Run all tests in the tests directory.
    
    Args:
        verbosity: Test output verbosity (0=quiet, 1=normal, 2=verbose)
    
    Returns:
        True if all tests passed, False otherwise
    """
    # Discover and run all tests
    loader = unittest.TestLoader()
    start_dir = os.path.dirname(__file__)
    suite = loader.discover(start_dir, pattern='test_*.py')
    
    runner = unittest.TextTestRunner(verbosity=verbosity)
    result = runner.run(suite)
    
    # Print summary
    print("\n" + "="*70)
    print(f"Tests run: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print(f"Skipped: {len(result.skipped)}")
    print("="*70)
    
    return result.wasSuccessful()


def main():
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Run PerFlow Python frontend tests'
    )
    parser.add_argument(
        '-v', '--verbose',
        action='count',
        default=1,
        help='Increase verbosity (can be used multiple times)'
    )
    parser.add_argument(
        '-q', '--quiet',
        action='store_true',
        help='Minimal output'
    )
    parser.add_argument(
        '--pattern',
        default='test_*.py',
        help='Pattern to match test files (default: test_*.py)'
    )
    
    args = parser.parse_args()
    
    # Determine verbosity
    if args.quiet:
        verbosity = 0
    else:
        verbosity = min(args.verbose, 2)
    
    # Run tests
    success = run_tests(verbosity=verbosity)
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
