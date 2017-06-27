/**
 * program: dadafilterbank
 *
 * Purpose: connect to a ring buffer and create Sigproc output per TAB on request
 * 
 * Depending on science case and mode, a ringbuffer page is interpreted as an array of Stokes IQUV:
 *   [tab=NTABS (12)][time=(25000 or 12500)][the 4 components IQUV][1536 channels]
 *
 * Written for the AA-Alert project, ASTRON
 *
 * Author: Jisk Attema, Netherlands eScience Center
 * Licencse: Apache v2.0
 *
 * Inspired by the dada_dbevent.c code from PSRDADA: http://psrdada.sourceforge.net/index.shtml
 * I decided to do this one because:
 *  * the large amount of data flowing around. 
 *    Using the original code would require an extra ringbuffer instance to hold the data, and another memory copy.
 *  * now using a simple unix socket instead of a TCP/IP socket. (so no interaction with the network stack)
 *  * much of the code was already written anyways
 *
 * communicate with the unix datagram socket via fi. netcat:
 * echo 'dump' | nc -uU <socket>
 *
 * TODO: smarter dumping. Now, whenever a signal is received, we dump 1 page and overwrite existing data
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <poll.h>
#include <sys/un.h>
#include <netdb.h>

#include "dada_hdu.h"
#include "ascii_header.h"
#include "filterbank.h"

#define SOCKBUFSIZE 16384 // Buffer size of socket 16kB

FILE *runlog = NULL;
#define LOG(...) {fprintf(stdout, __VA_ARGS__); fprintf(runlog, __VA_ARGS__); fflush(stdout);}

#define NTABS 12
int output[NTABS];

#define CACHE_SIZE 10
unsigned char *cache_pages[CACHE_SIZE];
int cache_start, cache_end;

unsigned int nchannels;
float min_frequency;
float channel_bandwidth;
float tsamp;
int ntimes;

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
  printf("usage: dadafilterbank -c <science case> -k <hexadecimal key> -l <logfile> -s <start packet number> -t <trigger socket> -n <filename prefix for dumps>\n");
  printf("e.g. dadafits -k dada -l log.txt\n");
  return;
}

/**
 * Parse commandline
 */
void parseOptions(int argc, char *argv[],
    char **key, int *science_case, unsigned long *startpacket, char **trigger_socket, char **prefix, char **logfile) {
  int c;
  int setk=0, setl=0, setc=0, sets=0, sett=0, setn=0;
  while((c=getopt(argc,argv,"c:k:l:s:t:n:"))!=-1) {
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

      // -c <science case>
      case('c'):
        setc=1;
        *science_case = atoi(optarg);
        if (*science_case < 3 || *science_case > 4) {
          printOptions();
          exit(0);
        }
        break;

      // -t <trigger socket>
      case('t'):
        sett=1;
        *trigger_socket = strdup(optarg);
        break;

      // -s start packet number
      case('s'):
        *startpacket = atol(optarg);
        sets=1;
        break;

      // -n <trigger socket>
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
  if (!setk || !setl || !setc || !sets || !sett || !setn) {
    printOptions();
    exit(EXIT_FAILURE);
  }
}

void open_files(char *prefix, int page_count) {
  int tab; // tight array beam

  // 1 page => 1024 microseconds
  // startpacket is in units of 1.28 us since UNIX epoch
  //
  for (tab=0; tab<NTABS; tab++) {
    char fname[256];
    snprintf(fname, 256, "%s_%06i_%02i.fil", prefix, page_count, tab + 1);

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
      NTABS,     // int nbeams,
      tab + 1,   // int ibeam, TODO: start at 1?
      4          // int nifs
    );

    // Allocate for 8 pages IQUV for 1 TAB
    // https://blog.plenz.com/2014-04/so-you-want-to-write-to-a-file-real-fast.html
    if(fallocate(output[tab], 0, 0, 1228800391) == -1 && errno == EOPNOTSUPP) {
      ftruncate(output[tab], 1228800391);
    }
  }
}

void close_files() {
  int tab; // tight array beam

  for (tab=0; tab<NTABS; tab++) {
    filterbank_close(output[tab]);
  }
}

int unix_domain_socket (char *socket_path) {
  struct sockaddr_un addr;
  char buf[100];
  int fd,cl,rc;

  if ( (fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
    perror("socket error");
    exit(-1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
  unlink(socket_path);

  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("bind error");
    exit(-1);
  }

  return fd;
}

int main (int argc, char *argv[]) {
  char *key;
  char *logfile;
  int science_case;
  unsigned long startpacket;
  char *trigger_socket;
  char *dump_prefix;

  // parse commandline
  parseOptions(argc, argv, &key, &science_case, &startpacket, &trigger_socket, &dump_prefix, &logfile);

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
  LOG("Trigger socket = %s\n", trigger_socket);
  LOG("Dump prefix = %s\n", dump_prefix);

  // connect to ring buffer
  dada_hdu_t *ringbuffer = init_ringbuffer(key);
  ipcbuf_t *data_block = (ipcbuf_t *) ringbuffer->data_block;
  ipcio_t *ipc = ringbuffer->data_block;

  // open trigger socket
  int trigger_fd = unix_domain_socket(trigger_socket);
  if (trigger_fd < 0) { 
    LOG("ERROR opening trigger socket, error code %i\n", trigger_fd);
    exit(EXIT_FAILURE);
  }

  // for interaction with ringbuffer
  uint64_t bufsz = ipc->curbufsz;
  uint64_t full_bufs = 0;
  char *page = NULL;
  int page_count = 0;

  // for interaction with the trigger socket
  char message[256];
  message[255] = '\0';
  struct pollfd p;
  p.fd = trigger_fd;
  p.events = POLLIN;
  p.revents = 0;

  // to deal with triggers
  int tab; // tight array beam
  int to_write = 0; // pages left to write
  int read_count;

  // get the first page
  page = ipcbuf_get_next_read(data_block, &bufsz);
  if (! page) {
    LOG("ERROR ipcbuf_get_next_read failed");
    exit(EXIT_FAILURE);
  }

  while(1) {
    p.revents = 0;
    int rc = poll(&p, 1, 50); // wait 50 miliseconds

    if (rc < 0) {
      // problem with poll call
      LOG("ERROR poll failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    // else if (rc == 0) {
    //   // select timed out
    // }
    else if (rc > 0) {
      // we received something
      read(trigger_fd, message, 254);
      LOG("Received trigger: '%s'\n", message);
      if (to_write == 0) {
        // we were not writing, open files, and prepare to write 8 samples to disk
        to_write = 8;
        open_files(dump_prefix, page_count);
      } else if (to_write > 0) {
        // we were writing to disk, but another trigger happened
        // reset the write count to 8 again
        to_write = 0;
      }
    }

    full_bufs = ipcbuf_get_nfull(data_block);
    if (full_bufs >= 4) {
      // The delay has reached sufficient length,
      // process the next page to prevent the ringbuffer from overflowing

      // Check if this page needs to be saved
      if (to_write > 0) {
        // write this page to disk, and decrease write counter
        to_write--;

        for (tab=0; tab<NTABS; tab++) {
          // SAMPLE * IF * NCHAN
          write(output[tab], &page[tab*ntimes*4*1536], sizeof(char) * 1 * ntimes * 4 * 1536);
          // see here: http://lkml.iu.edu/hypermail/linux/kernel/1005.2/01845.html
        }

        if (to_write == 0) {
          // done writing
          close_files();
        }
      }

      // proceed to next page
      ipcbuf_mark_cleared((ipcbuf_t *) ipc);
      page = ipcbuf_get_next_read(data_block, &bufsz);
      if (! page) {
        LOG("ERROR ipcbuf_get_next_read failed");
        exit(EXIT_FAILURE);
      }
      page_count++;
    }
    // TODO: process remaining pages after a 'quit' has been triggered
  }

  dada_hdu_unlock_read(ringbuffer);
  dada_hdu_disconnect(ringbuffer);
  LOG("Read %i pages\n", page_count);
}
