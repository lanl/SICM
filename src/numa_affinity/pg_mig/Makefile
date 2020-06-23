CC = gcc
LDFLAGS = -fopenmp -lnuma -lm -w 

pgmg: test.c page_migration_test.c page_migration_test.h
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) pgmg
