#!/bin/bash
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# run_all_tests.sh - Automated test execution script for MPI sampler tests
#
# This script runs all MPI sampler test programs and collects their outputs.
# It supports various configurations and generates a summary report.

set -e  # Exit on error

# ============================================================================
# Configuration
# ============================================================================

# Default values
NUM_PROCESSES=4
OUTPUT_DIR="/tmp/perflow_test_results"
SAMPLING_FREQ=1000
BUILD_DIR="build"
VERBOSE=0
RUN_VALIDATION=1

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# ============================================================================
# Usage Information
# ============================================================================

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Automated test execution script for MPI sampler tests.

OPTIONS:
    -n NUM      Number of MPI processes (default: 4)
    -o DIR      Output directory for results (default: /tmp/perflow_test_results)
    -f FREQ     Sampling frequency in Hz (default: 1000)
    -b DIR      Build directory (default: build)
    -v          Verbose mode
    --no-validation  Skip validation script
    -h          Show this help message

EXAMPLES:
    # Run with default settings (4 processes, 1000 Hz)
    $0

    # Run with 8 processes and higher sampling frequency
    $0 -n 8 -f 10000

    # Run in verbose mode with custom output directory
    $0 -v -o /tmp/my_results

EOF
    exit 1
}

# ============================================================================
# Parse Command Line Arguments
# ============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        -n)
            NUM_PROCESSES="$2"
            shift 2
            ;;
        -o)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -f)
            SAMPLING_FREQ="$2"
            shift 2
            ;;
        -b)
            BUILD_DIR="$2"
            shift 2
            ;;
        -v)
            VERBOSE=1
            shift
            ;;
        --no-validation)
            RUN_VALIDATION=0
            shift
            ;;
        -h)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# ============================================================================
# Helper Functions
# ============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dependencies() {
    log_info "Checking dependencies..."
    
    if ! command -v mpirun &> /dev/null; then
        log_error "mpirun not found. Please install MPI."
        exit 1
    fi
    
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        exit 1
    fi
    
    if [ ! -f "$BUILD_DIR/lib/libperflow_mpi_sampler.so" ]; then
        log_error "MPI sampler library not found. Please build the project first."
        exit 1
    fi
    
    log_info "All dependencies satisfied."
}

# ============================================================================
# Main Test Execution
# ============================================================================

main() {
    log_info "=========================================="
    log_info "MPI Sampler Test Suite"
    log_info "=========================================="
    log_info "Configuration:"
    log_info "  MPI Processes: $NUM_PROCESSES"
    log_info "  Sampling Frequency: $SAMPLING_FREQ Hz"
    log_info "  Output Directory: $OUTPUT_DIR"
    log_info "  Build Directory: $BUILD_DIR"
    log_info "=========================================="
    echo ""
    
    # Check dependencies
    check_dependencies
    
    # Create output directory
    mkdir -p "$OUTPUT_DIR"
    log_info "Created output directory: $OUTPUT_DIR"
    
    # Clean up old output files
    rm -f /tmp/perflow_mpi_rank_*.pflw
    rm -f /tmp/perflow_mpi_rank_*.txt
    
    # List of test programs
    TEST_PROGRAMS=(
        "mpi_sampler_test"
        "test_compute_intensive"
        "test_comm_intensive"
        "test_memory_intensive"
        "test_hybrid"
        "test_stress"
    )
    
    # Test descriptions
    declare -A TEST_DESCRIPTIONS
    TEST_DESCRIPTIONS["mpi_sampler_test"]="Basic MPI Sampler Test"
    TEST_DESCRIPTIONS["test_compute_intensive"]="Compute-Intensive Workload"
    TEST_DESCRIPTIONS["test_comm_intensive"]="Communication-Intensive Workload"
    TEST_DESCRIPTIONS["test_memory_intensive"]="Memory-Intensive Workload"
    TEST_DESCRIPTIONS["test_hybrid"]="Hybrid Workload (Compute + Communication)"
    TEST_DESCRIPTIONS["test_stress"]="Stress Test (High Load)"
    
    # Track results
    declare -A TEST_RESULTS
    TOTAL_TESTS=0
    PASSED_TESTS=0
    FAILED_TESTS=0
    
    # Run each test
    for test_prog in "${TEST_PROGRAMS[@]}"; do
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        
        echo ""
        log_info "=========================================="
        log_info "Running: ${TEST_DESCRIPTIONS[$test_prog]}"
        log_info "Test program: $test_prog"
        log_info "=========================================="
        
        TEST_BINARY="$BUILD_DIR/tests/$test_prog"
        
        if [ ! -f "$TEST_BINARY" ]; then
            log_error "Test binary not found: $TEST_BINARY"
            TEST_RESULTS[$test_prog]="FAILED (binary not found)"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            continue
        fi
        
        # Run the test
        OUTPUT_FILE="$OUTPUT_DIR/${test_prog}_output.txt"
        ERROR_FILE="$OUTPUT_DIR/${test_prog}_error.txt"
        
        START_TIME=$(date +%s)
        
        if [ $VERBOSE -eq 1 ]; then
            LD_PRELOAD="$BUILD_DIR/lib/libperflow_mpi_sampler.so" \
            PERFLOW_OUTPUT_DIR=/tmp \
            PERFLOW_SAMPLING_FREQ=$SAMPLING_FREQ \
            mpirun -n $NUM_PROCESSES "$TEST_BINARY" 2>&1 | tee "$OUTPUT_FILE"
            TEST_EXIT_CODE=${PIPESTATUS[0]}
        else
            LD_PRELOAD="$BUILD_DIR/lib/libperflow_mpi_sampler.so" \
            PERFLOW_OUTPUT_DIR=/tmp \
            PERFLOW_SAMPLING_FREQ=$SAMPLING_FREQ \
            mpirun -n $NUM_PROCESSES "$TEST_BINARY" > "$OUTPUT_FILE" 2> "$ERROR_FILE"
            TEST_EXIT_CODE=$?
        fi
        
        END_TIME=$(date +%s)
        DURATION=$((END_TIME - START_TIME))
        
        if [ $TEST_EXIT_CODE -eq 0 ]; then
            log_info "Test completed successfully in ${DURATION}s"
            TEST_RESULTS[$test_prog]="PASSED (${DURATION}s)"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            
            # Copy output files to results directory
            for rank in $(seq 0 $((NUM_PROCESSES - 1))); do
                if [ -f "/tmp/perflow_mpi_rank_${rank}.pflw" ]; then
                    cp "/tmp/perflow_mpi_rank_${rank}.pflw" \
                       "$OUTPUT_DIR/${test_prog}_rank_${rank}.pflw"
                fi
                if [ -f "/tmp/perflow_mpi_rank_${rank}.txt" ]; then
                    cp "/tmp/perflow_mpi_rank_${rank}.txt" \
                       "$OUTPUT_DIR/${test_prog}_rank_${rank}.txt"
                fi
            done
            
            # Clean up temporary files
            rm -f /tmp/perflow_mpi_rank_*.pflw
            rm -f /tmp/perflow_mpi_rank_*.txt
        else
            log_error "Test failed with exit code $TEST_EXIT_CODE"
            TEST_RESULTS[$test_prog]="FAILED (exit code $TEST_EXIT_CODE)"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            
            if [ $VERBOSE -eq 0 ]; then
                log_error "Error output:"
                cat "$ERROR_FILE"
            fi
        fi
    done
    
    # ========================================================================
    # Run Validation Script
    # ========================================================================
    
    if [ $RUN_VALIDATION -eq 1 ] && [ -f "tests/validate_sampler_results.py" ]; then
        echo ""
        log_info "=========================================="
        log_info "Running validation script..."
        log_info "=========================================="
        
        if python3 tests/validate_sampler_results.py "$OUTPUT_DIR" > "$OUTPUT_DIR/validation_report.txt"; then
            log_info "Validation completed successfully"
            if [ $VERBOSE -eq 1 ]; then
                cat "$OUTPUT_DIR/validation_report.txt"
            fi
        else
            log_warning "Validation script encountered issues"
            cat "$OUTPUT_DIR/validation_report.txt"
        fi
    fi
    
    # ========================================================================
    # Generate Summary Report
    # ========================================================================
    
    echo ""
    log_info "=========================================="
    log_info "Test Summary"
    log_info "=========================================="
    
    for test_prog in "${TEST_PROGRAMS[@]}"; do
        result="${TEST_RESULTS[$test_prog]}"
        if [[ $result == PASSED* ]]; then
            echo -e "${GREEN}✓${NC} ${TEST_DESCRIPTIONS[$test_prog]}: $result"
        else
            echo -e "${RED}✗${NC} ${TEST_DESCRIPTIONS[$test_prog]}: $result"
        fi
    done
    
    echo ""
    log_info "Total: $TOTAL_TESTS tests"
    log_info "Passed: $PASSED_TESTS tests"
    log_info "Failed: $FAILED_TESTS tests"
    log_info "Output directory: $OUTPUT_DIR"
    echo ""
    
    if [ $FAILED_TESTS -eq 0 ]; then
        log_info "All tests passed! ✓"
        exit 0
    else
        log_error "Some tests failed."
        exit 1
    fi
}

# Run main function
main
