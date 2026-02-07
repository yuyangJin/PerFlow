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
cat > config/make.def << EOF
#---------------------------------------------------------------------------
# Items in this file will need to be changed for each platform.
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# Parallel Fortran:
#
# For CG, EP, FT, MG, LU, SP and BT, which are in Fortran, the following must 
# be defined:
#
# MPIFC      - Fortran compiler
# FFLAGS     - Fortran compilation arguments
# FMPI_INC   - any -I arguments required for compiling MPI/Fortran 
# FLINK      - Fortran linker
# FLINKFLAGS - Fortran linker arguments
# FMPI_LIB   - any -L and -l arguments required for linking MPI/Fortran 
# 
# compilations are done with \$(MPIFC) \$(FMPI_INC) \$(FFLAGS) or
#                            \$(MPIFC) \$(FFLAGS)
# linking is done with       \$(FLINK) \$(FMPI_LIB) \$(FLINKFLAGS)
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# This is the fortran compiler used for MPI programs
#---------------------------------------------------------------------------
MPIFC = mpifort -g
# This links MPI fortran programs; usually the same as \${MPIFC}
FLINK	= \$(MPIFC)

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
FFLAGS	= -O3 -fallow-argument-mismatch -fallow-invalid-boz

#---------------------------------------------------------------------------
# Global *link time* flags. Flags for increasing maximum executable 
# size usually go here. 
#---------------------------------------------------------------------------
FLINKFLAGS = \$(FFLAGS)


#---------------------------------------------------------------------------
# Parallel C:
#
# For IS and DT, which are in C, the following must be defined:
#
# MPICC         - C compiler 
# CFLAGS     - C compilation arguments
# CMPI_INC      - any -I arguments required for compiling C 
# CLINK      - C linker
# CLINKFLAGS - C linker flags
# CMPI_LIB      - any -L and -l arguments required for linking C 
#
# compilations are done with \$(MPICC) \$(CMPI_INC) \$(CFLAGS) or
#                            \$(MPICC) \$(CFLAGS)
# linking is done with       \$(CLINK) \$(CMPI_LIB) \$(CLINKFLAGS)
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# This is the C compiler used for MPI programs
#---------------------------------------------------------------------------
MPICC = mpicc -g
# This links MPI C programs; usually the same as \${MPICC}
CLINK	= \$(MPICC)

#---------------------------------------------------------------------------
# These macros are passed to the linker 
#---------------------------------------------------------------------------
CMPI_LIB  = -lm

#---------------------------------------------------------------------------
# These macros are passed to the compiler 
#---------------------------------------------------------------------------
CMPI_INC =

#---------------------------------------------------------------------------
# Global *compile time* flags for C programs
#---------------------------------------------------------------------------
CFLAGS	= -O3 -fallow-argument-mismatch -fallow-invalid-boz

#---------------------------------------------------------------------------
# Global *link time* flags. Flags for increasing maximum executable 
# size usually go here. 
#---------------------------------------------------------------------------
CLINKFLAGS = \$(CFLAGS)

#---------------------------------------------------------------------------
# MPI dummy library:
#
# Uncomment if you want to use the MPI dummy library supplied by NAS instead 
# of the true message-passing library. The include file redefines several of
# the above macros. It also invokes make in subdirectory MPI_dummy. Make 
# sure that no spaces or tabs precede include.
#---------------------------------------------------------------------------
# include ../config/make.dummy

#---------------------------------------------------------------------------
# Utilities C:
#
# This is the C compiler used to compile C utilities.  Flags required by 
# this compiler go here also; typically there are few flags required; hence 
# there are no separate macros provided for such flags.
#---------------------------------------------------------------------------
CC	= gcc -g
UCFLAGS = -O3


#---------------------------------------------------------------------------
# Destination of executables, relative to subdirs of the main directory. . 
#---------------------------------------------------------------------------
BINDIR	= ${WORK_DIR}/MPI/bin

#---------------------------------------------------------------------------
# Some machines (e.g. Crays) have 128-bit DOUBLE PRECISION numbers, which
# is twice the precision required for the NPB suite. A compiler flag 
# (e.g. -dp) can usually be used to change DOUBLE PRECISION variables to
# 64 bits, but the MPI library may continue to send 128 bits. Short of
# recompiling MPI, the solution is to use MPI_REAL to send these 64-bit
# numbers, and MPI_COMPLEX to send their complex counterparts. Uncomment
# the following line to enable this substitution. 
# 
# NOTE: IF THE I/O BENCHMARK IS BEING BUILT, WE USE CONVERTFLAG TO
#       SPECIFIY THE FORTRAN RECORD LENGTH UNIT. IT IS A SYSTEM-SPECIFIC
#       VALUE (USUALLY 1 OR 4). UNCOMMENT THE SECOND LINE AND SUBSTITUTE
#       THE CORRECT VALUE FOR "length".
#       IF BOTH 128-BIT DOUBLE PRECISION NUMBERS AND I/O ARE TO BE ENABLED,
#       UNCOMMENT THE THIRD LINE AND SUBSTITUTE THE CORRECT VALUE FOR
#       "length"
#---------------------------------------------------------------------------
# CONVERTFLAG	= -DCONVERTDOUBLE
# CONVERTFLAG	= -DFORTRAN_REC_SIZE=length
# CONVERTFLAG	= -DCONVERTDOUBLE -DFORTRAN_REC_SIZE=length
CONVERTFLAG	= -DFORTRAN_REC_SIZE=1

#---------------------------------------------------------------------------
# The variable RAND controls which random number generator 
# is used. It is described in detail in README.install. 
# Use "randi8" unless there is a reason to use another one. 
# Other allowed values are "randi8_safe", "randdp" and "randdpvec"
#---------------------------------------------------------------------------
RAND   = randi8
# The following is highly reliable but may be slow:
# RAND   = randdp


EOF

log_success "make.def created successfully"

# Create BINDIR if it doesn't exist
mkdir -p "${WORK_DIR}/MPI/bin"

# Compile benchmarks
log_info "Compiling NPB benchmarks..."
COMPILED_COUNT=0
FAILED_BENCHMARKS=""

for benchmark in $NPB_BENCHMARKS; do
    BENCHMARK_UPPER=$(echo "$benchmark" | tr '[:lower:]' '[:upper:]')
    log_info "Compiling ${BENCHMARK_UPPER}.${NPB_CLASS}..."
    
    if make "${benchmark}" CLASS="${NPB_CLASS} -j12" 2>&1 | tee "/tmp/npb_compile_${benchmark}.log"; then
        if [ -f "${WORK_DIR}/MPI/bin/${benchmark}.${NPB_CLASS}.x" ]; then
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
ls -lh "${WORK_DIR}/MPI/bin/"*.x 2>/dev/null || log_warning "No binaries found in ${WORK_DIR}/MPI/bin/"

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

ls -lh "${WORK_DIR}/MPI/bin/"*.x 2>/dev/null >> "$WORK_DIR/npb_compilation_info.txt" || echo "None" >> "$WORK_DIR/npb_compilation_info.txt"

log_success "Compilation complete! Binary location: ${WORK_DIR}/MPI/bin/"
log_info "Compilation info saved to: $WORK_DIR/npb_compilation_info.txt"

exit 0
