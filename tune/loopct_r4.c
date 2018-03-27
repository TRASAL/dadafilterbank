/*
 * Input:   ntabs nchannels padded_size
 * Output:  ntabs ntimes -nchannels    ; ntimes < padded_size
 *
 * We process a finished tab directly, so no need to build up the full ntabs array
 */

void deinterleave(const char *page, char *transposed, const int ntabs, const int nchannels, const int ntimes, const int padded_size) {
  int tab;
  for (tab = 0; tab < ntabs; tab++) {

    int channel;
#pragma omp parallel for
    for (channel = 0; channel < nchannels; channel+=4) {
      const char *channelA = &page[(tab*nchannels + channel + 0)*padded_size];
      const char *channelB = &page[(tab*nchannels + channel + 1)*padded_size];
      const char *channelC = &page[(tab*nchannels + channel + 2)*padded_size];
      const char *channelD = &page[(tab*nchannels + channel + 3)*padded_size];

      int time;
      for (time = 0; time < ntimes; time++) {

        // reverse freq order to comply with header
        transposed[time*nchannels+nchannels-(channel+0)-1] = channelA[time];
        transposed[time*nchannels+nchannels-(channel+1)-1] = channelB[time];
        transposed[time*nchannels+nchannels-(channel+2)-1] = channelC[time];
        transposed[time*nchannels+nchannels-(channel+3)-1] = channelD[time];
      }
    }
  }
}
