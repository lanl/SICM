CC?=gcc
FC?=gfortran
CXX?=g++
INCLUDES=sicm_low.h
SOURCES=sicm_low sicm_arena rbtree

JEPATH?=$(HOME)/jemalloc
IDIR=include
CFLAGS=-I$(IDIR) -I$(JEPATH)/include -fPIC -Wall -fopenmp -O2
LDFLAGS=-L$(JEPATH)/lib -lnuma -ljemalloc

ODIR=obj
SDIR=src

DEPS=$(patsubst %,$(IDIR)/%,$(INCLUDES))

OBJ = $(patsubst %,$(ODIR)/%.o,$(SOURCES))

all: sicm sg fortran

sg: $(OBJ) obj/sg_fshim.o obj/sg.o src/sg.f90 src/sg.cpp sicm
	$(CC) -o libsg.so obj/sg.o $(OBJ) -shared $(CFLAGS) $(LDFLAGS)
	$(CXX) -o libsgcpp.so src/sg.cpp obj/sg.o $(OBJ) -shared $(CFLAGS)
	$(FC) -o libsgf.so src/sg.f90 obj/sg_fshim.o obj/sg.o $(OBJ) -shared $(CFLAGS)

sicm: $(OBJ)
	$(CC) -o lib$@.so $^ -shared $(CFLAGS) $(LDFLAGS)

fortran: src/fbinding.f90 $(OBJ) obj/fbinding.o
	$(FC) -o libsicm_f90.so src/fbinding.f90 obj/fbinding.o $(OBJ) -shared $(CFLAGS) $(LDFLAGS)

obj/sicm_cpp.o: src/sicm_cpp.cpp include/sicm_cpp.hpp $(OBJ)
	$(CXX) -o obj/sicm_cpp.o -c src/sicm_cpp.cpp $(CFLAGS)

cpp: obj/sicm_cpp.o $(OBJ)
	$(CXX) -o libsicm_cpp.so obj/sicm_cpp.o $(OBJ) -shared $(CFLAGS)

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

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<
