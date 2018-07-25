#!/bin/bash
# This wrapper expects all '.o' files that it's given to actually
# be binary LLVM IR files. It also assumes that each '.o' file
# has a corresponding '.c' file. It first links them together into one
# giant IR file (to get the entire call graph), then it iterates over
# each of the files and calls a compiler pass on them, then it
# compiles them to *actual* object files, then it links them using
# the arguments that it's given.

# Gets the location of the script to find Compass
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
LIB_DIR="$DIR/../lib"

# The original arguments and the ones we're going to add
ARGS=$@

# An array and space-delimited string of object files that we want to link
FILES_ARR=()
BC_STR=""
OBJECT_STR=""

# Iterate over all arguments
for word in $ARGS; do

  # Check if the argument is an object file that we need to link
  if [[ "$word" =~ (.*)\.o ]]; then
    echo ${BASH_REMATCH[1]}
    FILES_ARR+=("${BASH_REMATCH[1]}")
    BC_STR="$BC_STR ${BASH_REMATCH[1]}.bc"
    OBJECT_STR="$OBJECT_STR ${BASH_REMATCH[1]}.o"
  fi

done

# Check if there are zero files
if [ ${#FILES_ARR[@]} -eq 0 ]; then
  echo "WARNING: There are no object files being passed to the linker."
fi

# Link all of the IR files into one
llvm-link $BC_STR -o .sicm_ir.bc

# Run the compiler pass to generate the call graph
opt -load ${LIB_DIR}/compass.so -compass-mode=analyze \
    -compass-quick-exit -compass -compass-depth=3 \
    .sicm_ir.bc -o .sicm_ir.bc

# Run the compiler pass on each individual file
# Also compile each file to its transformed '.o', overwriting the old one
for file in "${FILES_ARR[@]}"; do
  opt -load ${LIB_DIR}/compass.so -compass-mode=transform -compass -compass-depth=3 $file.bc -o $file.bc;
  clang -c $file.bc -o $file.o
done

# Now finally link the transformed '.o' files
clang -L${LIB_DIR} -lhigh -Wl,-rpath,${LIB_DIR} $ARGS
