/**
 * program: dadasigproc
 *
 * Purpose: connect to a ring buffer and create Sigproc output per TAB on request
 * 
 *          A ringbuffer page is interpreted as an array of Stokes IQUV:
 *          TODO
 *
 *          Written for the AA-Alert project, ASTRON
 *
 * Author: Jisk Attema, Netherlands eScience Center
 * Licencse: Apache v2.0
 */

#include <string.h>
#include <stdio.h>
#include <getopt.h>

#include "dada_hdu.h"
#include "ascii_header.h"

FILE *runlog = NULL;
#define LOG(...) {fprintf(stdout, __VA_ARGS__); fprintf(runlog, __VA_ARGS__); fflush(stdout);}

#define NTABS 12
#define NCHANNELS 1536
#define NTIMES 25000

#define CACHE_SIZE 10
unsigned char *cache_pages[CACHE_SIZE];
int cache_start, cache_end;

unsigned char *get_page() {
  unsigned char *page = NULL;

  if (cache_start != cache_end) {
    page = cache_pages[cache_start++];
    if (cache_start == CACHE_SIZE) {
      cache_start = 0;
    }
  }
  return page;
}

int add_page(unsigned char *page) {
  int cache_count = cache_end - cache_start;
  if (cache_count < 0) {
    cache_count += CACHE_SIZE;
  } else if (cache_count == CACHE_SIZE) {
    LOG("Cache full");
    exit(EXIT_FAILURE);
  }

  cache_pages[cache_end++] = page;
  if (cache_end == CACHE_SIZE) {
    cache_end = 0;
  }
}

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
  unsigned int uintValue;
  float floatValue[2];
  ascii_header_get(header, "SAMPLES_PER_BATCH", "%d", &uintValue);
  ascii_header_get(header, "CHANNELS", "%d", &uintValue);
  ascii_header_get(header, "MIN_FREQUENCY", "%f", &floatValue[0]);
  ascii_header_get(header, "CHANNEL_BANDWIDTH", "%f", &floatValue[1]);

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
  printf("usage: dadasigproc -k <hexadecimal key> -l <logfile>\n");
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

  LOG("dadasigproc version: " VERSION "\n");

  dada_hdu_t *ringbuffer = init_ringbuffer(key);
  ipcbuf_t *data_block = (ipcbuf_t *) ringbuffer->data_block;
  ipcio_t *ipc = ringbuffer->data_block;

  int quit = 0;
  uint64_t bufsz = ipc->curbufsz;

  int page_count = 0;
  char *page = NULL;

  while(!quit && !ipcbuf_eod(data_block)) {
    int tab; // tight array beam

    page = ipcbuf_get_next_read(data_block, &bufsz);
    if (! page) {
      quit = 1;
    } else {
      // Wait for there to be 4 pages ahead of us
      bool wait = true;
      while (wait) {
        int written = ipcbuf_get_write_count(data_block);
        int read = ipcbuf_get_read_count(data_block);
        if (written - read < 4) {
          LOG("Waiting");
          wait = true;
          sleep(1);
        } else {
          wait = false;
        }
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
}
