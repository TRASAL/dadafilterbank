#include <string.h>

void deinterleave(char *page, char *transposed, const int ntabs, const int nchannels, const int ntimes, const int padded_size) {
  int tab;
  for (tab = 0; tab < ntabs; tab++) {

    int time;
    for (time = 0; time < ntimes; time++) {

      // build temporary array containing a complete channel row
      char temp[nchannels];

      int channel;
#pragma omp parallel for
      for (channel = 0; channel < nchannels; channel++) {
        // reverse freq order to comply with header
        temp[nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + time];
      }

      // copy full row at once
      memcpy(&transposed[time*nchannels], temp, nchannels);
    }
  }
}
