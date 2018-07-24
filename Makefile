# Compilers
CC?=gcc
FC?=gfortran
CXX?=g++

# Source files
LOW_SOURCES=sicm_low sicm_arena rbtree
HIGH_SOURCES=sg_fshim sg high
INCLUDES=sicm_low.h

# External dependencies, set these to let the Makefile find them
JEPATH?=$(HOME)/jemalloc
LLVMPATH?=/usr/lib/llvm-6.0

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
CFLAGS=-I$(IDIR) -I$(JEPATH)/include -fPIC -Wall -fopenmp -O2 -g 
LDFLAGS=-L$(JEPATH)/lib -lnuma -ljemalloc -Wl,-rpath,$(realpath $(JEPATH)/lib)

# Generated targets
DEPS=$(patsubst %,$(IDIR)/%,$(INCLUDES))
LOW_OBJ = $(patsubst %,$(LOW_ODIR)/%.o,$(LOW_SOURCES))
HIGH_OBJ = $(patsubst %,$(HIGH_ODIR)/%.o,$(HIGH_SOURCES))

all: sicm sg fortran compass

# Make sure all directories exist
libdir:
	mkdir -p $(LIBDIR)
lowdir:
	mkdir -p $(LOW_ODIR)
highdir:
	mkdir -p $(HIGH_ODIR)

sg: $(LOW_OBJ) $(HIGH_OBJ) $(HIGH_SDIR)/sg.f90 $(HIGH_SDIR)/sg.cpp sicm libdir
	$(CC) -o $(LIBDIR)/libhigh.so obj/high/high.o $(LOW_OBJ) -shared $(CFLAGS) $(LDFLAGS)
	$(CC) -o $(LIBDIR)/libsg.so obj/high/sg.o $(LOW_OBJ) -shared $(CFLAGS) $(LDFLAGS)
	$(CXX) -o $(LIBDIR)/libsgcpp.so $(HIGH_SDIR)/sg.cpp obj/high/sg.o $(LOW_OBJ) -shared $(CFLAGS)
	$(FC) -o $(LIBDIR)/libsgf.so $(HIGH_SDIR)/sg.f90 obj/high/sg_fshim.o obj/high/sg.o $(LOW_OBJ) -shared $(CFLAGS)

sicm: $(LOW_OBJ)
	$(CC) -o $(LIBDIR)/lib$@.so $^ -shared $(CFLAGS) $(LDFLAGS)

fortran: $(LOW_SDIR)/fbinding.f90 $(LOW_OBJ) $(LOW_ODIR)/fbinding.o libdir
	$(FC) -o $(LIBDIR)/libsicm_f90.so $(LOW_SDIR)/fbinding.f90 $(LOW_ODIR)/fbinding.o $(LOW_OBJ) -shared $(CFLAGS) $(LDFLAGS)

obj/low/sicm_cpp.o: $(LOW_SDIR)/sicm_cpp.cpp include/sicm_cpp.hpp $(LOW_OBJ)
	$(CXX) -o $(LOW_ODIR)/sicm_cpp.o -c $(LOW_SDIR)/sicm_cpp.cpp $(CFLAGS)

cpp: $(LOW_ODIR)/sicm_cpp.o $(LOW_OBJ) libdir
	$(CXX) -o $(LIBDIR)/libsicm_cpp.so $(LOW_ODIR)/sicm_cpp.o $(LOW_OBJ) -shared $(CFLAGS)

compass:
	$(CXX) $(CFLAGS) -I$(LLVMPATH)/include -Wl,-rpath,"$(LLVMPATH)/lib" -shared -o $(LIBDIR)/compass.so $(HIGH_SDIR)/compass.cpp

.PHONY: examples

examples: sicm sg cpp
	$(CC) -o examples/basic examples/basic.c -L. -lsicm $(CFLAGS) $(LDFLAGS)
	$(CC) -o examples/hugepages examples/hugepages.c -L. -lsicm $(CFLAGS) $(LDFLAGS)
	$(CXX) -o examples/class examples/class.cpp -L. -lsicm_cpp $(CFLAGS) $(LDFLAGS)
	$(CXX) -o examples/stl examples/stl.cpp -L. -lsicm_cpp $(CFLAGS) $(LDFLAGS)
	$(CC) -o examples/greedy examples/greedy.c -L. -lsg $(CFLAGS) $(LDFLAGS)
	$(CXX) -o examples/greedypp examples/greedypp.cpp -L. -lsgcpp $(CFLAGS) $(LDFLAGS)
	$(FC) -o examples/greedyf examples/greedyf.f90 -L. -lsgf $(CFLAGS) $(LDFLAGS)
	$(CC) -o examples/simple_knl_test examples/simple_knl_test.c -L. -lsg $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf $(ODIR)/* $(LIBDIR)/*

$(LOW_ODIR)/%.o: $(LOW_SDIR)/%.c $(DEPS) lowdir
	$(CC) $(CFLAGS) -o $@ -c $<

$(HIGH_ODIR)/%.o: $(HIGH_SDIR)/%.c $(DEPS) highdir
	$(CC) $(CFLAGS) -o $@ -c $<
