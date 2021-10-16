/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  librosie.h                                                               */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018, 2019, 2021.                          */
/*  © Copyright IBM Corporation 2016, 2017.                                  */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#ifndef LIBROSIE_H
#define LIBROSIE_H

#include <stdint.h>		/* uint32_t, uint8_t */

#define INITIAL_RPLX_SLOTS 32
#define MIN_ALLOC_LIMIT_MB 8192	/* KB */

#define TRUE 1
#define FALSE 0

#define MAX_ENCODER_NAME_LENGTH 64 /* arbitrary limit to avoid runaway strcmp */

#define SUCCESS 0
#define ERR_OUT_OF_MEMORY -2
#define ERR_SYSCALL_FAILED -3
#define ERR_ENGINE_CALL_FAILED -4

#include <pthread.h>		/* FUTURE: Make this conditional, so
				   that users who do not need
				   threads? */

typedef struct rosie_engine {
     void *L;
     pthread_mutex_t lock;
} Engine;

// -----------------------------------------------------------------------------

/*
   IMPORTANT: 

   byte_ptr, rosie_string/str, and rosie_matchresult/match must be
   kept in sync with str.h.  We do not include str.h here because we
   want librosie.h to be a single self-contained header file that can
   be copied to (e.g.) /usr/local/include during installation.
*/

/* A char must be 8 bits, for compatibility with librosie. */
#include <limits.h>
#if (CHAR_BIT != 8)
#error "Size of char is not 8 bits and will conflict with byte_ptr in str.h"
#endif

#define byte_ptr const uint8_t *

typedef struct rosie_string {
     uint32_t len;
     byte_ptr ptr;
} str;

typedef struct rosie_matchresult {
     str data;
     int leftover;
     int abend;
     int ttotal;
     int tmatch;
} match;

// -----------------------------------------------------------------------------

str  rosie_new_string (byte_ptr msg, size_t len);
str *rosie_new_string_ptr (byte_ptr msg, size_t len);
str *rosie_string_ptr_from (byte_ptr msg, size_t len);
str  rosie_string_from (byte_ptr msg, size_t len);
void rosie_free_string (str s);
void rosie_free_string_ptr (str *s);

void rosie_home_init(str *runtime, str *messages);

Engine *rosie_new (str *messages);
void    rosie_finalize (Engine *e);

int rosie_libpath (Engine *e, str *newpath);
int rosie_alloc_limit (Engine *e, int *newlimit, int *usage);
int rosie_config (Engine *e, str *retvals);
int rosie_compile (Engine *e, str *expression, int *pat, str *messages);
int rosie_free_rplx (Engine *e, int pat);
int rosie_match (Engine *e, int pat, int start, char *encoder, str *input, match *match);
int rosie_trace (Engine *e, int pat, int start, char *trace_style, str *input, int *matched, str *trace);
int rosie_load (Engine *e, int *ok, str *src, str *pkgname, str *messages);
int rosie_loadfile (Engine *e, int *ok, str *fn, str *pkgname, str *messages);
int rosie_import (Engine *e, int *ok, str *pkgname, str *as, str *actual_pkgname, str *messages);
int rosie_read_rcfile (Engine *e, str *filename, int *file_exists, str *options, str *messages);
int rosie_execute_rcfile (Engine *e, str *filename, int *file_exists, int *no_errors, str *messages);

int rosie_expression_refs (Engine *e, str *input, str *refs, str *messages);
int rosie_block_refs (Engine *e, str *input, str *refs, str *messages);
int rosie_expression_deps (Engine *e, str *input, str *deps, str *messages);
int rosie_block_deps (Engine *e, str *input, str *deps, str *messages);
int rosie_parse_expression (Engine *e, str *input, str *parsetree, str *messages);
int rosie_parse_block (Engine *e, str *input, str *parsetree, str *messages);

// -----------------------------------------------------------------------------

/* 
   CLI and REPL support: These functions are here for convenience
   only.  They may be factored out into a separate library in the
   future.
*/
int rosie_exec_cli (Engine *e, int argc, char **argv, char **err);
int rosie_exec_lua_repl (Engine *e, int argc, char **argv);

/* 
   rosie_matchfile() tries to match every line of a file.  It writes
   to stdout and stderr (unless those are /dev/null).  It is in
   librosie to support building a CLI/REPL, because we expect it to be
   faster than feeding one line at a time through rosie_match().
*/
int rosie_matchfile (Engine *e, int pat, char *encoder, int wholefileflag,
		     char *infilename, char *outfilename, char *errfilename,
		     int *cin, int *cout, int *cerr,
		     str *err);
// -----------------------------------------------------------------------------

/* 
   New (August, 2021) interface that will eventually replace
   rosie_match().  The new one, below, is implemented in a way that
   bypasses Lua when possible, and goes through Lua in a limited way
   when necessary (which is when the caller requests an encoder that
   is implemented in Lua).
*/

int rosie_match2 (Engine *e, uint32_t pat, char *encoder_name,
		  str *input, uint32_t startpos, uint32_t endpos,
		  struct rosie_matchresult *match,
		  uint8_t collect_times);

#endif
