JEPATH?=$(HOME)/jemalloc

CC?=gcc
CFLAGS=-Wall -g -I$(JEPATH)/include -I../include -DJEMALLOC_NO_RENAME
LFLAGS=-L.. -L$(JEPATH)/lib -lsicm -ljemalloc -lnuma -pthread
ALL=test1 test2 test3 test4 test5

.PHONY: all clean

all: $(ALL)

$(ALL): %: Makefile %.o
	$(CC) -o $@ $(CFLAGS) $@.o $(LFLAGS)

clean:
	rm -f *.o *.a *~ $(ALL)

%.o: %.c
	$(CC) $(CFLAGS) -o $*.o -c $*.c
