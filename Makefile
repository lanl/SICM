INCLUDES := sicm_low.h
SOURCES := sicm_low

IDIR := include
CFLAGS := -I$(IDIR) -fPIC -Wall -fopenmp -O2 -lnuma

ODIR := obj
SDIR := src

DEPS := $(patsubst %,$(IDIR)/%,$(INCLUDES))

OBJ = $(patsubst %,$(ODIR)/%.o,$(SOURCES))

sg: $(OBJ) obj/sg.o sicm
	gcc -o libsg.so obj/sg.o $(OBJ) -shared $(CFLAGS)

sicm: $(OBJ)
	gcc -o lib$@.so $^ -shared $(CFLAGS)

fortran: src/fbinding.f90 $(OBJ) obj/fbinding.o
	gfortran -o sicm_f90.so src/fbinding.f90 obj/fbinding.o $(OBJ) -shared $(CFLAGS)

obj/sicm_cpp.o: src/sicm_cpp.cpp include/sicm_cpp.hpp $(OBJ)
	g++ -o obj/sicm_cpp.o -c src/sicm_cpp.cpp $(CFLAGS)

cpp: obj/sicm_cpp.o $(OBJ)
	g++ -o libsicm_cpp.so obj/sicm_cpp.o $(OBJ) -shared $(CFLAGS)

.PHONY: examples

examples: libsicm.so
	gcc -o examples/basic examples/basic.c -L. -lsicm $(CFLAGS)
	gcc -o examples/hugepages examples/hugepages.c -L. -lsicm $(CFLAGS)
	g++ -o examples/class examples/class.cpp -L. -lsicm_cpp $(CFLAGS)
	g++ -o examples/stl examples/stl.cpp -L. -lsicm_cpp $(CFLAGS)
	gcc -o examples/greedy examples/greedy.c -L. -lsg $(CFLAGS)

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<
