#!/bin/bash
# This wrapper simply calls clang, but instead of outputting
# an object file, it will output a binary LLVM IR format, putting it
# into the .o file that would normally be the object file.
# This wrapper also parses and outputs the arguments used to compile each
# file, so that it can be read and used by the ld_wrapper.

# The path that this script is in
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Might want to define these manually depending on environment
LLVMPATH="${LLVMPATH:- }"
C_COMPILER="${C_COMPILER:-clang}"
CXX_COMPILER="${CXX_COMPILER:-clang++}"
FORT_COMPILER="${FORT_COMPILER:-flang}"
LD_WRAPPER="${LD_WRAPPER:-${DIR}/ld_wrapper.sh}"
COMPILER="$C_COMPILER"

# The original arguments to the compiler
ARGS=$@
INPUT_FILES=()
EXTENSIONS=() # Store extensions separately

# Whether or not arguments are specified
ONLY_COMPILE=false # `-c`
OUTPUT_FILE="" # `-o`

# Iterate over all arguments
PREV=""
for word in $ARGS; do

  if [[ "$word" =~ (.*)\.(c)$ ]]; then
    # Check if the argument is a C file.
    # Use the C++ compiler if a C++ file has already been specified.
    INPUT_FILES+=("${BASH_REMATCH[1]}")
    EXTENSIONS+=("${BASH_REMATCH[2]}")
    if [[ $COMPILER != "$CXX_COMPILER" ]]; then
      COMPILER="$C_COMPILER"
    fi
  elif [[ "$word" =~ (.*)\.(cpp)$ ]] || [[ "$word" =~ (.*)\.(C)$ ]] || [[ "$word" =~ (.*)\.(cc)$ ]]; then
    # Or if it's a C++ file
    INPUT_FILES+=("${BASH_REMATCH[1]}")
    EXTENSIONS+=("${BASH_REMATCH[2]}")
    COMPILER="$CXX_COMPILER"
  elif [[ "$word" =~ (.*)\.(f90)$ ]] || [[ "$word" =~ (.*)\.(f95)$ ]] || [[ "$word" =~ (.*)\.(f03)$ ]] || [[ "$word" =~ (.*)\.(F90)$ ]]; then
    # Or a Fortran file
    INPUT_FILES+=("${BASH_REMATCH[1]}")
    EXTENSIONS+=("${BASH_REMATCH[2]}")
    COMPILER="$FORT_COMPILER"
  elif [[ "$word" =~ \-o$ ]]; then
    # Remove "-o [outputfile]" from the arguments
    PREV="$word"
  elif [[ "$PREV" =~ \-o$ ]]; then
    # Put the output file in OUTPUTFILE, extension and all
    PREV=""
    OUTPUT_FILE=${word}
  elif [[ "$word" =~ "-c" ]]; then
    # If it's `-c`, then we don't link
    ONLY_COMPILE=true
  else
    # Everything except `-o`, `-c`, and the input source files
    EXTRA_ARGS="$EXTRA_ARGS $word"
  fi

done

if [[ ((${#INPUT_FILES[@]} -gt 1) && ($OUTPUT_FILE != "") && ($ONLY_COMPILE)) || (${#INPUT_FILES[@]} -eq 0) ]]; then
  # This is illegal, just call the compiler to error out
  ${LLVMPATH}${COMPILER} $ARGS
  exit $?
fi

# Produce a normal object file as well as the bytecode file
if [[ $OUTPUT_FILE  == "" ]]; then
  ${LLVMPATH}${COMPILER} $EXTRA_ARGS -c
  ${LLVMPATH}${COMPILER} $EXTRA_ARGS -emit-llvm -c
else
  if [[ "$ONLY_COMPILE" = true ]]; then
    # If we're only compiling and they specify an output file, adhere to that
    ${LLVMPATH}${COMPILER} $EXTRA_ARGS -c -o $OUTPUT_FILE
    if [[ "$OUTPUT_FILE" =~ (.*)\.o$ ]]; then
      # If their output file choice ends in '.o', replace that with '.bc'
      ${LLVMPATH}${COMPILER} $EXTRA_ARGS -emit-llvm -c -o ${BASH_REMATCH[1]}.bc
    else
      # Otherwise, just use append '.bc'. This way, the ld_wrapper doesn't have to guess
      ${LLVMPATH}${COMPILER} $EXTRA_ARGS -emit-llvm -c -o ${OUTPUT_FILE}.bc
    fi
  else
    # If we're going to link and they specify an executable name, ignore that until we link
    INPUTFILE_STR=""
    for ((i=0;i<${#INPUT_FILES[@]};++i)); do
      INPUTFILE_STR="${INPUTFILE_STR} ${INPUT_FILES[i]}.${EXTENSIONS[i]}"
    done
    ${LLVMPATH}${COMPILER} ${EXTRA_ARGS} -c $INPUTFILE_STR
    ${LLVMPATH}${COMPILER} ${EXTRA_ARGS} -emit-llvm -c $INPUTFILE_STR
  fi
fi

# Link if necessary
if [[ "$ONLY_COMPILE" = false ]]; then
  # Convert all e.g. "foo.c" -> "foo.o"
  OBJECT_FILES=""
  for file in ${INPUT_FILES[@]}; do
    OBJECT_FILES="$OBJECT_FILES ${file}.o"
  done

  echo "LD_WRAPPER TIME"

  # Call the ld_wrapper
  if [[ $OUTPUT_FILE == "" ]]; then
    echo "${LD_WRAPPER} $EXTRA_ARGS $OBJECT_FILES"
    ${LD_WRAPPER} $EXTRA_ARGS $OBJECT_FILES
  else
    echo "${LD_WRAPPER} $EXTRA_ARGS $OBJECT_FILES -o $OUTPUT_FILE"
    ${LD_WRAPPER} $EXTRA_ARGS $OBJECT_FILES -o $OUTPUT_FILE
  fi
fi
