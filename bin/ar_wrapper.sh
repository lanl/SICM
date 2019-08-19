#!/bin/bash
# This wrapper is intended to be used with the compiler_wrapper.sh and
# ld_wrapper.sh. It wraps the `ar` utility, and can be used in a build
# system that uses `ar` to archive a bunch of `.o` files, then links
# those together into the final executable.
# It works by simply archiving both the `.o` files that it was given,
# as well as the accompanying `.bc` files. These `.bc` files should
# have been output by `compiler_wrapper.sh`. Then, `ld_wrapper.sh`
# will extract these `.bc` files from the archive, transform them,
# recompile them into `.o` files, and re-archive them.
# It also includes the `.args` files in the archive, so that `ld_wrapper.sh`
# knows which arguments to use when recompiling the object files.

ARGS=("$@")
OBJECTFILES=""

if [[ ! ${ARGS[1]} =~ (.*)\.a$ ]]; then
  echo "The AR wrapper didn't get a '.a' file as its output argument. Aborting."
  exit 1
fi

AR_FILE="${ARGS[1]}"

echo -n "" > $AR_FILE
for word in ${ARGS[@]:2}; do
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
