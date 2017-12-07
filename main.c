/**
 * program: dadafilterbank
 *
 * Purpose: connect to a ring buffer and create Sigproc output per TAB on request
 * 
 *    A ringbuffer page is interpreted as an array of Stokes I:
 *    [NTABS, NCHANNELS, padded_size] = [12, 1536, > 25000]
 *
 *    Written for the AA-Alert project, ASTRON
 *
 * Author: Jisk Attema, Netherlands eScience Center
 * Licencse: Apache v2.0
 */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>

#include "dada_hdu.h"
#include "ascii_header.h"
#include "filterbank.h"

FILE *runlog = NULL;
#define LOG(...) {fprintf(stdout, __VA_ARGS__); fprintf(runlog, __VA_ARGS__); fflush(stdout); fflush(runlog);}

#define NCHANNELS 1536
#define MAXTABS 12
int output[MAXTABS];

unsigned int nchannels;
double min_frequency;
double bandwidth;
double channel_bandwidth;
double tsamp;
double ra;
double dec;
char source_name[256];
double az_start;
double za_start;
double mjd_start;
unsigned int nbit;
int ntimes;
int ntabs;

/**
 * Open a connection to the ringbuffer
 *
 * @param {char *} key String containing the shared memory key as hexadecimal number
 * @returns {hdu *} A connected HDU
 */
dada_hdu_t *init_ringbuffer(char *key) {
  uint64_t nbufs;

  multilog_t* multilog = NULL; // TODO: See if this is used in anyway by dada

  // create hdu
  dada_hdu_t *hdu = dada_hdu_create (multilog);

  // init key
  key_t shmkey;
  sscanf(key, "%x", &shmkey);
  dada_hdu_set_key(hdu, shmkey);
  LOG("dadafilterbank SHMKEY: %s\n", key);

  // connect
  if (dada_hdu_connect (hdu) < 0) {
    LOG("ERROR in dada_hdu_connect\n");
    exit(EXIT_FAILURE);
  }

  // Make data buffers readable
  if (dada_hdu_lock_read(hdu) < 0) {
    LOG("ERROR in dada_hdu_open_view\n");
    exit(EXIT_FAILURE);
  }

  // get write address
  char *header;
  uint64_t bufsz;
  header = ipcbuf_get_next_read (hdu->header_block, &bufsz);
  if (! header || ! bufsz) {
    LOG("ERROR. Get next header block error\n");
    exit(EXIT_FAILURE);
  }

  // parse header
  ascii_header_get(header, "NCHAN", "%d", &nchannels);
  ascii_header_get(header, "MIN_FREQUENCY", "%lf", &min_frequency);
  ascii_header_get(header, "CHANNEL_BANDWIDTH", "%lf", &channel_bandwidth);
  ascii_header_get(header, "BW", "%lf", &bandwidth);
  ascii_header_get(header, "TSAMP", "%lf", &tsamp);
  ascii_header_get(header, "RA", "%lf", &ra);
  ascii_header_get(header, "DEC", "%lf", &dec);
  ascii_header_get(header, "SOURCE", "%s", source_name);
  ascii_header_get(header, "AZ_START", "%lf", &az_start);
  ascii_header_get(header, "ZA_START", "%lf", &za_start);
  ascii_header_get(header, "MJD_START", "%lf", &mjd_start);
  ascii_header_get(header, "NBIT", "%d", &nbit);

  // tell the ringbuffer the header has been read
  if (ipcbuf_mark_cleared(hdu->header_block) < 0) {
    LOG("ERROR. Cannot mark the header as cleared\n");
    exit(EXIT_FAILURE);
  }

  LOG("psrdada HEADER:\n%s\n", header);

  return hdu;
}

/**
 * Print commandline options
 */
void printOptions() {
  printf("usage: dadafilterbank -c <science case> -m <science mode> -k <hexadecimal key> -l <logfile> -b <padded_size> -n <filename prefix for dumps>\n");
  printf("e.g. dadafits -k dada -l log.txt\n");
  return;
}

/**
 * Parse commandline
 */
void parseOptions(int argc, char *argv[],
    char **key, int *science_case, int *science_mode, int *padded_size, char **prefix, char **logfile) {
  int c;
  int setk=0, setb=0, setl=0, setc=0, setm=0, setn=0;
  while((c=getopt(argc,argv,"b:c:m:k:l:n:"))!=-1) {
    switch(c) {
      // -k <hexadecimal_key>
      case('k'):
        *key = strdup(optarg);
        setk=1;
        break;

      // -b padded_size (bytes)
      case('b'):
        *padded_size = atoi(optarg);
        setb=1;
        break;

      // -l log file
      case('l'):
        *logfile = strdup(optarg);
        setl=1;
        break;

      // -c <science case>
      case('c'):
        setc=1;
        *science_case = atoi(optarg);
        break;

      // -m <science mode>
      case('m'):
        setm=1;
        *science_mode = atoi(optarg);
        break;

      // -n <filename prefix>
      case('n'):
        setn=1;
        *prefix = strdup(optarg);
        break;

      // -h
      case('h'):
        printOptions();
        exit(EXIT_SUCCESS);
        break;

      default:
        fprintf(stderr, "Unknow option '%c'\n", c);
        exit(EXIT_FAILURE);
        break;
    }
  }

  // All arguments are required
  if (!setk || !setl || !setb || !setc || !setm || !setn) {
    if (!setk) fprintf(stderr, "Error: DADA key not set\n");
    if (!setl) fprintf(stderr, "Error: Log file not set\n");
    if (!setc) fprintf(stderr, "Error: Science case not set\n");
    if (!setm) fprintf(stderr, "Error: Science mode not set\n");
    if (!setn) fprintf(stderr, "Error: DADA key not set\n");
    exit(EXIT_FAILURE);
  }
}

void open_files(char *prefix, int ntabs) {
  int tab;
  for (tab=0; tab<ntabs; tab++) {
    char fname[256];
    if (ntabs == 1) {
      snprintf(fname, 256, "%s.fil", prefix);
    }
    else {
      snprintf(fname, 256, "%s_%02i.fil", prefix, tab + 1);
    }

    // open filterbank file
    output[tab] = filterbank_create(
      fname,     // filename
      10,        // int telescope_id,
      15,        // int machine_id,
      source_name, // char *source_name,
      az_start,       // double az_start,
      za_start,       // double za_start,
      ra,       // double src_raj,
      dec,       // double src_dej,
      mjd_start,       // double tstart
      tsamp,     // double tsamp,
      nbit,         // int nbits,
      min_frequency + bandwidth - .5 * channel_bandwidth,  // double fch1,
      -1 * channel_bandwidth, // double foff,
      nchannels, // int nchans,
      ntabs,     // int nbeams,
      tab + 1,   // int ibeam
      1          // int nifs
    );
  }
}

void close_files() {
  int tab;

  for (tab=0; tab<ntabs; tab++) {
    filterbank_close(output[tab]);
  }
}

/**
 * Catch SIGINT then sync and close files before exiting
 */
void sigint_handler (int sig) {
  LOG("SIGINT received, aborting\n");
  int i;
  for (i=0; i<ntabs; i++) {
    if (output[i]) {
      fsync(output[i]);
      filterbank_close(output[i]);
    }
  }
  exit(EXIT_FAILURE);
}


int main (int argc, char *argv[]) {
  char *key;
  char *logfile;
  int science_case;
  int science_mode;
  int padded_size;
  char *file_prefix;

  // parse commandline
  parseOptions(argc, argv, &key, &science_case, &science_mode, &padded_size, &file_prefix, &logfile);

  // set up logging
  if (logfile) {
    runlog = fopen(logfile, "w");
    if (! runlog) {
      LOG("ERROR opening logfile: %s\n", logfile);
      exit(EXIT_FAILURE);
    }
    LOG("Logging to logfile: %s\n", logfile);
    free (logfile);
  }

  if (science_case == 3) {
    // NTIMES (12500) per 1.024 seconds -> 0.00008192 [s]
    ntimes = 12500;
  } else if (science_case == 4) {
    // NTIMES (25000) per 1.024 seconds -> 0.00004096 [s]
    ntimes = 25000;
  } else {
    LOG("Error: Illegal science case '%i'", science_mode);
    exit(EXIT_FAILURE);
  }


  LOG("dadafilterbank version: " VERSION "\n");
  LOG("Science case = %i\n", science_case);
  LOG("Filename prefix = %s\n", file_prefix);

  if (science_mode == 0) {
    // I + TAB
    ntabs = 12;
    LOG("Science mode: I + TAB\n");
  } else if (science_mode == 2) {
    // I + IAB
    ntabs = 1;
    LOG("Science mode: I + IAB\n");
  } else if (science_mode == 1 || science_mode == 3) {
    LOG("Error: modes IQUV+TAB / IQUV+IAB not supported");
    exit(EXIT_FAILURE);
  } else {
    LOG("Error: Illegal science mode '%i'", science_mode);
    exit(EXIT_FAILURE);
  }

  // connect to ring buffer
  dada_hdu_t *ringbuffer = init_ringbuffer(key);
  ipcbuf_t *data_block = (ipcbuf_t *) ringbuffer->data_block;
  ipcio_t *ipc = ringbuffer->data_block;

  // create filterbank files, and close files on C-c
  open_files(file_prefix, ntabs);
  signal(SIGINT, sigint_handler);

  // for interaction with ringbuffer
  uint64_t bufsz = ipc->curbufsz;
  char *page = NULL;

  // for processing a page
  int tab, channel, time;
  char *buffer = malloc(ntimes * NCHANNELS * sizeof(char));

  int page_count = 0;
  int quit = 0;
  while(!quit && !ipcbuf_eod(data_block)) {

    page = ipcbuf_get_next_read(data_block, &bufsz);
    if (! page) {
      quit = 1;
    } else {
      // page [NTABS, NCHANNELS, time(padded_size)]
      // file [time, NCHANNELS]
      for (tab = 0; tab < ntabs; tab++) {
        for (channel = 0; channel < NCHANNELS; channel++) {
          for (time = 0; time < ntimes; time++) {
            // reverse freq order to comply with header
            buffer[time*NCHANNELS+NCHANNELS-channel-1] = page[(tab*NCHANNELS + channel) * padded_size + time];
          }
        }
        write(output[tab], buffer, sizeof(char) * ntimes * NCHANNELS);
      }
      ipcbuf_mark_cleared((ipcbuf_t *) ipc);
      page_count++;
    }
  }

  if (ipcbuf_eod(data_block)) {
    LOG("End of data received\n");
  }

  dada_hdu_unlock_read(ringbuffer);
  dada_hdu_disconnect(ringbuffer);
  free(buffer);
  LOG("Read %i pages\n", page_count);
}
