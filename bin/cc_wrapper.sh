#!/bin/bash
# This wrapper simply calls clang, but instead of outputting
# an object file, it will output a binary LLVM IR format, putting it
# into the .o file that would normally be the object file.

# The original arguments and the ones we're going to add
ARGS=$@
EXTRA_ARGS=""
FILE=""
ONLY_COMPILE=false

# Iterate over all arguments
for word in $ARGS; do

  # Check if the argument is a C file
  if [[ "$word" =~ (.*)\.c ]]; then
    FILE=${BASH_REMATCH[1]}
  fi

  # If the argument is -c
  if [[ "$word" =~ "-c" ]]; then
    ONLY_COMPILE=true
  fi

done


if [ "$ONLY_COMPILE" = false ]; then
  echo "WARNING: This wrapper was intended to be used with '-c' to compile only, not to link."
fi

EXTRA_ARGS="-emit-llvm -o $FILE.bc $EXTRA_ARGS"

# Compile to both a '.bc' file as well as a '.o', in parallel
clang $ARGS $EXTRA_ARGS &
clang $ARGS &
wait
