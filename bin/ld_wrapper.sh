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
LLVMOPT="${LLVMOPT:-opt}"
LD_COMPILER="${LD_COMPILER:-clang}"
LD_LINKER="${LD_LINKER:-clang}"

# The original arguments and the ones we're going to add
ARGS=$@
# The new arguments that we're going to use to link. We're going
# to remove any .a files, and replace them with the .o files that they should have contained.
LINKARGS=""
LINKER_INPUT_FILES=""

# An array and space-delimited string of object files that we want to link
FILES_ARR=()
BC_STR=""

# Iterate over all arguments
PREV=""
OUTPUT_FILE=""
for word in $ARGS; do
  # If the argument is an option, just pass it along.
  # Also just pass along the argument to `-o`.
  if [[ ("$word" =~ ^-.*) ]]; then
    LINKARGS="$LINKARGS $word"
  elif [[ $PREV == "-o" ]]; then
    OUTPUT_FILE="$word"
    LINKARGS="$LINKARGS $word"
  # Check if the argument is an object file that we need to link
  elif [[ $(file --mime-type -b "$word") == "application/x-object" ]]; then
    # If it ends in '.o', replace that with '.bc'. Otherwise, just append '.bc'
    # This is the same rule that the compiler wrapper uses to create the '.bc' file.
    if [[ "$word" =~ (.*)\.o$ ]]; then
      FILES_ARR+=("${BASH_REMATCH[1]}")
      BC_STR="$BC_STR ${BASH_REMATCH[1]}.bc"
    else
      FILES_ARR+=("${word}")
      BC_STR="$BC_STR ${word}.bc"
    fi
    LINKER_INPUT_FILES="$LINKER_INPUT_FILES $word"
  elif [[ ($word =~ (.*)\.a$) && ($(file --mime-type -b "$word") =~ ^text) ]]; then
    # We've found a `.a` file. Assume it was created with the ar_wrapper.sh.
    # Each line is just a filename.
    # Notably, *don't* add this .a file to the list of link arguments.
    while read line; do
      FILES_ARR+=("${line}")
      BC_STR="$BC_STR ${line}.bc"
      LINKER_INPUT_FILES="$LINKER_INPUT_FILES ${line}.o"
    done < "${word}"
  elif [[ $(file --mime-type -b $(readlink -f "$word")) == "application/x-sharedlib" ]]; then
    LINKARGS="$LINKARGS $word"
  fi
  PREV="${word}"
done

# Output file defaults to `a.out`.
if [[ $OUTPUT_FILE == "" ]]; then
  OUTPUT_FILE="a.out"
  LINKARGS="$LINKARGS -o $OUTPUT_FILE"
fi

# Check if there are zero files
if [ ${#FILES_ARR[@]} -eq 0 ]; then
  echo "WARNING: There are no object files being passed to the linker."
fi

# Link all of the IR files into one
${LLVMPATH}${LLVMLINK} $BC_STR -o .sicm_ir.bc

# Run the compiler pass to generate the call graph.
${LLVMPATH}${LLVMOPT} -load ${LIB_DIR}/libsicm_compass.so -compass-mode=analyze \
    -compass-quick-exit -compass -compass-depth=3 \
    .sicm_ir.bc -o .sicm_ir_transformed.bc

# Run the compiler pass on each individual file
# Construct a newline-separated list of commands.
COMMANDS=""
if [ -z ${SH_RDSPY+x} ]; then
    for file in "${FILES_ARR[@]}"; do
      COMMANDS+="${LLVMPATH}${LLVMOPT} -load ${LIB_DIR}/libsicm_compass.so -compass-mode=transform -compass -compass-depth=3 ${file}.bc -o ${file}.bc"
      COMMANDS+=$'\n'
    done
else
    for file in "${FILES_ARR[@]}"; do
      COMMANDS+="${LLVMPATH}${LLVMOPT} -load ${LIB_DIR}/libsicm_compass.so -load ${LIB_DIR}/libsicm_rdspy.so -compass-mode=transform -compass -compass-depth=3 -rdspy -rdspy-sampling-threshold=${SH_RDSPY_SAMPLE} ${file}.bc -o ${file}.bc"
      COMMANDS+=$'\n'
    done
fi
echo "$COMMANDS" | xargs -I CMD --max-procs=64 bash -c CMD

# Also compile each file to its transformed object file, overwriting the old one
COMMANDS=""
for file in "${FILES_ARR[@]}"; do
  FILEARGS=`cat $file.args`
  COMMANDS+="${LLVMPATH}${LD_COMPILER} $FILEARGS -c ${file}.bc -o ${file}.o"
  COMMANDS+=$'\n'
done
echo "$COMMANDS" | xargs -I CMD --max-procs=64 bash -c CMD

# Now finally link the transformed '.o' files
LINKARGS="$LINKARGS -L${LIB_DIR} -lsicm_high -lsicm_new_delete -lsicm_cstd -Wl,-rpath,${LIB_DIR}"
${LLVMPATH}${LD_LINKER} $LINKER_INPUT_FILES $LINKARGS
