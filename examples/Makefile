# Compilers
CC?=gcc
FC?=gfortran
CXX?=g++

# External dependencies, set these to let the Makefile find them
JEPATH?=$(HOME)/jemalloc

# Local directories
IDIR=../include
LIBDIR=../lib

# Flags
CFLAGS=-I$(IDIR) -I$(JEPATH)/include -Wall -fopenmp -O2 -g
LDFLAGS=-L$(LIBDIR) -L$(JEPATH)/lib -lnuma -ljemalloc -Wl,-rpath,$(realpath $(JEPATH)/lib)

TARGETS=basic hugepages class stl greedy greedypp greedyf simple_knl_test

all: $(TARGETS)

$(LIBDIR)/lib*.so:
	$(MAKE) -C ..

basic: basic.c $(LIBDIR)/libsicm.so
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -lsicm

hugepages: hugepages.c $(LIBDIR)/libsicm.so
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -lsicm

class: class.cpp $(LIBDIR)/libsicm.so $(LIBDIR)/libsicm_cpp.so
	$(CXX) -o $@ $< $(CFLAGS) $(LDFLAGS) -lsicm_cpp  -lsicm

stl: stl.cpp $(LIBDIR)/libsicm.so $(LIBDIR)/libsicm_cpp.so
	$(CXX) -o $@ $< $(CFLAGS) $(LDFLAGS) -lsicm_cpp  -lsicm

greedy: greedy.c $(LIBDIR)/libsg.so
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -lsg -lsicm

greedypp: greedypp.cpp $(LIBDIR)/libsicm.so $(LIBDIR)/libsgcpp.so
	$(CXX) -o $@ $< $(CFLAGS) $(LDFLAGS) -lsgcpp -lsicm

greedyf: greedyf.f90 $(LIBDIR)/libsicm.so $(LIBDIR)/libsgf.so
	$(FC) -o $@ $< $(CFLAGS) -I.. $(LDFLAGS) -lsgf -lsicm

simple_knl_test: simple_knl_test.c $(LIBDIR)/libsicm.so $(LIBDIR)/libsg.so
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -lsg -lsicm

run-%: %
	LD_LIBRARY_PATH=$(LIBDIR):$(LD_LIBRARY_PATH) ./$<

clean:
	rm -rf $(TARGETS)
