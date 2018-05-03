JEPATH=$(HOME)/jemalloc

CC=gcc
CFLAGS=-Wall -g -I$(JEPATH)/include -I../include -DJEMALLOC_NO_RENAME
LFLAGS=-L.. -L$(HOME)/jemalloc/lib -lsicm -ljemalloc -lnuma -pthread
ALL=test1 test2 test3

.PHONY: all clean

all: $(ALL)

test1: test1.o
	$(CC) -o $@ $(CFLAGS) $@.c $(LFLAGS)

test2: test2.o
	$(CC) -o $@ $(CFLAGS) $@.c $(LFLAGS)

test3: test3.o
	$(CC) -o $@ $(CFLAGS) $@.c $(LFLAGS)

clean:
	rm -f *.o *.a *~ $(ALL)

$(ALL): Makefile

%.o: %.c
	$(CC) $(CFLAGS) -o $*.o -c $*.c
