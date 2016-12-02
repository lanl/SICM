INCLUDES := sicm_low.h dram.h knl_hbm.h numa_common.h
SOURCES := sicm_low dram knl_hbm numa_common

IDIR := include
CFLAGS := -I$(IDIR) -Wall -lnuma -fopenmp -O2

ODIR := obj
SDIR := src

DEPS := $(patsubst %,$(IDIR)/%,$(INCLUDES))

OBJ = $(patsubst %,$(ODIR)/%.o,$(SOURCES))

sicm: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<
