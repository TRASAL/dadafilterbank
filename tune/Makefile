TARGETS=loopct loopct_r2 loopct_r4 loopct_r6 looptc looptc_c1 looptc_c2 looptc_c4 looptc_c6

all: $(TARGETS)

%: %.c main.c
	gcc -march=native -O3 -Ofast -fstrict-aliasing -fopenmp -o $@ -std=c99 main.c $<

time:
	export OMP_NUM_THREADS=2
	for i in $(TARGETS); do echo -n $$i && ./$$i 12 1536 25000 25088; done
