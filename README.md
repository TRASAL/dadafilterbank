# dadafilterbank

Connect to a [PSRdada](http://psrdada.sourceforge.net/) ringbuffer and write out the data in [filterbank](http://sigproc.sourceforge.net/) format.

This program is part of the data handling pipeline for the AA-ALERT project.
See [dadatrigger](https://github.com/AA-ALERT/dadatrigger) for an introduction and dataflow schema.

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
- mode 0: Stokes I + TAB (12 tied array beams)
- mode 2: Stokes I + IAB (coherent beams, so only one tied array beam)

Not supported modes:
- mode 1: Stokes IQUV + TAB
- mode 3: Stokes IQUV + IAB


## Science cases

The data rate is set per science case.
Supported cases:
- case 3: 12500 samples per second
- case 4: 25000 samples per second


# The ringbuffer

## Header block

Metadata is read from the PSRdada header block.
Note that some of the metadata available in the header block is ignored, due to code constraints and optimizations.
For values that should be present see the table below.

|header key| description | notes | units |
|----------|-------------|-------|-------|
| MIN\_FREQUENCY | Center of lowest frequency band            |                              | |
| BW             | Total bandwidth of the observation         |                              | |
| RA             |                                            |                              | |
| DEC            |                                            |                              | |
| SOURCE         |                                            |                              | |
| AZ\_START      |                                            |                              | |
| ZA\_START      |                                            |                              | |
| MJD\_START     |                                            |                              | |
| PADDED\_SIZE   | Length of the fastest dimension of the data array |                       | |
| SCIENCE\_CASE  | Mode of operation of ARTS, determines data rate   |                       | |
| SCIENCE\_MODE  | Mode of operation of ARTS, determines data layout |                       | |

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

# Building

To connect to the PSRDada ring buffer, we depend on some object files that can be obtained when compiling PSRDada.
The location of these files is assumed to be in the **PSRDADA** directory.
Alternatively, set **SOURCE\_ROOT** such that the files are in **SOURCE\_ROOT/src/psrdada**.

Building is then done using the Makefile:
```bash
  make
```

# Contributers

Jisk Attema, Netherlands eScience Center
Leon Oostrum, UvA

# NOTES

1. maximum length of *source_name* is currently 255 characters. Longer names will result in undefined behaviour in dada functions.
2. The number of frequency channels is hardcoded to 1536, the number of bits to 8.
