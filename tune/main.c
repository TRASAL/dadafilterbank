#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>
#include <sys/mman.h>

int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "Need 4 arguments: ntabs, nchannels, ntimes, padded_size\n");
    exit(EXIT_FAILURE);
  }

  int ntabs = atoi(argv[1]);
  int nchannels = atoi(argv[2]);
  int ntimes = atoi(argv[3]);
  int padded_size = atoi(argv[4]);

  if (padded_size < ntimes || ntabs <= 0 || nchannels <= 0 || ntimes <= 0) {
    fprintf(stderr, "Illegal parameter values\n");
    exit(EXIT_FAILURE);
  }

  size_t mysize = ntabs * nchannels * padded_size;
  printf("% 4i % 4i % 4i % 4i %6.2fMB ", ntabs, nchannels, ntimes, padded_size, mysize / (1024.0*1024.0));

  char *transposed = (char *)malloc(mysize);
  char *page = (char *)malloc(mysize);

  // prevent moving memory pages to swap
  mlock(page, mysize);
  mlock(transposed, mysize);

  double start = omp_get_wtime();

  int i;
  for (i=0; i<10; i++) {
    deinterleave(page, transposed, ntabs, nchannels, ntimes, padded_size);
    // memcpy(page, transposed, mysize);
  }

  double end = omp_get_wtime();

  printf("%.6f ms\n", (end - start)*1e3/10);

  free(page);
  free(transposed);

  exit(EXIT_SUCCESS);
}
