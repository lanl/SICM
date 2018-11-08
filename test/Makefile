JEPATH?=$(HOME)/jemalloc
SICM_ROOT?=$(HOME)/SICM

CC?=gcc
CFLAGS=-Wall -std=c11 -g -I$(JEPATH)/include -I$(SICM_ROOT)/include -DJEMALLOC_NO_RENAME
LFLAGS=-L$(SICM_ROOT)/lib -L$(JEPATH)/lib -lsicm -ljemalloc -lnuma -pthread -Wl,-rpath,$(realpath $(JEPATH)/lib) -Wl,-rpath,$(realpath $(SICM_ROOT)/lib)
ALL=test1 test2 test3 test4 test5

.PHONY: all clean

all: $(ALL)

$(ALL): %: Makefile %.o
	$(CC) -o $@ $(CFLAGS) $@.o $(LFLAGS)

run-test%: test%
	./$^

clean:
	rm -f *.o *.a *~ $(ALL)

%.o: %.c
	$(CC) $(CFLAGS) -o $*.o -c $*.c
