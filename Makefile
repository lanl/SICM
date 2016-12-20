INCLUDES := sicm_low.h
SOURCES := sicm_low

IDIR := include
CFLAGS := -I$(IDIR) -fPIC -Wall -fopenmp -O2 -lnuma

ODIR := obj
SDIR := src

DEPS := $(patsubst %,$(IDIR)/%,$(INCLUDES))

OBJ = $(patsubst %,$(ODIR)/%.o,$(SOURCES))

sicm: $(OBJ)
	gcc -o lib$@.so $^ -shared $(CFLAGS)
	
fortran: src/fbinding.f90 $(OBJ) obj/fbinding.o
	gfortran -o sicm_f90.so src/fbinding.f90 obj/fbinding.o $(OBJ) -shared $(CFLAGS)

examples: sicm
	gcc -o examples/basic examples/basic.c -L. -lsicm $(CFLAGS)

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<
