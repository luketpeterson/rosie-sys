## -*- Mode: Makefile; -*-                                             
##
## Makefile for building the Rosie Pattern Language peg matcher
## (based on lpeg)
##
## © Copyright IBM Corporation 2016, 2017, 2018, 2019, 2020.
## © Copyright Jamie A. Jennings, 2018.
## LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)
## AUTHOR: Jamie A. Jennings

BUILD_ROOT=$(shell cd ../../..; pwd)
COMMONDIR = ../include

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

# How to set these generically?
ifdef FILEDEBUG
filedebug='-DFILEDEBUG=1'
endif

ifdef VMEBUG
vmdebug='-DVMDEBUG=1'
endif

ifdef BUFDEBUG
bufdebug='-DBUFDEBUG=1'
endif


COPT = -O2 $(debug_flag) $(ndebug_flag) $(debugger_flag) $(filedebug) $(vmdebug) $(bufdebug)
FILES = buf.o vm.o ktable.o capture.o file.o json.o rplx.o

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

# ASAN_OPT may be exported from the Makefile that calls this one
CFLAGS = -fvisibility=hidden $(CWARNS) $(COPT) -std=$(Cstd) -I$(COMMONDIR) -fPIC $(ASAN_OPT)

default: $(FILES)

# AR= ar rc
# RANLIB= ranlib

# rpeg.a: $(FILES)
# 	$(AR) $@ $< $(FILES)
# 	$(RANLIB) $@

.PHONY: clean
clean:
	rm -f $(FILES) rpeg.a *.o

#
# gcc -I../include -MM *.c >>Makefile
#
buf.o: buf.c ../include/buf.h
capture.o: capture.c ../include/config.h ../include/buf.h \
  ../include/ktable.h ../include/capture.h ../include/rplx.h \
  ../include/vm.h
file.o: file.c ../include/ktable.h ../include/config.h ../include/file.h \
  ../include/rplx.h
json.o: json.c ../include/config.h ../include/buf.h ../include/ktable.h \
  ../include/capture.h ../include/rplx.h ../include/vm.h \
  ../include/json.h
ktable.o: ktable.c ../include/config.h ../include/ktable.h
rplx.o: rplx.c ../include/config.h ../include/rplx.h ../include/ktable.h
stack.o: stack.c
vm.o: vm.c ../include/config.h ../include/rplx.h ../include/ktable.h \
  ../include/buf.h ../include/vm.h stack.c
