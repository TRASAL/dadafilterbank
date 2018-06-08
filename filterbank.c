/**
 * Filterbank header from page 4 of http://sigproc.sourceforge.net/sigproc.pdf, retreived 2017-05-31
 *
 * telescope id (int): 0=fake data; 1=Arecibo; 2=Ooty... others to be added
 * machine id (int): 0=FAKE; 1=PSPM; 2=WAPP; 3=OOTY... others to be added
 * data type (int): 1=filterbank; 2=time series... others to be added
 * rawdatafile (char []): the name of the original data file
 * source name (char []): the name of the source being observed by the telescope
 * barycentric (int): equals 1 if data are barycentric or 0 otherwise
 * pulsarcentric (int): equals 1 if data are pulsarcentric or 0 otherwise
 * az start (double): telescope azimuth at start of scan (degrees)
 * za start (double): telescope zenith angle at start of scan (degrees)
 * src raj (double): right ascension (J2000) of source (hhmmss.s)
 * src dej (double): declination (J2000) of source (ddmmss.s)
 * tstart (double): time stamp (MJD) of first sample
 * tsamp (double): time interval between samples (s)
 * nbits (int): number of bits per time sample
 * nsamples (int): number of time samples in the data file (rarely used any more)
 * fch1 (double): centre frequency (MHz) of first filterbank channel
 * foff (double): filterbank channel bandwidth (MHz)
 * FREQUENCY START (character): start of frequency table (see below for explanation)
 * fchannel (double): frequency channel value (MHz)
 * FREQUENCY END (character): end of frequency table (see below for explanation)
 * nchans (int): number of filterbank channels
 * nifs (int): number of seperate IF channels
 * refdm (double): reference dispersion measure (cm−3 pc)
 * period (double): folding period (s)
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "filterbank.h"

static void put_raw_string(int fd, char *string) {
  int len = strlen(string);
  ssize_t size;
  size = write(fd, &len, sizeof(int));
  size = write(fd, string, sizeof(char) * len);
}

static void put_string(int fd, char *name, char *value) {
  put_raw_string(fd, name);
  put_raw_string(fd, value);
}

static void put_double(int fd, char *name, double value) {
  put_raw_string(fd, name);
  ssize_t size = write(fd, &value, sizeof(double));
}

static void put_int(int fd, char *name, int value) {
  put_raw_string(fd, name);
  ssize_t size = write(fd, &value, sizeof(int));
}

void filterbank_close(int fd) {
  close(fd);
}

int filterbank_create(
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
    int nifs) {
  int fd = open(file_name, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

  put_raw_string(fd, "HEADER_START");
  put_int(fd, "telescope_id", telescope_id);
  put_int(fd, "machine_id", machine_id);
  put_int(fd, "data_type", 1); // 1: filterbank data, 2: time series dada, DM=0
  put_string(fd, "rawdatafile", file_name);
  put_string(fd, "source_name", source_name);
  put_int(fd, "barycentric", 0); // 0: no, 1: yes
  put_int(fd, "pulsarcentric", 0); // 0: no, 1: yes
  put_double(fd, "az_start", az_start);
  put_double(fd, "za_start", za_start);
  put_double(fd, "src_raj", src_raj);
  put_double(fd, "src_dej", src_dej);
  put_double(fd, "tstart", tstart);
  put_double(fd, "tsamp", tsamp);
  put_int(fd, "nbits", nbits);
  put_double(fd, "fch1", fch1);
  put_double(fd, "foff", foff);
  put_int(fd, "nchans", nchans);
  put_int(fd, "nbeams", nbeams); // NOT DOCUMENTED BUT IN USE IN THE SIGPROC CODE
  put_int(fd, "ibeam", ibeam); // NOT DOCUMENTED BUT IN USE IN THE SIGPROC CODE
  put_int(fd, "nifs", nifs);
  put_raw_string(fd, "HEADER_END");

  return fd;
}
