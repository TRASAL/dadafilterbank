SOURCE_ROOT ?= $(HOME)
BIN_DIR ?= $(SOURCE_ROOT)/install/bin

VERSION := $(shell git rev-parse HEAD )

# http://psrdada.sourceforge.net/
PSRDADA  ?= $(SOURCE_ROOT)/src/psrdada

OPTIMIZATION := -Ofast -march=native

INCLUDES := -I"$(PSRDADA)/src/"
DADA_DEPS := $(PSRDADA)/src/dada_hdu.o $(PSRDADA)/src/ipcbuf.o $(PSRDADA)/src/ipcio.o $(PSRDADA)/src/ipcutil.o $(PSRDADA)/src/ascii_header.o $(PSRDADA)/src/multilog.o $(PSRDADA)/src/tmutil.o $(PSRDADA)/src/fileread.o $(PSRDADA)/src/filesize.o

dadafilterbank: main.c filterbank.c
	gcc -o dadafilterbank main.c filterbank.c `pkg-config --cflags --libs cfitsio` $(DADA_DEPS) -I"$(PSRDADA)/src" $(OPTIMIZATION) -DVERSION='"$(VERSION)"'
