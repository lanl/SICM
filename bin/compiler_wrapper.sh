#!/bin/bash
# This wrapper simply calls clang, but instead of outputting
# an object file, it will output a binary LLVM IR format, putting it
# into the .o file that would normally be the object file.
# This wrapper also parses and outputs the arguments used to compile each
# file, so that it can be read and used by the ld_wrapper.

# The path that this script is in
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# The original arguments and the ones we're going to add
ARGS=$@
OUTPUT_ARGS=""
EXTRA_ARGS=""
FILE=""
OUTPUTFILE=""
ONLY_COMPILE=false
LLVMPATH="${LLVMPATH:- }"
C_COMPILER="${C_COMPILER:-clang}"
CXX_COMPILER="${CXX_COMPILER:-clang++}"
FORT_COMPILER="${FORT_COMPILER:-flang}"
LD_WRAPPER="${LD_WRAPPER:-${DIR}/ld_wrapper.sh}"
COMPILER="$C_COMPILER"
PREV=""
FORTRAN=false

# Iterate over all arguments
for word in $ARGS; do

  # Check if the argument is a C file
  # Also don't put this in the output arguments
  if [[ "$word" =~ (.*)\.c$ ]]; then
    FILE=${BASH_REMATCH[1]}
    COMPILER="$C_COMPILER"
  # Or if it's a C++ file
  elif [[ "$word" =~ (.*)\.cpp$ ]] || [[ "$word" =~ (.*)\.C$ ]] || [[ "$word" =~ (.*)\.cc$ ]]; then
    FILE=${BASH_REMATCH[1]}
    COMPILER="$CXX_COMPILER"
  elif [[ "$word" =~ (.*)\.f90$ ]] || [[ "$word" =~ (.*)\.f95$ ]] || [[ "$word" =~ (.*)\.f03$ ]] || [[ "$word" =~ (.*)\.F90$ ]]; then
    FILE=${BASH_REMATCH[1]}
    COMPILER="$FORT_COMPILER"
    FORTRAN=true
  # Remove "-o [outputfile]" from the arguments
  elif [[ "$word" =~ \-o$ ]]; then
    PREV="$word"
  # Put the output file in OUTPUTFILE, extension and all
  elif [[ "$PREV" =~ \-o$ ]]; then
    PREV=""
    OUTPUTFILE=${word}
  # Everything else gets output to the ld_wrapper
  else
    OUTPUT_ARGS="$OUTPUT_ARGS $word"
  fi

  # If the argument is -c
  if [[ "$word" =~ "-c" ]]; then
    ONLY_COMPILE=true
  fi

done

# EXTRA_ARGS are arguments used to compile to IR
export EXTRA_ARGS="-emit-llvm -o ${OUTPUTFILE}.bc $EXTRA_ARGS"

if [ "$ONLY_COMPILE" = false ]; then
  # If the user didn't specify -c, we need to call the ld_wrapper
  ${LD_WRAPPER} $OUTPUT_ARGS -o $OUTPUTFILE
else
  # If the user *did* specify -c, just compile
  # Output the original arguments to a file, to be used by ld_wrapper
  echo $OUTPUT_ARGS > $OUTPUTFILE.args
  # Compile to both a '.bc' file as well as the standard object file
  ${LLVMPATH}${COMPILER} $ARGS $EXTRA_ARGS
  ${LLVMPATH}${COMPILER} $ARGS
fi
