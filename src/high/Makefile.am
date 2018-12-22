#
# Copyright 2018        UT-Battelle, LLC
#                       All rights reserved.
#
# See COPYING in top-level directory.
#
# Additional copyrights may follow
#
# $HEADER$
#

bin_PROGRAMS = hotset memreserve dump_info
lib_LTLIBRARIES = libhigh.la

dump_info_SOURCES = dump_info.c
dump_info_CFLAGS = @JEMALLOC_CPPFLAGS@ -I@abs_top_srcdir@/include

hotset_SOURCES = hotset.c
hotset_CFLAGS = @JEMALLOC_CPPFLAGS@ -I@abs_top_srcdir@/include

memreserve_SOURCES = memreserve.c
memreserve_LDADD = -lnuma
memreserve_CFLAGS = @JEMALLOC_CPPFLAGS@ -I@abs_top_srcdir@/include

libhigh_la_SOURCES = high.c profile.c sicm_rdspy.c
libhigh_la_LIBADD = @JEMALLOC_LIBS@ -lnuma @abs_top_builddir@/src/low/libsicm.la -lrt
libhigh_la_LDFLAGS = -version-info 0:0:0
libhigh_la_CFLAGS = @JEMALLOC_CPPFLAGS@ -I@abs_top_srcdir@/include

if HAVE_LLVM
lib_LTLIBRARIES += libcompass.la librdspy.la

libcompass_la_SOURCES = compass.cpp
libcompass_la_LIBADD = @LLVM_LIBS@
libcompass_la_LDFLAGS = -version-info 0:0:0 -disable-static
libcompass_la_CPPFLAGS = @LLVM_CPPFLAGS@ -I@abs_top_srcdir@/include -disable-static

librdspy_la_SOURCES = rdspy.cpp
librdspy_la_LIBADD = @LLVM_LIBS@
librdspy_la_LDFLAGS = -version-info 0:0:0
librdspy_la_CPPFLAGS = @LLVM_CPPFLAGS@
endif
