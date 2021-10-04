## -*- Mode: Makefile; -*-                                             
##
## Makefile for building the Rosie Pattern Language peg matcher
## (based on lpeg)
##
## © Copyright IBM Corporation 2016, 2017, 2018.
## © Copyright Jamie A. Jennings, 2018.
## LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)
## AUTHOR: Jamie A. Jennings

BUILD_ROOT=$(shell cd ..; pwd)
COMMONDIR=$(BUILD_ROOT)/include

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

COPT = -O2 $(debug_flag) $(ndebug_flag) $(debugger_flag)

ifeq ($(PLATFORM), macosx)
CC= cc
Cstd= c99
else
CC= gcc
Cstd= c99
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

CFLAGS = -fvisibility=hidden $(CWARNS) $(COPT) -std=$(Cstd) -I$(COMMONDIR) -fPIC

match: match.o
	$(CC) -o $@ $< -I../include ../runtime/*.o

.PHONY: clean
clean:
	rm match
	rm -f *.o

#
# gcc -MM *.c -I../include >>Makefile
#
