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

  // Filterbank header from page 4 of http://sigproc.sourceforge.net/sigproc.pdf, retreived 2017-05-31
  put_raw_string(fd, "HEADER_START");
  put_int(fd, "telescope_id", telescope_id);
  put_int(fd, "machine_id", machine_id);
  put_int(fd, "data_type", 1); // 1: filterbank data, 2: time series dada, DM=0...

  // rawdatafile (char []): the name of the original data file
  // In our case, this can be longer than 80 characters, which several readers cannot handle.
  // A filterbank file is valid without this field
  // put_string(fd, "rawdatafile", file_name);

  put_string(fd, "source_name", source_name); // the name of the source being observed by the telescope
  put_int(fd, "barycentric", 0); // 0: no, 1: yes
  put_int(fd, "pulsarcentric", 0); // 0: no, 1: yes
  put_double(fd, "az_start", az_start); // telescope azimuth at start of scan (degrees)
  put_double(fd, "za_start", za_start); // telescope zenith angle at start of scan (degrees)
  put_double(fd, "src_raj", src_raj); // right ascension (J2000) of source (hhmmss.s)
  put_double(fd, "src_dej", src_dej); // declination (J2000) of source (ddmmss.s)
  put_double(fd, "tstart", tstart); // time stamp (MJD) of first sample
  put_double(fd, "tsamp", tsamp); // time interval between samples (s)
  put_int(fd, "nbits", nbits); // number of bits per time sample
  put_double(fd, "fch1", fch1); // centre frequency (MHz) of first filterbank channel
  put_double(fd, "foff", foff); // filterbank channel bandwidth (MHz)
  put_int(fd, "nchans", nchans); // number of filterbank channels
  put_int(fd, "nbeams", nbeams); // NOT DOCUMENTED BUT IN USE IN THE SIGPROC CODE
  put_int(fd, "ibeam", ibeam); // NOT DOCUMENTED BUT IN USE IN THE SIGPROC CODE
  put_int(fd, "nifs", nifs); // number of seperate IF channels
  put_raw_string(fd, "HEADER_END");

  return fd;
}
