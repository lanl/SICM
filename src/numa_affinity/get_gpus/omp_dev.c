#include <stdio.h>
#include <omp.h>

int main(){
	printf("%d", omp_get_num_devices());
	return 1;
}
