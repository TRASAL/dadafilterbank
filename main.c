/**
 * program: dadafilterbank
 *
 * Purpose: connect to a ring buffer and create Sigproc output per TAB on request
 * 
 *          A ringbuffer page is interpreted as an array of Stokes IQUV:
 *          [tab=NTABS (12)][time=NTIMES (25000)][the 4 components IQUV][1536 channels]
 *
 *          SC3: NTIMES (12500) per 1.024 seconds -> TSAMP 0.00008192
 *          SC4: NTIMES (25000) per 1.024 seconds -> TSAMP 0.00004096
 *          TODO: science case selection, now defaults to 4
 *
 *          Written for the AA-Alert project, ASTRON
 *
 * Author: Jisk Attema, Netherlands eScience Center
 * Licencse: Apache v2.0
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

#include "dada_hdu.h"
#include "ascii_header.h"
#include "filterbank.h"

FILE *runlog = NULL;
#define LOG(...) {fprintf(stdout, __VA_ARGS__); fprintf(runlog, __VA_ARGS__); fflush(stdout);}

#define NTABS 12
#define NTIMES 25000
#define TSAMP 0.00004096

#define CACHE_SIZE 10
unsigned char *cache_pages[CACHE_SIZE];
int cache_start, cache_end;


unsigned int nchannels;
float min_frequency;
float channel_bandwidth;

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
  printf("usage: dadafilterbank -k <hexadecimal key> -l <logfile>\n");
  printf("e.g. dadafits -k dada -l log.txt\n");
  return;
}

/**
 * Parse commandline
 */
void parseOptions(int argc, char *argv[], char **key, char **logfile) {
  int c;

  int setk=0, setl=0;
  while((c=getopt(argc,argv,"k:l:"))!=-1) {
    switch(c) {
      // -k <hexadecimal_key>
      case('k'):
        *key = strdup(optarg);
        setk=1;
        break;

      // -l log file
      case('l'):
        *logfile = strdup(optarg);
        setl=1;
        break;

      default:
        printOptions();
        exit(0);
    }
  }

  // All arguments are required
  if (!setk || !setl) {
    printOptions();
    exit(EXIT_FAILURE);
  }
}


int main (int argc, char *argv[]) {
  char *key;
  char *logfile;
  int tab; // tight array beam

  // parse commandline
  parseOptions(argc, argv, &key, &logfile);

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

  LOG("dadafilterbank version: " VERSION "\n");

  // connect to ring buffer
  dada_hdu_t *ringbuffer = init_ringbuffer(key);
  ipcbuf_t *data_block = (ipcbuf_t *) ringbuffer->data_block;
  ipcio_t *ipc = ringbuffer->data_block;

  // open filterbank file
  FILE *output[NTABS];

  for (tab=0; tab<NTABS; tab++) {
    char fname[256];
    snprintf(fname, 256, "mydata_%02i.fil", tab);

    output[tab] = filterbank_create(
      fname,     // filename
      1,         // int telescope_id,
      1,         // int machine_id,
      "testing", // char *source_name,
      0.0,       // double az_start,
      1.0,       // double za_start,
      2.0,       // double src_raj,
      3.0,       // double src_dej,
      0.0,       // double tstart,
      TSAMP,     // double tsamp,
      8,         // int nbits,
      min_frequency,       // double fch1,
      channel_bandwidth,   // double foff,
      nchannels, // int nchans,
      NTABS,     // int nbeams,
      tab + 1,   // int ibeam, TODO: start at 1?
      1          // int nifs
    );
  }

  int quit = 0;
  uint64_t bufsz = ipc->curbufsz;
  uint64_t full_bufs = 0;

  int page_count = 0;
  char *page = NULL;

  while(!quit && !ipcbuf_eod(data_block)) {
    page = ipcbuf_get_next_read(data_block, &bufsz);
    if (! page) {
      quit = 1;
    } else {
      // Wait for there to be 4 pages ahead of us
      int wait = 1;
      while (wait) {
        // TODO: should we add expclit reader id from data_block->iread
        full_bufs = ipcbuf_get_nfull(data_block);
        if (full_bufs < 4) {
          wait = 1;
          usleep(10000); // 10 miliseconds
        } else {
          wait = 0;
        }
      }
      // page is [tab=NTABS][time=NTIMES][the 4 components IQUV][1536 channels]
      // filterbank format is [time, polarization, frequency]
      for (tab=0; tab<NTABS; tab++) {
        fwrite(&page[tab*NTIMES*4*1536], sizeof(char), NTIMES * 1 * 1536, output[tab]); // SAMPLE * IF * NCHAN
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
  LOG("Read %i pages\n", page_count);

  for (tab=0; tab<NTABS; tab++) {
    filterbank_close(output[tab]);
  }
}
