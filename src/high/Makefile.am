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

bin_PROGRAMS = 
lib_LTLIBRARIES = libhigh.la libsg.la libsgcxx.la libsgf.la

libhigh_la_SOURCES = high.c
libhigh_la_LIBADD = 
libhigh_la_LDFLAGS = -version-info 0:0:0

libsgcxx_la_SOURCES = sg_bindings_cxx.cpp sg.c
libsgcxx_la_LIBADD = 

libsgf_la_SOURCES = sg_bindings_fort.f90 sg.c
libsgf_la_LIBADD = 

libsg_la_SOURCES = sg.c
libsg_la_LIBADD =
libsg_la_LDFLAGS = -version-info 0:0:0

if HAVE_LLVM
lib_LTLIBRARIES += libcompass.la

libcompass_la_SOURCES = compass.cpp
libcompass_la_LIBADD = 
libcompass_la_LDFLAGS = -version-info 0:0:0
endif
