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

fortranmoddir = ${exec_prefix}/lib
lib_LTLIBRARIES = libsicm.la libsicm_cpp.la libsicm_f90.la
fortranmod_DATA = sicm_f90.mod

sicm_f90.mod: libsicm_f90.la
	touch sicm_f90.mod

libsicm_la_SOURCES = sicm_low.c sicm_arena.c
libsicm_la_LIBADD = @JEMALLOC_LIBS@ -lnuma
libsicm_la_LDFLAGS = -version-info 0:0:0
libsicm_la_CFLAGS = @JEMALLOC_CPPFLAGS@ -I@top_srcdir@/include

libsicm_f90_la_SOURCES = fbinding_c.c fbinding_f90.f90
libsicm_f90_la_LIBADD = 
libsicm_f90_la_LDFLAGS = -version-info 0:0:0
libsicm_f90_la_CFLAGS = @JEMALLOC_CPPFLAGS@ -I@top_srcdir@/include

libsicm_cpp_la_SOURCES = sicm_cpp.cpp
libsicm_cpp_la_LIBADD = 
libsicm_cpp_la_LDFLAGS = -version-info 0:0:0
libsicm_cpp_la_CPPFLAGS = -I@top_srcdir@/include
