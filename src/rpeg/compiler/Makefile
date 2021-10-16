## -*- Mode: Makefile; -*-                                             
##
## Makefile for building the rosie custom version of lpeg
##
## © Copyright Jamie A. Jennings 2018, 2019, 2020.
## © Copyright IBM Corporation 2016, 2017, 2018.
## LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)
## AUTHOR: Jamie A. Jennings

BUILD_ROOT=$(shell cd ../../..; pwd)
COMMONDIR = ../include

ifndef LUA_INCLUDE_DIR
LUA_INCLUDE_DIR=$(BUILD_ROOT)/submodules/lua/include
endif ## LUA_INCLUDE_DIR

REPORTED_PLATFORM=$(shell (uname -o || uname -s) 2> /dev/null)
ifeq ($(REPORTED_PLATFORM), Darwin)
PLATFORM=macosx
else ifeq ($(REPORTED_PLATFORM), GNU/Linux)
PLATFORM=linux
else
PLATFORM=none
endif

ndebug_flag=-DNDEBUG

ifdef DEBUG
debug_flag=-DDEBUG
ndebug_flag=
endif

ifdef LLDB
debugger_flag=-O0 -g
ndebug_flag=
endif

COPT = -DLPEG_DEBUG -O2 $(debug_flag) $(ndebug_flag) $(debugger_flag)
FILES = rbuf.o lpcap.o lptree.o lpcode.o lpprint.o

ifeq ($(PLATFORM), macosx)
CC= cc
Cstd= c99
DLLFLAGS = -bundle -undefined dynamic_lookup
else
CC= gcc
Cstd= c99
DLLFLAGS = -shared -fPIC
endif

ifdef ROSIE_DEBUG
COPT += -DROSIE_DEBUG
endif

CWARNS = -Wall -Wextra -pedantic \
	-Waggregate-return \
	-Wcast-align \
	-Wcast-qual \
	-Wdisabled-optimization \
	-Wpointer-arith \
	-Wshadow \
	-Wsign-compare \
	-Wundef \
	-Wwrite-strings \
	-Wbad-function-cast \
	-Wmissing-prototypes \
	-Wnested-externs \
	-Wstrict-prototypes \
        -Wunreachable-code \
        -Wno-missing-declarations \


# ASAN_OPT may be exported from the Makefile that calls this one
CFLAGS = -fvisibility=hidden $(CWARNS) $(COPT) -std=$(Cstd) -I$(LUA_INCLUDE_DIR) -I$(COMMONDIR) -fPIC $(ASAN_OPT)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)


default: $(FILES)

# lpeg.so: $(FILES)
# 	env $(CC) $(DLLFLAGS) $(FILES) -o lpeg.so

.PHONY: clean
clean:
	rm -f $(FILES)


lpcap.o: lpcap.c lpcap.h rbuf.c $(COMMONDIR)/rbuf.h lptypes.h $(COMMONDIR)/rpeg.h ktable-macros.h
lpcode.o: lpcode.c lptypes.h lpcode.h lptree.h lpcap.h 
lpprint.o: lpprint.c lptypes.h lpprint.h lptree.h lpcap.h
lptree.o: lptree.c lptypes.h lpcap.h lpcode.h lptree.h lpprint.h $(COMMONDIR)/str.h $(COMMONDIR)/rpeg.h ktable-macros.h
rbuf.o: rbuf.c $(COMMONDIR)/rbuf.h 
rcap.o: rcap.c $(COMMONDIR)/rcap.h $(COMMONDIR)/rbuf.h lpcap.h $(COMMONDIR)/ktable-macros.h
lpsave.o: lpsave.c lptypes.h lpsave.h lptree.h lpcap.h $(COMMONDIR)/ktable-macros.h


