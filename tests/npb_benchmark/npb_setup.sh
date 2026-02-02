#!/bin/bash
# Copyright 2024 PerFlow Authors
# Licensed under the Apache License, Version 2.0

# NPB Compilation Setup Script
# This script clones NPB 3.4 MPI repository and compiles all NPB programs

set -e

# ============================================================================
# Configuration
# ============================================================================

NPB_REPO_URL="https://github.com/llnl/NPB.git"
NPB_COMMIT="35cd0e4a895da7dea0316fac34b4da9ab5d7cba5"
NPB_DIR="NPB"
NPB_MPI_DIR="NPB3.4/NPB3.4-MPI"
WORK_DIR="${NPB_WORK_DIR:-$(pwd)/npb_workspace}"

# Default NPB benchmarks and classes
NPB_BENCHMARKS="${NPB_BENCHMARKS:-bt cg ep ft is lu mg sp}"
NPB_CLASS="${NPB_CLASS:-C}"

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

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

NPB Compilation Setup Script

OPTIONS:
    -w DIR      Working directory for NPB (default: ./npb_workspace)
    -b LIST     Space-separated list of benchmarks (default: bt cg ep ft is lu mg sp)
    -c CLASS    Problem class (A, B, C, D, E, F) (default: C)
    -h          Show this help message

EXAMPLES:
    # Compile all default benchmarks with class C
    $0

    # Compile specific benchmarks with class B
    $0 -b "cg ep ft" -c B

    # Use custom working directory
    $0 -w /scratch/npb_build

ENVIRONMENT VARIABLES:
    NPB_WORK_DIR    Working directory for NPB (overridden by -w)
    NPB_BENCHMARKS  Benchmarks to compile (overridden by -b)
    NPB_CLASS       Problem class (overridden by -c)

EOF
    exit 1
}

# ============================================================================
# Parse Command Line Arguments
# ============================================================================

while getopts "w:b:c:h" opt; do
    case $opt in
        w)
            WORK_DIR="$OPTARG"
            ;;
        b)
            NPB_BENCHMARKS="$OPTARG"
            ;;
        c)
            NPB_CLASS="$OPTARG"
            ;;
        h)
            usage
            ;;
        \?)
            log_error "Invalid option: -$OPTARG"
            usage
            ;;
    esac
done

# ============================================================================
# Main Script
# ============================================================================

log_info "NPB Compilation Setup"
log_info "Working directory: $WORK_DIR"
log_info "Benchmarks: $NPB_BENCHMARKS"
log_info "Problem class: $NPB_CLASS"
echo ""

# Create working directory
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# Clone NPB repository if not exists
if [ ! -d "$NPB_DIR" ]; then
    log_info "Cloning NPB repository..."
    git clone "$NPB_REPO_URL" "$NPB_DIR"
    cd "$NPB_DIR"
    git checkout "$NPB_COMMIT"
    cd ..
    log_success "NPB repository cloned successfully"
else
    log_info "NPB repository already exists, skipping clone"
fi

# Navigate to NPB-MPI directory
NPB_PATH="$WORK_DIR/$NPB_DIR/$NPB_MPI_DIR"
if [ ! -d "$NPB_PATH" ]; then
    log_error "NPB-MPI directory not found: $NPB_PATH"
    exit 1
fi

cd "$NPB_PATH"
log_info "NPB-MPI directory: $NPB_PATH"

# Create make.def configuration file
log_info "Creating make.def configuration..."
cat > config/make.def << 'EOF'
#---------------------------------------------------------------------------
# Items in this file will need to be changed for each platform.
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# Parallel Fortran:
#
# For CG, EP, FT, MG, LU, SP, BT and UA, which are in Fortran, the following
# must be defined:
#
# F77        - Fortran compiler
# FFLAGS     - Fortran compilation arguments
# F_INC      - any -I arguments required for compiling Fortran 
# FLINK      - Fortran linker
# FLINKFLAGS - Fortran linker arguments
# F_LIB      - any -L and -l arguments required for linking Fortran 
# 
# compilations are done with $(F77) $(F_INC) $(FFLAGS) or
#                            $(F77) $(FFLAGS)
# linking is done with       $(FLINK) $(F_LIB) $(FLINKFLAGS)
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# This is the fortran compiler used for MPI programs
#---------------------------------------------------------------------------
F77 = mpif77
# This links MPI fortran programs; usually the same as ${F77}
FLINK	= $(F77)

#---------------------------------------------------------------------------
# These macros are passed to the linker 
#---------------------------------------------------------------------------
F_LIB  =

#---------------------------------------------------------------------------
# These macros are passed to the compiler 
#---------------------------------------------------------------------------
F_INC =

#---------------------------------------------------------------------------
# Global *compile time* flags for Fortran programs
#---------------------------------------------------------------------------
FFLAGS	= -O3 -fallow-argument-mismatch

#---------------------------------------------------------------------------
# Global *link time* flags. Flags for increasing maximum executable 
# size usually go here. 
#---------------------------------------------------------------------------
FLINKFLAGS = -O3 -fallow-argument-mismatch


#---------------------------------------------------------------------------
# Parallel C:
#
# For IS and DT, which are in C, the following must be defined:
#
# CC         - C compiler 
# CFLAGS     - C compilation arguments
# C_INC      - any -I arguments required for compiling C 
# CLINK      - C linker
# CLINKFLAGS - C linker flags
# C_LIB      - any -L and -l arguments required for linking C 
#
# compilations are done with $(CC) $(C_INC) $(CFLAGS) or
#                            $(CC) $(CFLAGS)
# linking is done with       $(CLINK) $(C_LIB) $(CLINKFLAGS)
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# This is the C compiler used for MPI programs
#---------------------------------------------------------------------------
CC = mpicc
# This links MPI C programs; usually the same as ${CC}
CLINK	= $(CC)

#---------------------------------------------------------------------------
# These macros are passed to the linker 
#---------------------------------------------------------------------------
C_LIB  = -lm

#---------------------------------------------------------------------------
# These macros are passed to the compiler 
#---------------------------------------------------------------------------
C_INC =

#---------------------------------------------------------------------------
# Global *compile time* flags for C programs
#---------------------------------------------------------------------------
CFLAGS	= -O3 -fallow-argument-mismatch

#---------------------------------------------------------------------------
# Global *link time* flags. Flags for increasing maximum executable 
# size usually go here. 
#---------------------------------------------------------------------------
CLINKFLAGS = -O3 -fallow-argument-mismatch


#---------------------------------------------------------------------------
# Utilities C:
#
# This is the C compiler used to compile C utilities.  Flags required by 
# this compiler go here also; typically there are few flags required; hence 
# there are no separate macros provided for such flags.
#---------------------------------------------------------------------------
UCC	= gcc
UCFLAGS = -O3


#---------------------------------------------------------------------------
# Destination of executables, relative to subdirs of the main directory. . 
#---------------------------------------------------------------------------
BINDIR	= ../bin


#---------------------------------------------------------------------------
# The variable RAND controls which random number generator 
# is used. It is described in detail in README.install. 
# Use "ranka" unless there is a reason to use another one. 
# Other allowed values are "randdp", "randdpvec", "randlc", and "ranlc"
#---------------------------------------------------------------------------
RAND   = ranka
# The following is highly reliable but may be slow:
# RAND   = randdp


#---------------------------------------------------------------------------
# The variable WTIME is the name of the wtime source code module in the
# common directory.  
# For most machines,       use wtime.c
# For SGI power challenge: use wtime_sgi64.c
#---------------------------------------------------------------------------
WTIME  = wtime.c

EOF

log_success "make.def created successfully"

# Compile benchmarks
log_info "Compiling NPB benchmarks..."
COMPILED_COUNT=0
FAILED_BENCHMARKS=""

for benchmark in $NPB_BENCHMARKS; do
    BENCHMARK_UPPER=$(echo "$benchmark" | tr '[:lower:]' '[:upper:]')
    log_info "Compiling ${BENCHMARK_UPPER}.${NPB_CLASS}..."
    
    if make "${benchmark}" CLASS="${NPB_CLASS}" 2>&1 | tee "/tmp/npb_compile_${benchmark}.log"; then
        if [ -f "bin/${benchmark}.${NPB_CLASS}.x" ]; then
            log_success "${BENCHMARK_UPPER}.${NPB_CLASS} compiled successfully"
            COMPILED_COUNT=$((COMPILED_COUNT + 1))
        else
            log_warning "${BENCHMARK_UPPER}.${NPB_CLASS} compilation completed but binary not found"
            FAILED_BENCHMARKS="$FAILED_BENCHMARKS $benchmark"
        fi
    else
        log_error "${BENCHMARK_UPPER}.${NPB_CLASS} compilation failed"
        FAILED_BENCHMARKS="$FAILED_BENCHMARKS $benchmark"
    fi
    echo ""
done

# Summary
echo ""
log_info "=================================="
log_info "NPB Compilation Summary"
log_info "=================================="
log_info "Successfully compiled: $COMPILED_COUNT benchmarks"

if [ -n "$FAILED_BENCHMARKS" ]; then
    log_warning "Failed benchmarks:$FAILED_BENCHMARKS"
fi

# List compiled binaries
log_info "Compiled binaries:"
ls -lh bin/*.x 2>/dev/null || log_warning "No binaries found in bin/"

# Save compilation info
cat > "$WORK_DIR/npb_compilation_info.txt" << EOF
NPB Compilation Information
===========================
Date: $(date)
NPB Path: $NPB_PATH
Benchmarks: $NPB_BENCHMARKS
Problem Class: $NPB_CLASS
Compiled Count: $COMPILED_COUNT

Compiled Binaries:
EOF

ls -lh "$NPB_PATH/bin/"*.x 2>/dev/null >> "$WORK_DIR/npb_compilation_info.txt" || echo "None" >> "$WORK_DIR/npb_compilation_info.txt"

log_success "Compilation complete! Binary location: $NPB_PATH/bin/"
log_info "Compilation info saved to: $WORK_DIR/npb_compilation_info.txt"

exit 0
