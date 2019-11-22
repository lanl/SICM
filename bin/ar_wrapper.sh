#!/bin/bash
# This wrapper is intended to be used with the compiler_wrapper.sh and
# ld_wrapper.sh. It wraps the `ar` utility, and can be used in a build
# system that uses `ar` to archive a bunch of `.o` files, then links
# those together into the final executable.

# This should eventually be replaced by a proper `ar` wrapper that actually
# archives the files that it needs. `ranlib` would also have to have a wrapper
# in that case. However, for now, it simply creates a
# newline-separated list of files (extensions removed) that are "in the archive."
# When the ld_wrapper.sh script sees this file, it will transform, compile,
# and link all of these files together.

ARGS=("$@")
OBJECTFILES=""
START_OF_OBJECTFILES="2"
AR_FILE_INDEX="1"

# First check if the second argument is a `.a` file. This it he most usual case.
if [[ ! ${ARGS[1]} =~ (.*)\.a$ ]]; then
  START_OF_OBJECTFILES="1"
  AR_FILE_INDEX="0"
  # If not, check the first argument. It's valid to call `ar` without any arguments
  # other than the `.a` file and the list of files that you want to archive.
  if [[ ! ${ARGS[0]} =~ (.*)\.a$ ]]; then
    echo "The AR wrapper didn't get a '.a' file as its output argument. Aborting."
    exit 1
  fi
fi

AR_FILE="${ARGS[${AR_FILE_INDEX}]}"

echo -n "" > $AR_FILE
for word in ${ARGS[@]:${START_OF_OBJECTFILES}}; do
  if [[ "$word" =~ (.*)\.o$ ]]; then
    # Found an object file. Grab its .bc and .args files and archive them, too.
    FILE=${BASH_REMATCH[1]}

    # Just output the filename to the .a file. The ld wrapper will then read
    # in this list of files and get the .o, .bc, and .args files that *should*
    # have gone in the archive. This way, we don't have to construct the archive,
    # only to immediately extract all of the files from it.
    # We're removing the .o extension here, so it's just the filename.
    echo "`readlink -f $FILE`" >> $AR_FILE
  fi
done
