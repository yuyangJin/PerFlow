#!/bin/bash
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# NPB Benchmark Test - Main Orchestration Script
# One-click execution for comprehensive NPB benchmark testing with PerFlow

set -e

# ============================================================================
# Configuration
# ============================================================================

# Default paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${NPB_WORK_DIR:-$(pwd)/npb_workspace}"
RESULTS_DIR="${NPB_RESULTS_DIR:-$(pwd)/npb_results}"
PERFLOW_BUILD_DIR="${PERFLOW_BUILD_DIR:-${SCRIPT_DIR}/../../build}"

# Default benchmark configuration
NPB_BENCHMARKS="${NPB_BENCHMARKS:-bt cg ep ft is lu mg sp}"
NPB_CLASS="${NPB_CLASS:-C}"
PROCESS_SCALES="${NPB_PROCESS_SCALES:-1 4 16 64 128 256 512}"
PROCESS_SCALES_SQUARE="${NPB_PROCESS_SCALES_SQUARE:-1 4 9 16 36 64 121 256 484}"
NUM_ITERATIONS="${NUM_ITERATIONS:-3}"
SAMPLING_FREQ="${PERFLOW_SAMPLING_FREQ:-1000}"

# Execution options
USE_SLURM="${USE_SLURM:-0}"
SLURM_PARTITION="${SLURM_PARTITION:-ja}"
CLEANUP="${CLEANUP:-0}"

# Task control
RUN_SETUP="${RUN_SETUP:-1}"
RUN_BASELINE="${RUN_BASELINE:-1}"
RUN_OVERHEAD="${RUN_OVERHEAD:-1}"
RUN_HOTSPOT="${RUN_HOTSPOT:-1}"
RUN_VISUALIZE="${RUN_VISUALIZE:-1}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ============================================================================
# Functions
# ============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_task() {
    echo ""
    echo -e "${CYAN}============================================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}============================================================${NC}"
    echo ""
}

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

NPB Benchmark Test - Main Orchestration Script

One-click execution for comprehensive NPB benchmark testing with PerFlow's MPI sampler.
This script orchestrates all tasks from setup to visualization.

OPTIONS:
  General:
    -h, --help          Show this help message
    
  Directories:
    -w DIR              Working directory for NPB (default: ./npb_workspace)
    -r DIR              Results directory (default: ./npb_results)
    -d DIR              PerFlow build directory (default: ../../build)
    
  Benchmark Configuration:
    -b BENCHMARKS       Space-separated list of NPB benchmarks
                        (default: "bt cg ep ft is lu mg sp")
    -c CLASS            NPB problem class: A, B, C, D, E, F (default: C)
    -s SCALES           Space-separated process scales
                        (default: "1 4 16 64 128 256 512")
                        Note: BT and SP benchmarks automatically use square scales:
                        "1 4 9 16 36 64 121 256 484"
    -i NUM              Number of iterations per test (default: 3)
    -f FREQ             Sampling frequency in Hz (default: 1000)
    
  Execution:
    --slurm             Use SLURM for job submission
    -p PARTITION        SLURM partition name (default: ja)
    
  Task Control:
    --skip-setup        Skip NPB compilation setup
    --skip-baseline     Skip baseline measurement
    --skip-overhead     Skip overhead analysis
    --skip-hotspot      Skip hotspot analysis
    --skip-visualize    Skip visualization generation
    --only-setup        Only run setup task
    --only-baseline     Only run baseline task
    --only-overhead     Only run overhead task
    --only-hotspot      Only run hotspot task
    --only-visualize    Only run visualization task
    
  Cleanup:
    --cleanup           Remove intermediate files after completion

EXAMPLES:
  # Run full benchmark suite with defaults
  $0

  # Run specific benchmarks with custom parameters
  $0 -b "cg ep ft" -c B -s "4 16 64"

  # Run with SLURM on specific partition
  $0 --slurm -p compute

  # Run with higher sampling frequency
  $0 -f 5000

  # Only regenerate visualizations from existing data
  $0 --only-visualize

  # Full run with cleanup
  $0 --cleanup

ENVIRONMENT VARIABLES:
  NPB_WORK_DIR          Working directory (overridden by -w)
  NPB_RESULTS_DIR       Results directory (overridden by -r)
  PERFLOW_BUILD_DIR     PerFlow build directory (overridden by -d)
  NPB_BENCHMARKS        Benchmarks to run (overridden by -b)
  NPB_CLASS             Problem class (overridden by -c)
  NPB_PROCESS_SCALES    Process scales (overridden by -s)
  NPB_PROCESS_SCALES_SQUARE  Process scales for BT/SP (default: "1 4 9 16 36 64 121 256 484")
  NUM_ITERATIONS        Number of iterations (overridden by -i)
  PERFLOW_SAMPLING_FREQ Sampling frequency (overridden by -f)
  USE_SLURM             Use SLURM: 0 or 1 (overridden by --slurm)
  SLURM_PARTITION       SLURM partition (overridden by -p)
  CLEANUP               Cleanup flag: 0 or 1 (overridden by --cleanup)

WORKFLOW:
  1. Setup: Clone and compile NPB-MPI benchmarks
  2. Baseline: Run benchmarks without PerFlow sampler
  3. Overhead: Run benchmarks with PerFlow sampler
  4. Hotspot: Analyze performance sampling data
  5. Visualize: Generate charts and reports

OUTPUT FILES:
  - baseline_results.csv/json    Baseline performance data
  - overhead_results.csv/json    Overhead analysis data
  - hotspot_analysis.json        Hotspot analysis results
  - overhead_analysis.png        Overhead line chart
  - hotspot_analysis_*.png       Hotspot distribution charts
  - summary_report.txt           Comprehensive text report

EOF
    exit 0
}

# ============================================================================
# Parse Command Line Arguments
# ============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            ;;
        -w)
            WORK_DIR="$2"
            shift 2
            ;;
        -r)
            RESULTS_DIR="$2"
            shift 2
            ;;
        -d)
            PERFLOW_BUILD_DIR="$2"
            shift 2
            ;;
        -b)
            NPB_BENCHMARKS="$2"
            shift 2
            ;;
        -c)
            NPB_CLASS="$2"
            shift 2
            ;;
        -s)
            PROCESS_SCALES="$2"
            shift 2
            ;;
        -i)
            NUM_ITERATIONS="$2"
            shift 2
            ;;
        -f)
            SAMPLING_FREQ="$2"
            shift 2
            ;;
        --slurm)
            USE_SLURM=1
            shift
            ;;
        -p)
            SLURM_PARTITION="$2"
            shift 2
            ;;
        --cleanup)
            CLEANUP=1
            shift
            ;;
        --skip-setup)
            RUN_SETUP=0
            shift
            ;;
        --skip-baseline)
            RUN_BASELINE=0
            shift
            ;;
        --skip-overhead)
            RUN_OVERHEAD=0
            shift
            ;;
        --skip-hotspot)
            RUN_HOTSPOT=0
            shift
            ;;
        --skip-visualize)
            RUN_VISUALIZE=0
            shift
            ;;
        --only-setup)
            RUN_SETUP=1
            RUN_BASELINE=0
            RUN_OVERHEAD=0
            RUN_HOTSPOT=0
            RUN_VISUALIZE=0
            shift
            ;;
        --only-baseline)
            RUN_SETUP=0
            RUN_BASELINE=1
            RUN_OVERHEAD=0
            RUN_HOTSPOT=0
            RUN_VISUALIZE=0
            shift
            ;;
        --only-overhead)
            RUN_SETUP=0
            RUN_BASELINE=0
            RUN_OVERHEAD=1
            RUN_HOTSPOT=0
            RUN_VISUALIZE=0
            shift
            ;;
        --only-hotspot)
            RUN_SETUP=0
            RUN_BASELINE=0
            RUN_OVERHEAD=0
            RUN_HOTSPOT=1
            RUN_VISUALIZE=0
            shift
            ;;
        --only-visualize)
            RUN_SETUP=0
            RUN_BASELINE=0
            RUN_OVERHEAD=0
            RUN_HOTSPOT=0
            RUN_VISUALIZE=1
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            ;;
    esac
done

# ============================================================================
# Main Execution
# ============================================================================

# Print banner
echo ""
echo -e "${CYAN}################################################################${NC}"
echo -e "${CYAN}#                                                              #${NC}"
echo -e "${CYAN}#         NPB Benchmark Test for PerFlow MPI Sampler          #${NC}"
echo -e "${CYAN}#                                                              #${NC}"
echo -e "${CYAN}################################################################${NC}"
echo ""

# Display configuration
log_info "Configuration:"
log_info "  Working directory:      $WORK_DIR"
log_info "  Results directory:      $RESULTS_DIR"
log_info "  PerFlow build dir:      $PERFLOW_BUILD_DIR"
log_info "  Benchmarks:             $NPB_BENCHMARKS"
log_info "  Problem class:          $NPB_CLASS"
log_info "  Process scales:         $PROCESS_SCALES"
log_info "  Iterations per test:    $NUM_ITERATIONS"
log_info "  Sampling frequency:     ${SAMPLING_FREQ} Hz"
log_info "  Use SLURM:              $USE_SLURM"
if [ "$USE_SLURM" -eq 1 ]; then
    log_info "  SLURM partition:        $SLURM_PARTITION"
fi
log_info "  Cleanup:                $CLEANUP"

# Export environment variables for subscripts
export NPB_WORK_DIR="$WORK_DIR"
export NPB_RESULTS_DIR="$RESULTS_DIR"
export PERFLOW_BUILD_DIR="$PERFLOW_BUILD_DIR"
export NPB_BENCHMARKS="$NPB_BENCHMARKS"
export NPB_CLASS="$NPB_CLASS"
export NPB_PROCESS_SCALES="$PROCESS_SCALES"
export NPB_PROCESS_SCALES_SQUARE="$PROCESS_SCALES_SQUARE"
export NUM_ITERATIONS="$NUM_ITERATIONS"
export PERFLOW_SAMPLING_FREQ="$SAMPLING_FREQ"
export USE_SLURM="$USE_SLURM"
export SLURM_PARTITION="$SLURM_PARTITION"

# Create results directory
mkdir -p "$RESULTS_DIR"

# Track start time
START_TIME=$(date +%s)

# ============================================================================
# Task 1: NPB Compilation Setup
# ============================================================================

if [ "$RUN_SETUP" -eq 1 ]; then
    log_task "TASK 1: NPB Compilation Setup"
    
    if ! bash "$SCRIPT_DIR/npb_setup.sh"; then
        log_error "NPB setup failed"
        exit 1
    fi
    
    log_success "Task 1 completed successfully"
fi

# ============================================================================
# Task 2: Baseline Performance Measurement
# ============================================================================

if [ "$RUN_BASELINE" -eq 1 ]; then
    log_task "TASK 2: Baseline Performance Measurement"
    
    if ! bash "$SCRIPT_DIR/npb_baseline.sh"; then
        log_error "Baseline measurement failed"
        exit 1
    fi
    
    log_success "Task 2 completed successfully"
fi

# ============================================================================
# Task 3: Overhead Analysis
# ============================================================================

if [ "$RUN_OVERHEAD" -eq 1 ]; then
    log_task "TASK 3: Overhead Analysis (with PerFlow Sampler)"
    
    if ! bash "$SCRIPT_DIR/npb_overhead.sh"; then
        log_error "Overhead analysis failed"
        exit 1
    fi
    
    log_success "Task 3 completed successfully"
fi

# ============================================================================
# Task 4: Hotspot Analysis
# ============================================================================

if [ "$RUN_HOTSPOT" -eq 1 ]; then
    log_task "TASK 4: Hotspot Analysis"
    
    if ! python3 "$SCRIPT_DIR/npb_hotspot_analysis.py" "$RESULTS_DIR"; then
        log_error "Hotspot analysis failed"
        exit 1
    fi
    
    log_success "Task 4 completed successfully"
fi

# ============================================================================
# Task 5: Visualization
# ============================================================================

if [ "$RUN_VISUALIZE" -eq 1 ]; then
    log_task "TASK 5: Visualization Generation"
    
    if ! python3 "$SCRIPT_DIR/npb_visualize.py" "$RESULTS_DIR"; then
        log_warning "Visualization generation had issues (matplotlib may not be available)"
    else
        log_success "Task 5 completed successfully"
    fi
fi

# ============================================================================
# Cleanup
# ============================================================================

if [ "$CLEANUP" -eq 1 ]; then
    log_task "CLEANUP: Removing Intermediate Files"
    
    # Remove individual output files but keep summary data
    log_info "Removing individual benchmark output files..."
    rm -f "$RESULTS_DIR"/baseline/*.out 2>/dev/null || true
    rm -f "$RESULTS_DIR"/overhead/*.out 2>/dev/null || true
    
    # Optionally remove sample data (large files)
    read -p "Remove sample data files? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log_info "Removing sample data..."
        rm -rf "$RESULTS_DIR"/overhead/sample_data/* 2>/dev/null || true
    fi
    
    log_success "Cleanup completed"
fi

# ============================================================================
# Final Summary
# ============================================================================

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
ELAPSED_MIN=$((ELAPSED / 60))
ELAPSED_SEC=$((ELAPSED % 60))

echo ""
log_task "EXECUTION COMPLETE"
log_success "Total execution time: ${ELAPSED_MIN}m ${ELAPSED_SEC}s"
echo ""
log_info "Results are available in: $RESULTS_DIR"
log_info ""
log_info "Generated files:"
log_info "  - baseline/baseline_results.csv       Baseline performance data"
log_info "  - baseline/baseline_results.json      Baseline performance (JSON)"
log_info "  - overhead/overhead_results.csv       Overhead analysis data"
log_info "  - overhead/overhead_results.json      Overhead analysis (JSON)"
log_info "  - hotspot_analysis.json               Hotspot analysis results"
log_info "  - overhead_analysis.png               Overhead visualization"
log_info "  - hotspot_analysis_*.png              Hotspot visualizations"
log_info "  - summary_report.txt                  Text summary report"
echo ""

# Display summary report if it exists
if [ -f "$RESULTS_DIR/summary_report.txt" ]; then
    log_info "Summary Report Preview:"
    echo ""
    head -n 50 "$RESULTS_DIR/summary_report.txt"
    echo ""
    log_info "Full report: $RESULTS_DIR/summary_report.txt"
fi

log_success "NPB Benchmark Test completed successfully!"
echo ""

exit 0
