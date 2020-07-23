CC = gcc
LDFLAGS = -fopenmp -lnuma -lm -w 

affinuma: test2.c affinuma.c affinuma.h
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) affinuma
