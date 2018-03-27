void deinterleave(char *page, char *transposed, const int ntabs, const int nchannels, const int ntimes, const int padded_size) {
  int tab;
  for (tab = 0; tab < ntabs; tab++) {

    int time;
#pragma omp parallel for
    for (time = 0; time < ntimes; time++) {

      int channel;
      for (channel = 0; channel < nchannels; channel++) {

        // reverse freq order to comply with header
        transposed[time*nchannels+nchannels-channel-1] = page[(tab*nchannels + channel) * padded_size + time];
      }
    }
  }
}
