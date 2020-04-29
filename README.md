# dadafilterbank

Connect to a [PSRdada](http://psrdada.sourceforge.net/) ringbuffer and write out the data
in [filterbank](http://sigproc.sourceforge.net/) format.

This program is part of the data handling pipeline for the AA-ALERT project.
See [dadatrigger](https://github.com/AA-ALERT/dadatrigger) for an introduction and dataflow schema.

# Installation

Requirements:
 * Cmake
 * Psrdada

Note that psrdada could add an additional dependency on CUDA.
 
 Instructions:
 
```
$ mkdir build && cd build
$ cmake .. -DCMAKE_BUILD_TYPE=release
$ make
$ make install

```

# Usage

```bash
 $ dadafilterbank -k <hexadecimal key> -l <logfile> -n <filename prefix for dumps>
```

Command line arguments:
 * *-k* Set the (hexadecimal) key to connect to the ringbuffer.
 * *-l* Absolute path to a logfile (to be overwritten)
 * *-n* Prefix for the fitlerbank output files

# Modes of operation

## Science modes

The program implements different modes:
- mode 0: Stokes I + TAB (multiple beams)
- mode 2: Stokes I + IAB (coherent beams, so only one tied array beam)

Not supported modes:
- mode 1: Stokes IQUV + TAB
- mode 3: Stokes IQUV + IAB


## Science cases

The data rate is set per science case.
Supported cases:
- case 3: 12500 samples per second, 9 beams.
- case 4: 25000 samples per second, 12 beams.


# The ringbuffer

## Header block

Metadata is read from the PSRdada header block.
Note that some of the metadata available in the header block is ignored, due to code constraints and optimizations.
For values that should be present see the table below.

|header key      | type   | units            | description                                       | notes |
|----------------|--------|------------------|---------------------------------------------------|-------|
| MIN\_FREQUENCY | double | MHz              | Center of lowest frequency band                   |       |
| BW             | double | MHz              | Total bandwidth of the observation                |       |
| RA             | double | hhmmss.s         | Right ascension                                   |       |
| DEC            | double | ddmmss.s         | Declination                                       |       |
| SOURCE         | string | text             | Source name                                       |       |
| AZ\_START      | double | degrees          | Azimuth angle of telescope                        |       |
| ZA\_START      | double | degrees          | Zenith angle of telescope                         |       |
| MJD\_START     | double | days since epoch | Modified Julian Date                              |       |
| PADDED\_SIZE   | int    | bytes            | Length of the fastest dimension of the data array |       |
| SCIENCE\_CASE  | int    | 1                | Mode of operation of ARTS, determines data rate   |       |
| SCIENCE\_MODE  | int    | 1                | Mode of operation of ARTS, determines data layout |       |


## Data block

A ringbuffer page is interpreted as an array of Stokes I: [NTABS, NCHANNELS, padded\_size]
Array padding along the fastest dimension is implemented to facilitate memory copies.

# Filterbank output files

Tied array beams are written to separate files, one per observation.
Note that these files can become very big.

Filterbank file names are derived from the file name prefix (*-n* option).
- For science mode 0, *.fil* is appended, resulting in *prefix.fil*
- For science mode 2, both tied array beam number and *.fil* is appended, resulting in *prefix_NN.fil*.

To prevent issues with relative paths etc., please use fully resolved absolute paths (starting with a '/').

# Performance

Altough the program is relatively simple, the large arrays can cause performance issues wrt. caching.
The matrix transpose and inversion of the channel dimension takes longer than realtime using a naive implementation on the ARTS cluster.

In the *tune* subdirectory there are several implementations trying out different loop order and various levels of loop unrolling.
It also adds openMP, with the number of threads specified in the Makefile.
As a final step, you should pin the executable to a specific core using taskset.

To try them run:
```bash
  cd tune
  make all
  make time
```

For science 4 on the ARTS cluster, the *loopct_r6* implementation was fastest (using 2 to 4 threads); this is current implementation.

# Contributers

Jisk Attema, Netherlands eScience Center
Leon Oostrum, UvA

# NOTES

1. maximum length of *source_name* is currently 255 characters. Longer names will result in undefined behaviour in dada functions.
2. The number of frequency channels is hardcoded to 1536, the number of bits to 8.
