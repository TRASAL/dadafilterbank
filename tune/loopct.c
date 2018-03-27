void deinterleave(char *page, char *transposed, const int ntabs, const int nchannels, const int ntimes, const int padded_size) {
  int tab;
  for (tab = 0; tab < ntabs; tab++) {

    int channel;
#pragma omp parallel for
    for (channel = 0; channel < nchannels; channel++) {

      int time;
      for (time = 0; time < ntimes; time++) {

        // reverse freq order to comply with header
        transposed[time*nchannels+nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + time];
      }
    }
  }
}
