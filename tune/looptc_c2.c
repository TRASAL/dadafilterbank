#include <string.h>

void deinterleave(char *page, char *transposed, const int ntabs, const int nchannels, const int ntimes, const int padded_size) {
  int tab;
  for (tab = 0; tab < ntabs; tab++) {

    // unroll time dimension once
    int time;
    for (time = 0; time < ntimes; time+=2) {

      // build temporary array containing 2 complete channel rows
      char temp[2 * nchannels];

      int channel;
#pragma omp parallel for
      for (channel = 0; channel < nchannels; channel++) {
        // reverse freq order to comply with header
        temp[1*nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + (time + 0)];
        temp[2*nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + (time + 1)];
      }

      // copy 2 full row at once
      memcpy(&transposed[time*nchannels], temp, 2*nchannels);
    }
  }
}
