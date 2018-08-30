# -*- shell-script -*-
#
# Copyright 2018        UT-Battelle, LLC.
#                       All rights reserved.
# See COPYING in top-level directory.
# 
# Additional copyrights may follow
# 
# $HEADER$
#

AC_INIT([sicm], [0.1], [valleegr@ornl.gov])
AC_CONFIG_SRCDIR([include/sicm_low.h])

AM_INIT_AUTOMAKE([dist-bzip2 subdir-objects foreign tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_MACRO_DIR([m4])

# Checks for programs
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_FC
AC_PROG_INSTALL
AC_PROG_MKDIR_P

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h stdint.h stdlib.h string.h sys/time.h unistd.h utmpx.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime getpagesize munmap pow strerror])

AM_PROG_CC_C_O
m4_pattern_allow([AM_PROG_AR], [AM_PROG_AR])
LT_INIT

CPPFLAGS_save="$CPPFLAGS"
LIBS_save="$LIBS"
top_srcdir=`pwd`
AC_SUBST([CPPFLAGS],["-I$top_srcdir/include $CPPFLAGS_save"])

dnl# pthread

AC_CHECK_HEADERS(pthread.h,, [AC_MSG_ERROR([pthread.h required])])
AC_CHECK_LIB(pthread, pthread_create, [LIBS="$LIBS -lpthread"])

dnl# jemalloc

AC_MSG_CHECKING([jemalloc installation])
AC_ARG_WITH(jemalloc,
	    [AS_HELP_STRING([--with-jemalloc=PATH],
	    		    [Absolute path to the install directory for jemalloc]
	    		   )],
	    [JEMALLOC_INSTALL_DIR="${withval}"],
	    [JEMALLOC_INSTALL_DIR=""])

JEMALLOC_INC=""
JEMALLOC_LIB=""
if test "x$JEMALLOC_INSTALL_DIR" = "x" ; then
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
	JEMALLOC_INC="-I$JEMALLOC_INSTALL_DIR/include -I$JEMALLOC_INSTALL_DIR/include"
	JEMALLOC_LIB="-L$JEMALLOC_INSTALL_DIR/lib"
fi

CPPFLAGS_save="$CPPFLAGS"
LIBS_save="$LIBS"
AC_SUBST([CPPFLAGS],["$JEMALLOC_INC -DJEMALLOC_NO_DEMANGLE $CPPFLAGS_save"])
AC_SUBST([LIBS],["$JEMALLOC_LIB -ljemalloc $LIBS_save"])
AC_CHECK_HEADERS([jemalloc/jemalloc.h],[AC_DEFINE(HAVE_JEMALLOC)],[AC_MSG_ERROR([Cannot find jemalloc headers])])
AC_CHECK_LIB([jemalloc],[malloc],,AC_MSG_ERROR([Cannot use jemalloc lib]))
AC_SUBST([CPPFLAGS],["$CPPFLAGS_save"])
AC_SUBST([LIBS],["$LIBS_save"])
AC_SUBST([JEMALLOC_CPPFLAGS],["$JEMALLOC_INC -DJEMALLOC_NO_DEMANGLE"])
AC_SUBST([JEMALLOC_LIBS],["$JEMALLOC_LIB -ljemalloc -Wl,-rpath,$JEMALLOC_INSTALL_DIR/lib"])

dnl# libpfm

AC_MSG_CHECKING([libpfm installation])
AC_ARG_WITH(libpfm,
	    [AS_HELP_STRING([--with-libpfm=PATH],
	    		    [Absolute path to the install directory for libpfm]
	    		   )],
	    [LIBPFM_INSTALL_DIR="${withval}"],
	    [LIBPFM_INSTALL_DIR=""])

LIBPFM_INC=""
LIBPFM_LIB=""
if test "x$LIBPFM_INSTALL_DIR" = "x" ; then
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
	LIBPFM_INC="-I$LIBPFM_INSTALL_DIR/include -I$LIBPFM_INSTALL_DIR/include/jemalloc"
	LIBPFM_LIB="-L$LIBPFM_INSTALL_DIR/lib"
fi

CPPFLAGS_save="$CPPFLAGS"
LIBS_save="$LIBS"
AC_SUBST([CPPFLAGS],["$LIBPFM_INC $CPPFLAGS_save"])
AC_SUBST([LIBS],["$LIBPFM_LIB $LIBS_save"])
AC_CHECK_HEADERS([perfmon/pfmlib_perf_event.h],[AC_DEFINE(HAVE_LIBPFM)],[AC_MSG_ERROR([Cannot find libpfm headers])])
AC_CHECK_LIB([pfm],[pfm_initialize],,AC_MSG_ERROR([Cannot use libpfm lib]))

# dnl LLVM

AC_MSG_CHECKING([LLVM installation])
AC_ARG_WITH(llvm,
	    [AS_HELP_STRING([--with-llvm=PATH],
	    		    [Absolute path to the install directory for LLVM]
	    		   )],
	    [LLVM_INSTALL_DIR="${withval}"],
	    [LLVM_INSTALL_DIR=""])

LLVM_INC=""
LLVM_LIB=""
if test "x$LLVM_INSTALL_DIR" = "x" ; then
	AC_MSG_RESULT([no])
else
	AC_MSG_RESULT([yes])
	LLVM_INC="-I$LLVM_INSTALL_DIR/include -I$LLVM_INSTALL_DIR/include/llvm"
	LLVM_LIB="-L$LLVM_INSTALL_DIR/lib"
fi

CPPFLAGS_save="$CPPFLAGS"
LIBS_save="$LIBS"
AC_SUBST([CPPFLAGS],["$LLVM_INC $CPPFLAGS_save"])
AC_SUBST([LIBS],["$LLVM_LIB $LIBS_save"])
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS([llvm/Pass.h],[AM_CONDITIONAL([HAVE_LLVM],[true])],[AM_CONDITIONAL([HAVE_LLVM],[false])])
AC_SUBST([SICM_HAS_LLVM])
# Reset CPPFLAGS and LIBS so that we don't link against LLVM by default
AC_SUBST([CPPFLAGS],["$CPPFLAGS_save"]) 
AC_SUBST([LIBS],["$LIBS_save"])
AC_SUBST([LLVM_CPPFLAGS],[`$LLVM_INSTALL_DIR/bin/llvm-config --cxxflags`])
AC_SUBST([LLVM_LIBS],[`$LLVM_INSTALL_DIR/bin/llvm-config --ldflags`])
AC_LANG_POP([C++])

# dnl Generate Makefiles
AC_CONFIG_FILES([Makefile include/Makefile src/Makefile src/low/Makefile src/high/Makefile bin/Makefile])
AC_OUTPUT
