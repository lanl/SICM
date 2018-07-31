par=""
for i in $@
do 
	par="$par $i"	
done
gcc -o numatest src/numatest.c -lnuma -lrt -lm -std=gnu99
./numatest $par
