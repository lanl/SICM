#!/bin/bash
# First argument is script to be copied.
# Second argument is the string to replace.
# Third and subsequent arguments are the strings to replace it with in the copies.

SCRIPT=${1}
OLD=${2}
echo "Making a copy of ${SCRIPT}."
for NEW in "${@:3}"; do
  NEWSCRIPT=$(echo ${SCRIPT} | awk "{gsub(\"${OLD}\",\"${NEW}\"); print;}")
  echo "Creating ${NEWSCRIPT}..."
  awk "{gsub(\"${OLD}\",\"${NEW}\"); print;}" ${SCRIPT} > ${NEWSCRIPT}
done
