CC = nvcc
LDFLAGS = -lnuma -lm -gencode=arch=compute_75,code=compute_75

gpu_numa: cpu_gpu_alloc.cu 
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) gpu_numa
