#!/bin/bash
# This wrapper simply calls clang, but instead of outputting
# an object file, it will output a binary LLVM IR format, putting it
# into the .o file that would normally be the object file.
# This wrapper also parses and outputs the arguments used to compile each
# file, so that it can be read and used by the ld_wrapper. It removes

# The original arguments and the ones we're going to add
ARGS=$@
OUTPUT_ARGS=""
EXTRA_ARGS=""
FILE=""
ONLY_COMPILE=false
LLVMPATH="${LLVMPATH:- }"
C_COMPILER="${C_COMPILER:-clang}"
CXX_COMPILER="${CXX_COMPILER:-clang++}"
COMPILER="$C_COMPILER"
PREV=""

# Iterate over all arguments
for word in $ARGS; do

  # Check if the argument is a C file
  # Also don't put this in the output arguments
  if [[ "$word" =~ (.*)\.c$ ]]; then
    FILE=${BASH_REMATCH[1]}
    COMPILER="$C_COMPILER"
  # Or if it's a C++ file
  elif [[ "$word" =~ (.*)\.cpp$ ]] || [[ "$word" =~ (.*)\.C$ ]] || [[ "$word" =~ (.*)\.cc ]]; then
    FILE=${BASH_REMATCH[1]}
    COMPILER="$CXX_COMPILER"
  # Remove "-o [outputfile]" from the arguments
  elif [[ "$word" =~ \-o$ ]]; then
    PREV="$word"
  elif [[ "$word" =~ (.*)\.o$ ]] && [[ "$PREV" =~ \-o$ ]]; then
    PREV=""
  # Everything else gets output to the ld_wrapper
  else
    OUTPUT_ARGS="$OUTPUT_ARGS $word"
  fi

  # If the argument is -c
  if [[ "$word" =~ "-c" ]]; then
    ONLY_COMPILE=true
  fi

done

if [ "$ONLY_COMPILE" = false ]; then
  echo "WARNING: This wrapper was intended to be used with '-c' to compile only, not to link."
fi

# EXTRA_ARGS are arguments used to compile to IR
export EXTRA_ARGS="-emit-llvm -o $FILE.bc $EXTRA_ARGS"

# Output the original arguments to a file, to be used by ld_wrapper
echo $OUTPUT_ARGS > $FILE.args

# Compile to both a '.bc' file as well as a '.o', in parallel
${LLVMPATH}${COMPILER} $ARGS $EXTRA_ARGS &
${LLVMPATH}${COMPILER} $ARGS &
wait
