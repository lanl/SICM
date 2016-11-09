INCLUDES := sicm_low.h dram.h
SOURCES := sicm_low dram

IDIR := include
CFLAGS := -I$(IDIR) -Wall -lnuma

ODIR := obj
SDIR := src

DEPS := $(patsubst %,$(IDIR)/%,$(INCLUDES))

OBJ = $(patsubst %,$(ODIR)/%.o,$(SOURCES))

sicm: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<
