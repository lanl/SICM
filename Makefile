CC?=gcc
FC?=gfortran
CXX?=g++
INCLUDES=sicm_low.h
LOW_SOURCES=sicm_low sicm_arena rbtree
HIGH_SOURCES=sg_fshim sg

JEPATH?=$(HOME)/jemalloc
IDIR=include
CFLAGS=-I$(IDIR) -I$(JEPATH)/include -fPIC -Wall -fopenmp -O2
LDFLAGS=-L$(JEPATH)/lib -lnuma -ljemalloc

LOW_ODIR=obj/low
HIGH_ODIR=obj/high
LOW_SDIR=src/low
HIGH_SDIR=src/high

DEPS=$(patsubst %,$(IDIR)/%,$(INCLUDES))
LOW_OBJ = $(patsubst %,$(LOW_ODIR)/%.o,$(LOW_SOURCES))
HIGH_OBJ = $(patsubst %,$(HIGH_ODIR)/%.o,$(HIGH_SOURCES))

all: sicm sg fortran

sg: $(LOW_OBJ) $(HIGH_OBJ) src/high/sg.f90 src/high/sg.cpp sicm
	$(CC) -o libsg.so obj/high/sg.o $(LOW_OBJ) -shared $(CFLAGS) $(LDFLAGS)
	$(CXX) -o libsgcpp.so src/high/sg.cpp obj/high/sg.o $(LOW_OBJ) -shared $(CFLAGS)
	$(FC) -o libsgf.so src/high/sg.f90 obj/high/sg_fshim.o obj/high/sg.o $(LOW_OBJ) -shared $(CFLAGS)

sicm: $(LOW_OBJ)
	$(CC) -o lib$@.so $^ -shared $(CFLAGS) $(LDFLAGS)

fortran: src/low/fbinding.f90 $(LOW_OBJ) obj/low/fbinding.o
	$(FC) -o libsicm_f90.so src/low/fbinding.f90 obj/low/fbinding.o $(LOW_OBJ) -shared $(CFLAGS) $(LDFLAGS)

obj/low/sicm_cpp.o: src/low/sicm_cpp.cpp include/sicm_cpp.hpp $(LOW_OBJ)
	$(CXX) -o obj/low/sicm_cpp.o -c src/low/sicm_cpp.cpp $(CFLAGS)

cpp: obj/low/sicm_cpp.o $(LOW_OBJ)
	$(CXX) -o libsicm_cpp.so obj/low/sicm_cpp.o $(LOW_OBJ) -shared $(CFLAGS)

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
	rm -rf obj/* *.so

lowdir:
	mkdir -p $(LOW_ODIR)

highdir:
	mkdir -p $(HIGH_ODIR)

$(LOW_ODIR)/%.o: $(LOW_SDIR)/%.c $(DEPS) lowdir
	$(CC) $(CFLAGS) -o $@ -c $<

$(HIGH_ODIR)/%.o: $(HIGH_SDIR)/%.c $(DEPS) highdir
	$(CC) $(CFLAGS) -o $@ -c $<
