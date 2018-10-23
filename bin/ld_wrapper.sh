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
LLVMPATH="${LLVMPATH:- }"
LLVMLINK="${LLVMLINK:-llvm-link}"
OPT="${OPT:-opt}"
LD_COMPILER="${LD_COMPILER:-clang}"
LD_LINKER="${LD_LINKER:-clang}"

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
${LLVMPATH}${LLVMLINK} $BC_STR -o .sicm_ir.bc

# Run the compiler pass to generate the call graph
${LLVMPATH}${OPT} -load ${LIB_DIR}/libcompass.so -compass-mode=analyze \
    -compass-quick-exit -compass -compass-depth=3 \
    .sicm_ir.bc -o .sicm_ir.bc

# Run the compiler pass on each individual file
for file in "${FILES_ARR[@]}"; do
  ${LLVMPATH}${OPT} -load ${LIB_DIR}/libcompass.so -compass-mode=transform -compass -compass-depth=3 $file.bc -o $file.bc &
done
wait

# Also compile each file to its transformed '.o', overwriting the old one
for file in "${FILES_ARR[@]}"; do
  FILEARGS=`cat $file.args`
  echo IN LD_WRAPPER
  echo ${LLVMPATH}${LD_COMPILER} $FILEARGS -c $file.bc -o $file.o
  ${LLVMPATH}${LD_COMPILER} $FILEARGS -c $file.bc -o $file.o &
done
wait

# Now finally link the transformed '.o' files
echo LINKING
echo ${LLVMPATH}${LD_LINKER} -L${LIB_DIR} -lhigh -Wl,-rpath,${LIB_DIR} $ARGS
${LLVMPATH}${LD_LINKER} -L${LIB_DIR} -lhigh -Wl,-rpath,${LIB_DIR} $ARGS
