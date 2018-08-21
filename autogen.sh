#!/bin/sh

# exit on any error
set -e

# set up libtool if not done already
test -f autotools/ltmain.sh || ( libtoolize || glibtoolize )

# do all the autotools setup, in the right order, as many times as necessary
autoreconf -i --force

# clean up temporary files
rm -rf autom4te.cache
