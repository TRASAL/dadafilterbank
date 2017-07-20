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
#define LOG(...) {fprintf(stdout, __VA_ARGS__); fprintf(runlog, __VA_ARGS__); fflush(stdout);}

#define NCHANNELS 1536
#define MAXTABS 12
int output[MAXTABS];

unsigned int nchannels;
float min_frequency;
float channel_bandwidth;
float tsamp;
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
  LOG("dadafits SHMKEY: %s\n", key);

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
  ascii_header_get(header, "CHANNELS", "%d", &nchannels);
  ascii_header_get(header, "MIN_FREQUENCY", "%f", &min_frequency);
  ascii_header_get(header, "CHANNEL_BANDWIDTH", "%f", &channel_bandwidth);

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
  printf("usage: dadafilterbank -c <science case> -m <science mode> -k <hexadecimal key> -l <logfile> -s <start packet number> -b <padded_size> -n <filename prefix for dumps>\n");
  printf("e.g. dadafits -k dada -l log.txt\n");
  return;
}

/**
 * Parse commandline
 */
void parseOptions(int argc, char *argv[],
    char **key, int *science_case, int *science_mode, unsigned long *startpacket, int *padded_size, char **prefix, char **logfile) {
  int c;
  int setk=0, setb=0, setl=0, setc=0, setm=0, sets=0, setn=0;
  while((c=getopt(argc,argv,"b:c:m:k:l:s:n:"))!=-1) {
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
        if (*science_case < 3 || *science_case > 4) {
          printOptions();
          exit(0);
        }
        break;

      // -m <science mode>
      case('m'):
        setm=1;
        *science_mode = atoi(optarg);
        if (*science_mode != 0 && *science_mode != 1) {
          printOptions();
          exit(0);
        }
        break;

      // -s start packet number
      case('s'):
        *startpacket = atol(optarg);
        sets=1;
        break;

      // -n <filename prefix>
      case('n'):
        setn=1;
        *prefix = strdup(optarg);
        break;

      default:
        printOptions();
        exit(0);
    }
  }

  // All arguments are required
  if (!setk || !setl || !setb || !setc || !setm || !sets || !setn) {
    printOptions();
    exit(EXIT_FAILURE);
  }
}

void open_files(char *prefix) {
  int tab; // tight array beam

  // 1 page => 1024 microseconds
  // startpacket is in units of 1.28 us since UNIX epoch
  //
  for (tab=0; tab<ntabs; tab++) {
    char fname[256];
    snprintf(fname, 256, "%s_%02i.fil", prefix, tab + 1);

    // open filterbank file
    output[tab] = filterbank_create(
      fname,     // filename
      10,        // int telescope_id,
      15,        // int machine_id,
      "TODO",    // char *source_name,
      0.0,       // double az_start,
      1.0,       // double za_start,
      2.0,       // double src_raj,
      3.0,       // double src_dej,
      0.0,       // double tstart, TODO
      tsamp,     // double tsamp,
      8,         // int nbits,
      min_frequency,          // double fch1,
      -1 * channel_bandwidth, // double foff,
      nchannels, // int nchans,
      ntabs,     // int nbeams,
      tab + 1,   // int ibeam, TODO: start at 1?
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
  unsigned long startpacket;
  char *file_prefix;

  // parse commandline
  parseOptions(argc, argv, &key, &science_case, &science_mode, &startpacket, &padded_size, &file_prefix, &logfile);

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
    tsamp = 0.00008192;
    ntimes = 12500;
  } else if (science_case == 4) {
    // NTIMES (25000) per 1.024 seconds -> 0.00004096 [s]
    tsamp = 0.00004096;
    ntimes = 25000;
  }

  LOG("dadafilterbank version: " VERSION "\n");
  LOG("Science case = %i\n", science_case);
  LOG("Sampling time [s] = %f\n", tsamp);
  LOG("Filename prefix = %s\n", file_prefix);

  if (science_mode == 0) {
    // IAB
    ntabs = 1;
    LOG("Science mode: IAB\n");
  } else if (science_mode == 1) {
    // TAB
    ntabs = 12;
    LOG("Science mode: TAB\n");
  }


  // connect to ring buffer
  dada_hdu_t *ringbuffer = init_ringbuffer(key);
  ipcbuf_t *data_block = (ipcbuf_t *) ringbuffer->data_block;
  ipcio_t *ipc = ringbuffer->data_block;

  // create filterbank files, and close files on C-c
  open_files(file_prefix);
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
            buffer[time*NCHANNELS+channel] = page[(tab*NCHANNELS + channel) * padded_size + time];
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
