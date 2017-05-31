#ifndef __HAVE_FILTERBANK_H__
#define __HAVE_FILTERBANK_H__

#include <stdio.h>

extern void filterbank_close(FILE *file);

extern FILE *filterbank_create(
    char *file_name,
    int telescope_id,
    int machine_id,
    char *source_name,
    double az_start,
    double za_start,
    double src_raj,
    double src_dej,
    double tstart,
    double tsamp,
    int nbits,
    double fch1,
    double foff,
    int nchans,
    int nbeams,
    int ibeam,
    int nifs);
#endif
