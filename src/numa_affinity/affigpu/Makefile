CC = nvcc
LDFLAGS = -O2 -lnuma -lm -gencode=arch=compute_75,code=compute_75 -w

affigpu: test2.cu affigpu.cu
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) affigpu
