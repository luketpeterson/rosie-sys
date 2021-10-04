/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  librosie.h                                                               */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018, 2019.                                */
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

/* These codes are returned in the length field of an str whose ptr is
 * NULL as a cheap way to give the caller an explanation when an error
 * occurred.  Zero is reserved for the "no match" situation (not an
 * error, just an indication that a match did not succeed).
 */
#define NO_MATCH 0		/* for the 'bool' output encoder */
#define MATCH_WITHOUT_DATA 1	/* for the 'bool' output encoder */
#define ERR_NO_ENCODER 2	/* also used for "no trace style" */
#define ERR_NO_FILE 3		/* no such file or directory */
#define ERR_NO_PATTERN 4

#include <pthread.h>		/* TODO: make this conditional */

typedef struct rosie_engine {
     void *L;
     pthread_mutex_t lock;
} Engine;

#define byte_ptr uint8_t *

typedef struct rosie_string {
     uint32_t len;
     byte_ptr ptr;
} rstr;

typedef struct rosie_string str;

typedef struct rosie_matchresult {
     str data;
     int leftover;
     int abend;
     int ttotal;
     int tmatch;
} match;

str rosie_new_string(byte_ptr msg, size_t len);
str *rosie_new_string_ptr(byte_ptr msg, size_t len);
str *rosie_string_ptr_from(byte_ptr msg, size_t len);
str rosie_string_from(byte_ptr msg, size_t len);
void rosie_free_string(str s);
void rosie_free_string_ptr(str *s);

void rosie_home_init(str *runtime, str *messages);
Engine *rosie_new(str *messages);
void rosie_finalize(Engine *e);
int rosie_libpath(Engine *e, str *newpath);
int rosie_alloc_limit(Engine *e, int *newlimit, int *usage);
int rosie_config(Engine *e, str *retvals);
int rosie_compile(Engine *e, str *expression, int *pat, str *messages);
int rosie_free_rplx(Engine *e, int pat);
int rosie_match(Engine *e, int pat, int start, char *encoder, str *input, match *match);
int rosie_matchfile(Engine *e, int pat, char *encoder, int wholefileflag,
		    char *infilename, char *outfilename, char *errfilename,
		    int *cin, int *cout, int *cerr,
		    str *err);
int rosie_trace(Engine *e, int pat, int start, char *trace_style, str *input, int *matched, str *trace);
int rosie_load(Engine *e, int *ok, str *src, str *pkgname, str *messages);
int rosie_loadfile(Engine *e, int *ok, str *fn, str *pkgname, str *messages);
int rosie_import(Engine *e, int *ok, str *pkgname, str *as, str *actual_pkgname, str *messages);
int rosie_read_rcfile(Engine *e, str *filename, int *file_exists, str *options, str *messages);
int rosie_execute_rcfile(Engine *e, str *filename, int *file_exists, int *no_errors, str *messages);

int rosie_expression_refs(Engine *e, str *input, str *refs, str *messages);
int rosie_block_refs(Engine *e, str *input, str *refs, str *messages);
int rosie_expression_deps(Engine *e, str *input, str *deps, str *messages);
int rosie_block_deps(Engine *e, str *input, str *deps, str *messages);
int rosie_parse_expression(Engine *e, str *input, str *parsetree, str *messages);
int rosie_parse_block(Engine *e, str *input, str *parsetree, str *messages);

/* CLI and REPL support: These functions are here for convenience
   only.  They may be factored out into a separate library in the
   future.
*/
int rosie_exec_cli(Engine *e, int argc, char **argv, char **err);
int rosie_exec_lua_repl(Engine *e, int argc, char **argv);

/*

Administrative:
+  status:int, engine:void* = new(const char *name)
+  status:int = finalize(void *engine)
+  status:int, desc:string = config(void *engine)
*  status:int = setlibpath(void *engine, const char *libpath)
+  set soft memory limit to m MB, with optional logging of when it is hit
  logging level (to stderr)?
  clone an engine?  (to avoid setup cost; but cloned engine must be in new Lua state)


RPL:
+  status:int, pkgname:string, errors:strings = load(void *engine, const char *rpl)
+  status:int, pkgname:string, errors:strings = import(packageref, localname)
  status:int = undefine(id)
  test(rpl)?
  testfile(filename)?

Match/trace:
+  status:int, pat:int, errors:strings = compile(void *engine, const char *expression)
+  status:int = free_rplx(void *engine, int pat)
+  status:int = match(void *engine, int pat, int start, str *encoder,
		str *input, match *match);
+  status:int, tracestring:*buffer = trace(void *engine, int pat, buffer *input, int start, int encoder, int tracestyle)

  status:int, cin:int, cout:int, cerr:int, errors:strings =
    matchfile(void *engine, int pat, 
       const char *infilename, const char *outfilename, const char *errfilename, 
       int start, int encoder, int wholefile)

  status:int, cin:int, cout:int, cerr:int, errors:strings =
    tracefile(void *engine, void pat, 
       const char *infilename, const char *outfilename, const char *errfilename, 
       int start, int encoder, int readmethod, int tracestyle)

Debugging:
  status:int, desc:string = lookup(void *engine, const char *id)
  status:int, expr:string, errors:strings = expand(void *engine, const char *expr)
  status:int, descs:strings = list(void *engine, const char *localnamefilter, const char *packagenamefilter)

*/

#endif
