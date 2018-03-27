TARGETS=loopct loopct_r2 loopct_r4 looptc looptc_c1 looptc_c2 looptc_c4

all: $(TARGETS)

%: %.c main.c
	gcc -march=native -O3 -Ofast -fstrict-aliasing -fopenmp -o $@ -std=c99 main.c $<

time:
	for i in $(TARGETS); do echo -n $$i && ./$$i 12 1536 25000 25088; done
