#include <string.h>

void deinterleave(char *page, char *transposed, const int ntabs, const int nchannels, const int ntimes, const int padded_size) {
  int tab;
  for (tab = 0; tab < ntabs; tab++) {

    // unroll time dimension 4x
    int time;
    for (time = 0; time < ntimes; time+=6) {

      // build temporary array containing 4 complete channel rows
      char temp[6 * nchannels];

      int channel;
#pragma omp parallel for
      for (channel = 0; channel < nchannels; channel++) {
        // reverse freq order to comply with header
        temp[1*nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + (time + 0)];
        temp[2*nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + (time + 1)];
        temp[3*nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + (time + 2)];
        temp[4*nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + (time + 3)];
        temp[5*nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + (time + 4)];
        temp[6*nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + (time + 5)];
      }

      // copy 6 full rows at once, except for the last iteration
      if (time + 6 < ntimes) {
        memcpy(&transposed[time*nchannels], temp, 6*nchannels);
      } else {
        // deal with the last incomplete batch
        memcpy(&transposed[time*nchannels], temp, (ntimes-time)*nchannels);
      }
    }
  }
}
