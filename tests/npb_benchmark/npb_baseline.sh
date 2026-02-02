#!/bin/bash
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# NPB Baseline Performance Measurement Script
# This script runs NPB benchmarks without PerFlow sampler to collect baseline performance

set -e

# ============================================================================
# Configuration
# ============================================================================

WORK_DIR="${NPB_WORK_DIR:-$(pwd)/npb_workspace}"
NPB_DIR="NPB"
NPB_MPI_DIR="NPB3.4/NPB3.4-MPI"
RESULTS_DIR="${NPB_RESULTS_DIR:-$(pwd)/npb_results}"

# Default configuration
NPB_BENCHMARKS="${NPB_BENCHMARKS:-bt cg ep ft is lu mg sp}"
NPB_CLASS="${NPB_CLASS:-C}"
PROCESS_SCALES="${NPB_PROCESS_SCALES:-1 4 16 64 128 256 512}"
PROCESS_SCALES_SQUARE="${NPB_PROCESS_SCALES_SQUARE:-1 4 9 16 36 64 121 256 484}"
USE_SLURM="${USE_SLURM:-0}"
SLURM_PARTITION="${SLURM_PARTITION:-ja}"
NUM_ITERATIONS="${NUM_ITERATIONS:-1}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

get_process_scales() {
    local benchmark="$1"
    # BT and SP require perfect square number of processes
    if [ "$benchmark" = "bt" ] || [ "$benchmark" = "sp" ]; then
        echo "$PROCESS_SCALES_SQUARE"
    else
        echo "$PROCESS_SCALES"
    fi
}

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

NPB Baseline Performance Measurement Script

OPTIONS:
    -w DIR      Working directory for NPB (default: ./npb_workspace)
    -r DIR      Results directory (default: ./npb_results)
    -b LIST     Space-separated list of benchmarks (default: bt cg ep ft is lu mg sp)
    -c CLASS    Problem class (default: C)
    -s SCALES   Space-separated list of process scales (default: "1 4 16 64 128 256 512")
                Note: BT and SP benchmarks automatically use square scales: "1 4 9 16 36 64 121 256 484"
    -i NUM      Number of iterations per benchmark (default: 3)
    --slurm     Use SLURM for job submission
    -p PART     SLURM partition (default: ja)
    -h          Show this help message

EXAMPLES:
    # Run with default settings
    $0

    # Run specific benchmarks with SLURM
    $0 -b "cg ep ft" --slurm -p compute

    # Run with custom process scales
    $0 -s "4 16 64"

ENVIRONMENT VARIABLES:
    NPB_WORK_DIR            Working directory
    NPB_RESULTS_DIR         Results directory
    NPB_BENCHMARKS          Benchmarks to run
    NPB_CLASS               Problem class
    NPB_PROCESS_SCALES      Process scales (for most benchmarks)
    NPB_PROCESS_SCALES_SQUARE  Process scales for BT/SP (default: "1 4 9 16 36 64 121 256 484")
    USE_SLURM               Use SLURM (0 or 1)
    SLURM_PARTITION         SLURM partition name

EOF
    exit 1
}

# ============================================================================
# Parse Command Line Arguments
# ============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        -w)
            WORK_DIR="$2"
            shift 2
            ;;
        -r)
            RESULTS_DIR="$2"
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
        --slurm)
            USE_SLURM=1
            shift
            ;;
        -p)
            SLURM_PARTITION="$2"
            shift 2
            ;;
        -h)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            ;;
    esac
done

# ============================================================================
# Main Script
# ============================================================================

log_info "NPB Baseline Performance Measurement"
log_info "Working directory: $WORK_DIR"
log_info "Results directory: $RESULTS_DIR"
log_info "Benchmarks: $NPB_BENCHMARKS"
log_info "Problem class: $NPB_CLASS"
log_info "Process scales: $PROCESS_SCALES"
log_info "Iterations per benchmark: $NUM_ITERATIONS"
log_info "Use SLURM: $USE_SLURM"
if [ "$USE_SLURM" -eq 1 ]; then
    log_info "SLURM partition: $SLURM_PARTITION"
fi
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR/baseline"

# Check if NPB binaries exist
if [ ! -d "$WORK_DIR/MPI/bin" ]; then
    log_error "NPB binaries not found at $WORK_DIR/MPI/bin"
    log_error "Please run npb_setup.sh first"
    exit 1
fi

# Initialize CSV file for results
CSV_FILE="$RESULTS_DIR/baseline/baseline_results.csv"
echo "benchmark,class,num_processes,iteration,execution_time_sec,mop_total,mop_per_sec" > "$CSV_FILE"

# Initialize JSON file for results
JSON_FILE="$RESULTS_DIR/baseline/baseline_results.json"
echo "{" > "$JSON_FILE"
echo "  \"metadata\": {" >> "$JSON_FILE"
echo "    \"date\": \"$(date -Iseconds)\"," >> "$JSON_FILE"
echo "    \"benchmarks\": \"$NPB_BENCHMARKS\"," >> "$JSON_FILE"
echo "    \"class\": \"$NPB_CLASS\"," >> "$JSON_FILE"
echo "    \"process_scales\": \"$PROCESS_SCALES\"," >> "$JSON_FILE"
echo "    \"iterations\": $NUM_ITERATIONS" >> "$JSON_FILE"
echo "  }," >> "$JSON_FILE"
echo "  \"results\": [" >> "$JSON_FILE"

FIRST_RESULT=1

# Run benchmarks
for benchmark in $NPB_BENCHMARKS; do
    BENCHMARK_UPPER=$(echo "$benchmark" | tr '[:lower:]' '[:upper:]')
    BINARY="${WORK_DIR}/MPI/bin/${benchmark}.${NPB_CLASS}.x"
    
    if [ ! -f "$BINARY" ]; then
        log_warning "Binary not found: $BINARY, skipping..."
        continue
    fi
    
    # Get appropriate process scales for this benchmark
    BENCHMARK_PROCESS_SCALES=$(get_process_scales "$benchmark")
    
    log_info "Running benchmark: ${BENCHMARK_UPPER}.${NPB_CLASS}"
    log_info "  Process scales for $BENCHMARK_UPPER: $BENCHMARK_PROCESS_SCALES"
    
    for nprocs in $BENCHMARK_PROCESS_SCALES; do
        log_info "  Process scale: $nprocs"
        
        for iter in $(seq 1 $NUM_ITERATIONS); do
            log_info "    Iteration $iter/$NUM_ITERATIONS"
            
            # Prepare output file
            OUTPUT_FILE="$RESULTS_DIR/baseline/${benchmark}_${NPB_CLASS}_np${nprocs}_iter${iter}.out"
            
            # Run benchmark
            if [ "$USE_SLURM" -eq 1 ]; then
                # SLURM execution
                srun -p "$SLURM_PARTITION" -n "$nprocs" "$BINARY" > "$OUTPUT_FILE" 2>&1
            else
                # Direct MPI execution
                mpirun -n "$nprocs" "$BINARY" > "$OUTPUT_FILE" 2>&1
            fi
            
            # Parse execution time from output
            # NPB benchmarks print "Time in seconds" line
            EXEC_TIME=$(grep -i "Time in seconds" "$OUTPUT_FILE" | awk '{print $5}' | head -1)
            MOP_TOTAL=$(grep -i "Mop/s total" "$OUTPUT_FILE" | awk '{print $4}' | head -1)
            MOP_PER_SEC=$(grep -i "Mop/s/process" "$OUTPUT_FILE" | awk '{print $3}' | head -1)
            
            # Default values if parsing fails
            EXEC_TIME=${EXEC_TIME:-0.0}
            MOP_TOTAL=${MOP_TOTAL:-0.0}
            MOP_PER_SEC=${MOP_PER_SEC:-0.0}
            
            # Write to CSV
            echo "$benchmark,$NPB_CLASS,$nprocs,$iter,$EXEC_TIME,$MOP_TOTAL,$MOP_PER_SEC" >> "$CSV_FILE"
            
            # Write to JSON
            if [ $FIRST_RESULT -eq 0 ]; then
                echo "," >> "$JSON_FILE"
            fi
            FIRST_RESULT=0
            
            echo "    {" >> "$JSON_FILE"
            echo "      \"benchmark\": \"$benchmark\"," >> "$JSON_FILE"
            echo "      \"class\": \"$NPB_CLASS\"," >> "$JSON_FILE"
            echo "      \"num_processes\": $nprocs," >> "$JSON_FILE"
            echo "      \"iteration\": $iter," >> "$JSON_FILE"
            echo "      \"execution_time_sec\": $EXEC_TIME," >> "$JSON_FILE"
            echo "      \"mop_total\": $MOP_TOTAL," >> "$JSON_FILE"
            echo "      \"mop_per_sec\": $MOP_PER_SEC" >> "$JSON_FILE"
            echo "    }" >> "$JSON_FILE"
            
            log_success "      Time: ${EXEC_TIME}s"
        done
    done
    echo ""
done

# Close JSON file
echo "" >> "$JSON_FILE"
echo "  ]" >> "$JSON_FILE"
echo "}" >> "$JSON_FILE"

# Generate summary
log_info "=================================="
log_info "Baseline Measurement Summary"
log_info "=================================="
log_success "Results saved to:"
log_info "  CSV: $CSV_FILE"
log_info "  JSON: $JSON_FILE"
log_info "  Raw outputs: $RESULTS_DIR/baseline/*.out"

# Calculate and display average times
log_info ""
log_info "Average execution times (seconds):"
echo ""
printf "%-10s %-10s %s\n" "Benchmark" "Processes" "Avg Time (s)"
printf "%-10s %-10s %s\n" "----------" "----------" "--------------"

for benchmark in $NPB_BENCHMARKS; do
    BENCHMARK_PROCESS_SCALES=$(get_process_scales "$benchmark")
    for nprocs in $BENCHMARK_PROCESS_SCALES; do
        AVG_TIME=$(awk -F',' -v b="$benchmark" -v n="$nprocs" \
            'NR>1 && $1==b && $3==n {sum+=$5; count++} END {if(count>0) printf "%.3f", sum/count; else print "N/A"}' \
            "$CSV_FILE")
        if [ "$AVG_TIME" != "N/A" ]; then
            printf "%-10s %-10s %s\n" "$benchmark" "$nprocs" "$AVG_TIME"
        fi
    done
done

log_success "Baseline measurement completed successfully!"

exit 0
