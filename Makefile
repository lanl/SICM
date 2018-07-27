# Compilers
CC?=gcc
FC?=gfortran
CXX?=g++

# Source files
INCLUDES=sicm_low.h

# External dependencies, set these to let the Makefile find them
JEPATH?=$(HOME)/jemalloc
LLVMPATH?=/usr/lib/llvm-3.9

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

.PHONY: $(LIBDIR) $(LOW_ODIR) $(HIGH_ODIR) dirs examples

all: $(LIBDIR)/libsicm.so $(LIBDIR)/libsicm_cpp.so $(LIBDIR)/libsicm_f90.so \
     $(LIBDIR)/libsg.so $(LIBDIR)/libsgcpp.so $(LIBDIR)/libsgf.so           \
     $(LIBDIR)/libhigh.so                                                   \
     $(LIBDIR)/compass.so

# Make sure all directories exist
$(shell mkdir -p $(LIBDIR) $(LOW_ODIR) $(HIGH_ODIR))

$(LIBDIR)/libhigh.so: $(HIGH_ODIR)/high.o
	$(CC) -o $@ $^ -shared $(CFLAGS) $(LDFLAGS)

$(LIBDIR)/libsg.so: obj/high/sg.o
	$(CC) -o $@ $^ -shared $(CFLAGS) $(LDFLAGS)

$(LIBDIR)/libsgcpp.so: $(HIGH_SDIR)/sg.cpp obj/high/sg.o
	$(CXX) -o $@ $^ -shared $(CFLAGS)

$(LIBDIR)/libsgf.so: $(HIGH_SDIR)/sg.f90 obj/high/sg_fshim.o obj/high/sg.o
	$(FC) -o $@ $^ -shared $(CFLAGS) -J $(LIBDIR)

$(LIBDIR)/libsicm.so: $(LOW_ODIR)/sicm_low.o $(LOW_ODIR)/sicm_arena.o $(LOW_ODIR)/rbtree.o
	$(CC) -o $@ $^ -shared $(CFLAGS) $(LDFLAGS)

$(LIBDIR)/libsicm_f90.so: $(LOW_SDIR)/fbinding.f90 $(LOW_ODIR)/fbinding.o
	$(FC) -o $@ $^ -shared $(CFLAGS) -J $(LIBDIR) $(LDFLAGS)

$(LIBDIR)/libsicm_cpp.so: $(LOW_ODIR)/sicm_cpp.o
	$(CXX) -o $@ $^ -shared $(CFLAGS)

$(LIBDIR)/compass.so:
	$(CXX) $(CFLAGS) -I$(LLVMPATH)/include -Wl,-rpath,"$(LLVMPATH)/lib" -shared -o $@ $(HIGH_SDIR)/compass.cpp

examples:
	$(MAKE) -C examples

clean:
	rm -rf $(ODIR)/* $(LIBDIR)/*

$(LOW_ODIR)/%.o: $(LOW_SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(HIGH_ODIR)/%.o: $(HIGH_SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(LOW_ODIR)/%.o: $(LOW_SDIR)/%.cpp $(DEPS)
	$(CXX) $(CFLAGS) -o $@ -c $<

$(HIGH_ODIR)/%.o: $(HIGH_SDIR)/%.cpp $(DEPS)
	$(CXX) $(CFLAGS) -o $@ -c $<
