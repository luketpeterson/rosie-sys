# -*- Mode: BSDmakefile; -*-                                          
#
# Makefile for building librosie
#
# © Copyright Jamie A. Jennings 2019, 2020.
# © Copyright IBM Corporation 2016, 2017, 2018.
# LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)
# AUTHOR: Jamie A. Jennings

## -----------------------------------------------------------------------------
##
## Use "DEBUG=1" on the command line to cause librosie to log
## informational and error messages to stderr.
## 
## Use 'LUADEBUG=1' to include into the build a lua repl that can be
## accessed by passing '-D' to the rosie CLI as the first command line
## parameter.  This feature is needed for the white-box testing
## invoked by "make test".
##
## After compilation, the resulting files are placed in the "binaries"
## subdirectory.  
##
## ASAN_OPT may be set by a Makefile that calls this one.
##
## ROSIE_CLI may be set by a Makefile that calls this one.
##
## -----------------------------------------------------------------------------

HOME = $(shell cd ../..; pwd)

ifdef LUADEBUG
lua_debug="-DLUADEBUG"
ROSIE_CLI=1 ## LUADEBUG effectively requires ROSIE_CLI
endif

ifdef ROSIE_CLI
rosie_cli="-DROSIE_CLI"
lua_repl=lua_repl.o
readline_lib="-lreadline"
readline_path=$(HOME)/submodules/lua-readline/src/*.o
endif

ndebug_flag=-DNDEBUG

ifdef DEBUG
debug_flag=-DDEBUG
ndebug_flag=
endif

ifdef LOGGING
logging_flag=-DLOGGING
endif

ifdef LLDB
debugger_flag=-O0 -g
ndebug_flag=
endif

REPORTED_PLATFORM=$(shell (uname -o || uname -s) 2> /dev/null)
ifeq ($(REPORTED_PLATFORM), Darwin)
  PLATFORM=macosx
else ifeq ($(REPORTED_PLATFORM), GNU/Linux)
  PLATFORM=linux
else
  PLATFORM=none
endif

ifndef LUA_A
LUA_A=$(HOME)/submodules/lua/src/liblua.a
endif ## LUA_A

ROSIE_DYLIB_NAME=rosie
ROSIE_A=lib$(ROSIE_DYLIB_NAME).a

ifeq ($(PLATFORM),macosx)
  CC=cc
  SYSCFLAGS=-DLUA_USE_MACOSX -fPIC
  SYSLIBS=
  SYSLDFLAGS=-dynamiclib
  CFLAGS += -std=c99
  ROSIE_DYLIB=lib$(ROSIE_DYLIB_NAME).dylib
else 
  CC=gcc
  SYSCFLAGS=-DLUA_USE_LINUX -std=c99 -D_GNU_SOURCE=1 -fPIC
  SYSLDFLAGS=-shared
  SYSLIBS=-lpthread -ldl -lm
  ROSIE_DYLIB=lib$(ROSIE_DYLIB_NAME).so
endif

ifndef LUA_INCLUDE_DIR
LUA_INCLUDE_DIR=$(HOME)/submodules/lua/include
endif ## LUA_INCLUDE_DIR

RPEG_INCLUDE_DIR=$(HOME)/src/rpeg/include

MYCFLAGS=-I$(LUA_INCLUDE_DIR) -I$(RPEG_INCLUDE_DIR) \
	$(debug_flag) $(ndebug_flag) $(lua_debug) $(rosie_cli) $(debugger_flag) $(logging_flag)
MYLDFLAGS=
MYLIBS=
MYOBJS=

ifndef ROSIEBUILDLIB
$(error "Makefile variable ROSIEBUILDLIB not set")
endif

RPATH_LDFLAGS="-Wl,-rpath,$(ROSIEBUILDLIB)"

CFLAGS= -O2 -Wall -Wextra -pthread -DMULTIPLE_THREADS -DLUA_COMPAT_5_2 $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= $(SYSLIBS) $(MYLIBS)

AR= ar rc
RANLIB= ranlib
RM= rm -f

BINDIR= binaries

COMPILER_DIR = $(HOME)/src/rpeg/compiler
RUNTIME_DIR = $(HOME)/src/rpeg/runtime
CJSON = $(HOME)/submodules/lua-cjson/*.o
dependent_objs = liblua/*.o \
		$(lua_repl) \
		$(RUNTIME_DIR)/*.o \
		$(COMPILER_DIR)/*.o \
		$(CJSON)

.PHONY:
all: $(BINDIR)/$(ROSIE_DYLIB) $(BINDIR)/librosie.a $(BINDIR)/rosie $(BINDIR)/rosie_with_rpath

.PHONY:
RPEG:
	@echo "Building rpeg..."
	@$(MAKE) -C $(HOME)/src/rpeg

$(HOME)/submodules/lua-cjson/*.o:
	@echo "Building cjson..."
	@$(MAKE) -C $(HOME)/submodules/lua-cjson

$(LUA_A):
	@echo "Missing Rosie object files.  Need to build the other rosie components first."
	@false

liblua/*.o liblua:
	@mkdir -p liblua; \
	cd liblua && ar x $(LUA_A)

lua_repl.o: lua_repl.c lua_repl.h
	$(CC) -o $@ -c lua_repl.c $(CFLAGS) -I$(LUA_INCLUDE_DIR) -fvisibility=hidden

$(BINDIR):
	@mkdir -p $(BINDIR)

$(BINDIR)/librosie.o: librosie.c librosie.h logging.c registry.c rosiestring.c | $(BINDIR) RPEG $(CJSON)
	$(CC) $(ASAN_OPT) -fvisibility=hidden -o $@ -c librosie.c $(CFLAGS) -I$(RPEG_INCLUDE_DIR) 

$(BINDIR)/librosie.so: $(BINDIR)/librosie.o $(dependent_objs) | $(BINDIR) liblua
	$(CC) -o $@ $< $(dependent_objs) $(LIBS) $(LDFLAGS) $(ASAN_OPT) $(readline_lib) $(readline_path)

$(BINDIR)/librosie.dylib: $(BINDIR)/librosie.o $(dependent_objs) | $(BINDIR) liblua
	$(CC) -o $@ $< $(dependent_objs) $(LIBS) $(LDFLAGS) $(ASAN_OPT) $(readline_lib) $(readline_path)

$(BINDIR)/librosie.a: $(BINDIR)/librosie.o $(dependent_objs) | liblua
	$(AR) $@ $< $(dependent_objs)
	$(RANLIB) $@

$(BINDIR)/rosie.o: rosie.c librosie.c librosie.h lua_repl.h $(BINDIR)/$(ROSIE_DYLIB) | $(BINDIR) RPEG $(CJSON)
	$(CC) -o $@ -c rosie.c $(ASAN_OPT) $(CFLAGS) -I$(RPEG_INCLUDE_DIR)

$(BINDIR)/rosie_with_rpath: $(BINDIR)/rosie.o $(dependent_objs) liblua  | $(BINDIR)
	$(CC) -o $@ $< $(ASAN_OPT) $(RPATH_LDFLAGS) $(dependent_objs) $(LIBS) $(readline_lib) 

$(BINDIR)/rosie: $(BINDIR)/rosie.o $(dependent_objs) liblua  | $(BINDIR)
	$(CC) -o $@ $< $(ASAN_OPT) $(dependent_objs) $(LIBS) $(readline_lib)

$(RUNTIME_DIR)/*.o:
	$(MAKE) -C $(RUNTIME_DIR)

$(COMPILER_DIR)/*.o:
	$(MAKE) -C $(COMPILER_DIR)

.PHONY:
clean:
	-$(RM) -f *.o *.dylib *.so *.a
	-$(RM) -rf liblua binaries
	-$(MAKE) -C $(HOME)/src/rpeg clean

.PHONY:
test: $(BINDIR)/$(ROSIE_DYLIB) $(BINDIR)/$(ROSIE_A)
	here="$(shell pwd)"; \
	@echo "No librosie tests configured"

.PHONY:
echo:
	@echo "HOME= $(HOME)"
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "RM= $(RM)"
	@echo "LUADEBUG= $(LUADEBUG)"
	@echo "DEBUG= $(DEBUG)"



