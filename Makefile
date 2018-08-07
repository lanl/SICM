# Compilers
CC?=gcc
FC?=gfortran
CXX?=g++

# Source files
LOW_SOURCES=sicm_low sicm_arena rbtree
HIGH_SOURCES=high profile
INCLUDES=sicm_low.h

# External dependencies, set these to let the Makefile find them
JELIB?=./jemalloc-5.1.0/lib
JEINC?=./jemalloc-5.1.0/include
LLVMPATH?=/usr/lib/llvm-4.0
LLVMFLAGS=$(shell $(LLVMPATH)/bin/llvm-config --cxxflags)
LLVMLIBS=$(shell $(LLVMPATH)/bin/llvm-config --ldflags)$(shell $(LLVMPATH)/bin/llvm-config --libs)

# Local directories
IDIR=include
LIBDIR=lib
ODIR=obj
SDIR=src
LOW_ODIR=$(ODIR)/low
HIGH_ODIR=$(ODIR)/high
LOW_SDIR=$(SDIR)/low
HIGH_SDIR=$(SDIR)/high

# Flags
CFLAGS=-I$(IDIR) -I$(JEINC) -fPIC -Wall -fopenmp -O2 -g 
LDFLAGS=-L$(JELIB) -lnuma -ljemalloc -Wl,-rpath,$(realpath $(JELIB))
HIGHLDFLAGS=-lpfm

# Generated targets
DEPS=$(patsubst %,$(IDIR)/%,$(INCLUDES))
LOW_OBJ = $(patsubst %,$(LOW_ODIR)/%.o,$(LOW_SOURCES))
HIGH_OBJ = $(patsubst %,$(HIGH_ODIR)/%.o,$(HIGH_SOURCES))

all: sicm fortran

# Make sure all directories exist
libdir:
	mkdir -p $(LIBDIR)
lowdir:
	mkdir -p $(LOW_ODIR)
highdir:
	mkdir -p $(HIGH_ODIR)

high: $(LOW_OBJ) $(HIGH_OBJ) sicm libdir
	$(CC) -o $(LIBDIR)/libhigh.so obj/high/high.o obj/high/profile.o $(HIGHLDFLAGS) $(LOW_OBJ) -shared $(CFLAGS) $(LDFLAGS)

sicm: $(LOW_OBJ)
	$(CC) -o $(LIBDIR)/lib$@.so $^ -shared $(CFLAGS) $(LDFLAGS)

fortran: $(LOW_SDIR)/fbinding.f90 $(LOW_OBJ) $(LOW_ODIR)/fbinding.o libdir
	$(FC) -o $(LIBDIR)/libsicm_f90.so $(LOW_SDIR)/fbinding.f90 $(LOW_ODIR)/fbinding.o $(LOW_OBJ) -shared $(CFLAGS) $(LDFLAGS)

obj/low/sicm_cpp.o: $(LOW_SDIR)/sicm_cpp.cpp include/sicm_cpp.hpp $(LOW_OBJ)
	$(CXX) -o $(LOW_ODIR)/sicm_cpp.o -c $(LOW_SDIR)/sicm_cpp.cpp $(CFLAGS)

cpp: $(LOW_ODIR)/sicm_cpp.o $(LOW_OBJ) libdir
	$(CXX) -o $(LIBDIR)/libsicm_cpp.so $(LOW_ODIR)/sicm_cpp.o $(LOW_OBJ) -shared $(CFLAGS)

compass:
	$(LLVMPATH)/bin/clang++ $(LLVMFLAGS) $(LLVMLIBS) -o $(LIBDIR)/compass.so $(CFLAGS) -shared $(HIGH_SDIR)/compass.cpp

.PHONY: examples

examples: sicm cpp
	$(CC) -o examples/basic examples/basic.c -L. -lsicm $(CFLAGS) $(LDFLAGS)
	$(CC) -o examples/hugepages examples/hugepages.c -L. -lsicm $(CFLAGS) $(LDFLAGS)
	$(CXX) -o examples/class examples/class.cpp -L. -lsicm_cpp $(CFLAGS) $(LDFLAGS)
	$(CXX) -o examples/stl examples/stl.cpp -L. -lsicm_cpp $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf $(ODIR)/* $(LIBDIR)/*

$(LOW_ODIR)/%.o: $(LOW_SDIR)/%.c $(DEPS) lowdir
	$(CC) $(CFLAGS) -o $@ -c $<

$(HIGH_ODIR)/%.o: $(HIGH_SDIR)/%.c $(DEPS) highdir
	$(CC) $(CFLAGS) -o $@ -c $<
