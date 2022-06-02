/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  rosie.c                                                                  */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2019, 2020.                                */
/*  © Copyright IBM 2018.                                                    */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "librosie.h"

/* --- */

#define LUA_VERSION_MAJOR	"5"
#define LUA_VERSION_MINOR	"3"
#define LUA_VERSION_NUM		503
#define LUA_VERSION_RELEASE	"2"

#define LUA_VERSION	"Lua " LUA_VERSION_MAJOR "." LUA_VERSION_MINOR
#define LUA_RELEASE	LUA_VERSION "." LUA_VERSION_RELEASE
#define LUA_COPYRIGHT	LUA_RELEASE "  Copyright (C) 1994-2015 Lua.org, PUC-Rio"
#define LUA_AUTHORS	"R. Ierusalimschy, L. H. de Figueiredo, W. Celes"

/* --- */

static const char *progname = "rosie"; /* default */

#define BIND(name, fromlib) do {			\
  char *BINDerrorstring;				\
  (name) = dlsym(fromlib, #name);			\
  if ((BINDerrorstring = dlerror()) != NULL)  {		\
    fprintf(stderr, "%s: incompatible librosie version\n", progname);		\
    fprintf(stderr, "  use script ./rosie from rosie directory to run the local build\n");		\
    fprintf(stderr, "  or set LD_LIBRARY_PATH to build/lib\n");		\
    fprintf(stderr, "  details of error are: %s\n", BINDerrorstring); \
    exit(-3);						\
  }							\
  } while (0)

#ifdef __APPLE__
#define LIBROSIE "librosie.dylib"
#else
#define LIBROSIE "librosie.so" 
#endif

int main (int argc, char **argv) {
  str messages;
  int invoke_repl = 0;
  void *handle;
  char *err;
  Engine *(*rosie_new)(str *messages);
  void (*rosie_finalize)(Engine *e);
  int (*rosie_exec_cli)(Engine *e, int argc, char **argv, char **err);

  if (argv[0] && argv[0][0]) progname = argv[0];

  handle = dlopen(LIBROSIE, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    exit(-2);
  }
  dlerror();    /* Clear any previous error */
  
  BIND(rosie_new, handle);
  BIND(rosie_finalize, handle);
  BIND(rosie_exec_cli, handle);

  Engine *e = rosie_new(&messages);
  if (!e) {
    fprintf(stderr, "%s: %.*s\n", progname, messages.len, messages.ptr);
    exit(-1);
  }

  if ((argc > 0) && argv[1] && !strncmp(argv[1], "-D", 3)) {
    invoke_repl = 1;
    for (int i = 1; i < argc-1; i++) argv[i] = argv[i+1];
    argv[argc-1] = (char *)'\0';
    argc = argc - 1;
  }

  int status = rosie_exec_cli(e, argc, argv, &err);

  if (invoke_repl) {
#ifdef LUADEBUG
    int (*rosie_exec_lua_repl)(Engine *e, int argc, char **argv);
    BIND(rosie_exec_lua_repl, handle);
    printf("Entering %s\n", LUA_COPYRIGHT);
    rosie_exec_lua_repl(e, argc, argv);
#else
    fprintf(stderr, "%s: no lua debug support available\n", progname);
#endif
  }

  /* 
     Status values 0, 1 are used by the boolean output encoder.
     Status of 0 is also the unix "all is ok" value.
     Negative values indicate that some error occurred.
  */
  
  if (status >= 0) goto ok;

  fprintf(stderr, "%s: error %d: ", progname, status);
  switch (status) {
  case ERR_OUT_OF_MEMORY:
    fprintf(stderr, "out of memory\n");
    break;
  case ERR_SYSCALL_FAILED:
    fprintf(stderr, "syscall failed\n");
    break;
  case ERR_LUA_CLI_LOAD_FAILED:
    fprintf(stderr, "CLI failed to load (installation is incomplete?)\n");
    break;
  case ERR_LUA_CLI_EXEC_FAILED:
    fprintf(stderr, "an unknown error occurred (this is a bug), additional info: %s\n",
	    err ? err : "none");
  }
  
 ok:
  rosie_finalize(e);
  return status;
}

