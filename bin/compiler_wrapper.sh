#!/bin/bash
# This wrapper simply calls clang, but instead of outputting
# an object file, it will output a binary LLVM IR format, putting it
# into the .o file that would normally be the object file.
# This wrapper also parses and outputs the arguments used to compile each
# file, so that it can be read and used by the ld_wrapper.

# The path that this script is in
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# To disable the transformation
NO_IR="${NO_IR:- }"
NO_TRANSFORM="${NO_TRANSFORM:- }"

# Might want to define these manually depending on environment
LLVMPATH="${LLVMPATH:- }"
C_COMPILER="${C_COMPILER:-clang}"
CXX_COMPILER="${CXX_COMPILER:-clang++}"
FORT_COMPILER="${FORT_COMPILER:-flang}"
LD_WRAPPER="${LD_WRAPPER:-${DIR}/ld_wrapper.sh}"
DEFAULT_COMPILER="${DEFAULT_COMPILER:-${C_COMPILER}}"
COMPILER=""
OBJECT_FILES=""

# The original arguments to the compiler
ARGS=$@
INPUT_FILES=()
EXTENSIONS=() # Store extensions separately

# Whether or not arguments are specified
ONLY_COMPILE=false # `-c`
ONLY_LINK=false # No input files, but at least one object file
OUTPUT_FILE="" # `-o`
DEFAULT_COMPILER=false
LANGUAGE_SPECIFIED=false
LANGUAGE_SPECIFIED_LAST=false

# Iterate over all arguments
PREV=""
for word in $ARGS; do

  # FLAGS BEGINNING WITH '-'
  if [[ "$word" = "-" ]]; then
    # Reading from stdin, this is unsupported, so call the normal compiler
    # and exit
    DEFAULT_COMPILER=true
  elif [[ "$word" =~ ^\-o$ ]]; then
    # Remove "-o [outputfile]" from the arguments
    PREV="$word"
  elif [[ "$PREV" =~ ^\-o$ ]]; then
    # Put the output file in OUTPUTFILE, extension and all
    PREV=""
    OUTPUT_FILE=${word}
  elif [[ "$word" =~ ^\-c$ ]]; then
    # If it's `-c`, then we don't link
    ONLY_COMPILE=true
  elif [[ "$word" =~ ^\-M ]]; then
    # The user is trying to preprocess or generate deps
    DEFAULT_COMPILER=true
  elif [[ "$word" =~ ^\-x$ ]]; then
    # The user is telling us the input will be a certain language.
    # This information is particularly useful if we're reading from
    # stdin. Set the compiler.
    # TODO: use this to override other compiler choices
    LANGUAGE_SPECIFIED_LAST=true
  elif [[ "$word" =~ ^\-.* ]]; then
    # So that all arguments that begin with '-' aren't input files or input files
    EXTRA_ARGS="$EXTRA_ARGS $word"

  # FLAGS WITH A REQUIRED ARGUMENT
  elif [[ "$LANGUAGE_SPECIFIED_LAST" = true ]]; then
    # The last argument was "-x". Get the language that the user specified.
    if [[ "$word" = "c++" ]]; then
      COMPILER="$CXX_COMPILER"
    elif [[ "$word" = "c" ]]; then
      COMPILER="$C_COMPILER"
    fi
    LANGUAGE_SPECIFIED_LAST=false
    LANGUAGE_SPECIFIED=true

  # DETECT INPUT AND OUTPUT FILES
  elif [[ $(file --mime-type -b $(readlink -f "$word")) == "application/x-object" ]]; then
    OBJECT_FILES="${OBJECT_FILES} $word"
  elif [[ $(file --mime-type -b $(readlink -f "$word")) == "application/x-sharedlib" ]]; then
    EXTRA_ARGS="$EXTRA_ARGS $word"
  elif [[ "$word" =~ (.*)\.(c)$ ]]; then
    # Check if the argument is a C file.
    # Use the C++ compiler if a C++ file has already been specified.
    INPUT_FILES+=("${BASH_REMATCH[1]}")
    EXTENSIONS+=("${BASH_REMATCH[2]}")
    if [[ $COMPILER != "$CXX_COMPILER" ]]; then
      COMPILER="$C_COMPILER"
    fi
  elif [[ "$word" =~ (.*)\.(cpp)$ ]] || [[ "$word" =~ (.*)\.(C)$ ]] || [[ "$word" =~ (.*)\.(cc)$ ]] || [[ "$word" =~ (.*)\.(cxx)$ ]]; then
    # Or if it's a C++ file
    INPUT_FILES+=("${BASH_REMATCH[1]}")
    EXTENSIONS+=("${BASH_REMATCH[2]}")
    COMPILER="$CXX_COMPILER"
  elif [[ "$word" =~ (.*)\.(f90)$ ]] || [[ "$word" =~ (.*)\.(f95)$ ]] || [[ "$word" =~ (.*)\.(f03)$ ]] || [[ "$word" =~ (.*)\.(F90)$ ]] || [[ "$word" =~ (.*)\.(f)$ ]]; then
    # Or a Fortran file
    INPUT_FILES+=("${BASH_REMATCH[1]}")
    EXTENSIONS+=("${BASH_REMATCH[2]}")
    COMPILER="$FORT_COMPILER"
  else
    # Everything except `-o`, `-c`, and the input source files
    EXTRA_ARGS="$EXTRA_ARGS $word"
  fi

done

if [[ "$DEFAULT_COMPILER" = true ]]; then
  # The user is generating deps or preprocessing, just let
  # the compiler do this.
  ${LLVMPATH}${COMPILER} $ARGS
  exit $?
fi

if [[ ((${#INPUT_FILES[@]} -gt 1) && ($OUTPUT_FILE != "") && ("$ONLY_COMPILE" = true)) ]]; then
  # This is illegal, just call the compiler to error out
  echo "ERROR: There are multiple input files, we're not linking, and an output file was specified. Aborting."
  ${LLVMPATH}${COMPILER} $ARGS
  exit $?
fi

if [[ ((${#INPUT_FILES[@]} -eq 0) && (${#OBJECT_FILES[@]} -eq 0)) ]]; then
  echo "ERROR: There are no input files or object files. Aborting."
  ${LLVMPATH}${COMPILER} $ARGS
  exit $?
fi

if [[ (${#INPUT_FILES[@]} -eq 0) ]]; then
  if [[ $ONLY_COMPILE = true ]]; then
    echo "ERROR: Specified to only compile with '-c', but specified no input files. Aborting."
    ${LLVMPATH}${COMPILER} $ARGS
    exit $?
  fi
  ONLY_LINK=true
fi

# If we're just going to call the default compiler
if [[ ($NO_IR != " ") ]]; then
  if [[ ${ONLY_LINK} = false ]]; then
    ${LLVMPATH}${COMPILER} $ARGS
    if [[ ${ONLY_COMPILE} = false ]]; then
      ${LD_WRAPPER} $ARGS
    fi
  else
    ${LD_WRAPPER} $ARGS
  fi
  exit $?
fi

# Don't compile anything if there are no input files
if [[ "$ONLY_LINK" = false ]]; then
  INPUTFILE_STR=""
  for ((i=0;i<${#INPUT_FILES[@]};++i)); do
    INPUTFILE_STR="${INPUTFILE_STR} ${INPUT_FILES[i]}.${EXTENSIONS[i]}"
  done

  # Produce a normal object file as well as the bytecode file
  if [[ $OUTPUT_FILE  == "" ]]; then
    ${LLVMPATH}${COMPILER} $EXTRA_ARGS $INPUTFILE_STR -c
    ${LLVMPATH}${COMPILER} $EXTRA_ARGS -emit-llvm $INPUTFILE_STR -c
    for file in ${INPUT_FILES[@]}; do
      echo $EXTRA_ARGS > ${file}.args
    done
  else
    if [[ "$ONLY_COMPILE" = true ]]; then
      # If we're only compiling and they specify an output file, adhere to that
      ${LLVMPATH}${COMPILER} $EXTRA_ARGS $INPUTFILE_STR -c -o $OUTPUT_FILE
      if [[ "$OUTPUT_FILE" =~ (.*)\.o$ ]]; then
        # If their output file choice ends in '.o', replace that with '.bc'
        ${LLVMPATH}${COMPILER} $EXTRA_ARGS -emit-llvm -c $INPUTFILE_STR -o ${BASH_REMATCH[1]}.bc
        echo $EXTRA_ARGS > ${BASH_REMATCH[1]}.args
      else
        # Otherwise, just use append '.bc'. This way, the ld_wrapper doesn't have to guess.
        ${LLVMPATH}${COMPILER} $EXTRA_ARGS -emit-llvm $INPUTFILE_STR -c -o ${OUTPUT_FILE}.bc
        echo $EXTRA_ARGS > ${OUTPUT_FILE}.args
      fi
    else
      # If we're going to link and they specify an executable name, ignore that until we link
      ${LLVMPATH}${COMPILER} ${EXTRA_ARGS} -c $INPUTFILE_STR
      ${LLVMPATH}${COMPILER} ${EXTRA_ARGS} -emit-llvm -c $INPUTFILE_STR
      for file in ${INPUT_FILES[@]}; do
        echo $EXTRA_ARGS > ${file}.args
      done
    fi
  fi
fi

# Link if necessary
if [[ "$ONLY_COMPILE" = false ]]; then
  # Convert all e.g. "foo.c" -> "foo.o"
  for file in ${INPUT_FILES[@]}; do
    OBJECT_FILES="$OBJECT_FILES ${file}.o"
  done

  # Call the ld_wrapper
  if [[ $OUTPUT_FILE == "" ]]; then
    ${LD_WRAPPER} $EXTRA_ARGS $OBJECT_FILES
  else
    ${LD_WRAPPER} $EXTRA_ARGS $OBJECT_FILES -o $OUTPUT_FILE
  fi
fi
