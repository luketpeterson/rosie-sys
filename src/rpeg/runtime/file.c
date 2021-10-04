/*  -*- Mode: C; -*-                                                        */
/*                                                                          */
/*  file.c  Read/write binary RPL files                                     */
/*                                                                          */
/*  © Copyright Jamie A. Jennings 2018.                                     */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html) */
/*  AUTHOR: Jamie A. Jennings                                               */


#define RPLX_FILE_MIN_VERSION 0	      /* min version this code will accept  */
#define RPLX_FILE_MAX_VERSION 0	      /* max version this code will accept  */

#ifndef FILEDEBUG
#define FILEDEBUG 0
#else
#define FILEDEBUG 1
#endif

/* TODO
 *
 * Add meta-data to binary files, including:
 *   module name (for debugging/information purposes)
 *   source file timestamp (st_mtimespec from stat(2), 16 bytes) and length (st_size, 8 bytes)
 *   maybe whether a non-standard prelude was used? would be good debugging info.
 *   line number (in source file) for each pattern
 *
 * Write compiled library files to:
 *   rplx subdirectory of source directory
 * 
 * New import behavior for 'import X'
 *   For each directory D on the (ordered) libpath:
 *     If D/X.rplx exists, load it
 *     Elseif D/X.rpl exists, load D/rplx/D.rplx if not stale, else recompile
 *
 * If rplx subdirectory cannot be created or cannot be written to:
 *   Warn if log level is higher than "completely silent"
 *
 * Cache rpl expressions used at the CLI?  FUTURE, IF NEEDED.
 *   Possible approach: Could write temporary rplx files to a cache
 *   directory, with an index file.  The index could be an LRU list of
 *   n recent expressions (including any rpl given on the command
 *   line, and any imports, auto or explicit).  If the current CLI
 *   invocation matches the index entry exactly, AND the imports are
 *   not stale, then use the compiled file from the cache.
 *
 * New rosie CLI structure, based on command entered:
 *   match X.y    import X, and if compiled, then match using X.y
 *   grep X.y     dynamically load rpl compiler, compile findall:X.y, match [note 1]
 *   list *       import prelude as ., list patterns [note 4]
 *   list X.*     import X, list patterns
 *   test f1..fn  load each, compiling if necessary, extract tests 
 *                from source, run tests
 *   expand exp   dynamically load rpl compiler, do macro expansion, print
 *   trace exp    dynamically load rpl compiler, run trace, print [note 2]
 *   repl         dynamically load rpl compiler, invoke repl [note 3]
 *
 *   compile f1..fn  dynamically load rpl compiler, compile and save each
 *   compile exp f   FUTURE (save f.rplx file with anonymous entry point)
 *   dis f1..fn      disassemble each of f1, ... fn [note 3]
 *   
 * [1] Would be nice if the grep command did not need the compiler.
 * This is an optimization that can be implemented later, by
 * generating the find/findall code on the fly from a template.
 *
 * [2] Trace could eventually be much-enhanced, perhaps making use of
 * the vm instructions (i.e. the compiled pattern).  It should become
 * its own dynamically loadable module.
 *
 * [3] The repl and dis could be their own dynamically loadable
 * modules as well. And dis is already a separate executable.
 *
 * [4] The prelude is statically linked with (compiled into) every
 * module, so that each module's patterns run with the prelude that it
 * was written for.
 *
 * New librosie structure, to reflect new rosie CLI structure:
 *   librosie.so    match, search (find), grep (findall), list, test, 
 *                  expand, trace, compile (loading librosiec.so as needed)
 *   librosiec.so   compile (and save), repl, trace (requires librosiel.so)
 *   librosieo.so   output encoders that need lua (requires librosiel.so)
 *   librosiel.so   lua for rosie
 */

#if defined(__GNUC__)
#define _GNU_SOURCE
#endif

#include <stdlib.h>		/* malloc */
#include <sys/param.h>		/* MAXPATHLEN */
#include <string.h>
#include <assert.h>
#include <stdio.h> 

#if defined(__linux__)
#if !defined(__clang__)
#include <bsd/stdio.h>
#endif
#endif

#include "ktable.h"
#include "config.h"
#include "file.h"

static int write_int (FILE *out, int i) {
  unsigned char str[4];
  unsigned int iun = i;
  str[3] = (iun >> 24) & 0xFF;
  str[2] = (iun >> 16) & 0xFF;
  str[1] = (iun >> 8) & 0xFF;
  str[0] = iun & 0xFF;
  return (fwrite(str, 4, 1, out) == 1);
}

static int read_int (FILE *in, int *i) {
  unsigned char str[4];
  size_t items_read = fread(str, 4, 1, in);
  unsigned int iun = str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24);
  *i = iun;
  return (items_read == 1);
}

static int read_newline(FILE *stream) {
  char dummy[1]; 
  if (fread(dummy, 1, 1, stream) != 1) return FILE_ERR_READ;
  if (dummy[0] != '\n') return FILE_ERR_READ;
  return FILE_OK; 
}

#define write_newline(stream) {		   \
  written = fwrite("\n", 1, 1, (stream));  \
  if (written != 1) return FILE_ERR_WRITE; \
  }

/* TODO: fclose(out) before returning an error code */
int file_save (const char *filename, Chunk *chunk) {
  FILE *out;
  size_t bytes, written;
  int ok;

  out = fopen(filename, "wb");
  if (!out) return FILE_ERR_NOFILE;

  /* magic number */
  written = fwrite(FILE_MAGIC_NUMBER, sizeof(FILE_MAGIC_NUMBER), 1, out);
  if (written != 1) return FILE_ERR_WRITE;

  Ktable *kt = chunk->ktable;
  assert( kt );
  assert( kt->block );
  assert( kt->blocknext > 0 );
  assert( kt->elements );
  assert( kt->size > 0 );
  assert( kt->next > 0 );
  #if FILEDEBUG
  fprintf(stderr, "file_save: number of ktable entries is %d\n", kt->next - 1);
  #endif
  ok = write_int(out, kt->next - 1);
  if (!ok) return FILE_ERR_WRITE;
  #if FILEDEBUG
  fprintf(stderr, "file_save: size of ktable block is %d\n", kt->blocknext);
  #endif
  ok = write_int(out, kt->blocknext);
  if (!ok) return FILE_ERR_WRITE;

  write_newline(out);

  bytes = kt->next * sizeof(Ktable_element);
  written = fwrite(kt->elements, bytes, 1, out);   
  if (written != 1) return FILE_ERR_WRITE;

  write_newline(out);
  
  written = fwrite(kt->block, kt->blocknext, 1, out);   
  if (written != 1) return FILE_ERR_WRITE;

  write_newline(out);
  
  #if FILEDEBUG
  fprintf(stderr, "file_save: number of instructions is %zu\n", chunk->codesize);
  #endif
  ok = write_int(out, chunk->codesize);
  #if FILEDEBUG
  fprintf(stderr, "file_save: saved instruction size %s\n", ok ? "ok" : "ERROR");
  #endif
  if (!ok) return FILE_ERR_WRITE;
  written = fwrite(chunk->code, sizeof(Instruction), chunk->codesize, out);   
  if (written != chunk->codesize) return FILE_ERR_WRITE;
  #if FILEDEBUG
  fprintf(stderr, "file_save: number of instructions written is %zu\n", written);
  #endif

  write_newline(out);

  fclose(out);
  return FILE_OK;
}  

/* TODO: fclose(in) before returning an error code */
int file_load (const char *filename, Chunk *chunk) {
  FILE *in;
  size_t len;
  int n, ok, blocksize;
  size_t bytes;
  char magic[sizeof(FILE_MAGIC_NUMBER)];
  
  in = fopen(filename, "rb");
  if (!in) return FILE_ERR_NOFILE;

  len = fread(magic, sizeof(FILE_MAGIC_NUMBER), 1, in);
  if (len != 1) return FILE_ERR_READ;
  if (strncmp(magic, FILE_MAGIC_NUMBER, sizeof(FILE_MAGIC_NUMBER)) != 0)
    return FILE_ERR_MAGIC_NUMBER;

#if FILEDEBUG
  fprintf(stderr, "file_load: magic number ok\n");
#endif

  /* TODO: Insert a read of file version here.  Check against min, max. */
  /* TODO: Insert a read of endianness here. */
  /* TODO: Insert a read of rpl major/minor versions here. */

  ok = read_int(in, &n);
  if (!ok) return FILE_ERR_READ;
  if ((n < 0) || (n > MAX_CAPLISTSIZE)) return FILE_ERR_KTABLE_LEN;

#if FILEDEBUG
  fprintf(stderr, "file_load: number of ktable entries is %d\n", n);
#endif

  ok = read_int(in, &blocksize);
  if (!ok) return FILE_ERR_READ;
#if FILEDEBUG
  fprintf(stderr, "file_load: size of ktable block is %d\n", blocksize);
#endif
  if ((blocksize < 0) || (blocksize > MAX_INSTLEN_BYTES)) return FILE_ERR_KTABLE_SIZE;

  if (read_newline(in) < 0) return FILE_ERR_READ;

  Ktable *kt = ktable_new(n, blocksize);
  kt->blocksize = blocksize;
  kt->size = n;			/* capacity */
  kt->next = n + 1;		/* next element requires more capacity */

  /* N.B. This is dependent on same endianness of file writer and file reader  */
  bytes = (n+1) * sizeof(Ktable_element);
  ok = fread(kt->elements, bytes, 1, in);  
#if FILEDEBUG
  fprintf(stderr, "file_load: read of elements %s\n",
	  (ok != -1) ? "succeeded" : "failed");
#endif
  if (ok != 1) return FILE_ERR_READ;

  if (read_newline(in) < 0) return FILE_ERR_READ;

  ok = fread(kt->block, kt->blocksize, 1, in);
#if FILEDEBUG
  fprintf(stderr, "file_load: read of block %s\n",
	  (ok != -1) ? "succeeded" : "failed");
#endif
  if (ok != 1) return FILE_ERR_READ;
  kt->blocknext = kt->blocksize;

  if (read_newline(in) < 0) return FILE_ERR_READ;
  
  ok = read_int(in, &n);
  if (ok != 1) return FILE_ERR_READ;
  bytes = n * sizeof(Instruction);
  #if FILEDEBUG
  fprintf(stderr, "file_load: number of instructions is %d, bytes is %zu\n", n, bytes);
  #endif
  if ((n < 0) || (bytes > MAX_INSTLEN_BYTES)) return FILE_ERR_INST_LEN;

  Instruction *buf = (Instruction *) malloc((size_t) bytes);
  if (!buf) return FILE_ERR_MEM;
  len = fread((char *)buf, sizeof(Instruction), n, in);

  if (len != (size_t) n) return FILE_ERR_READ;
  #if FILEDEBUG
  fprintf(stderr, "file_load: number of instructions read is %d\n", n);
  #endif
  if (read_newline(in) < 0) return FILE_ERR_READ;

  chunk->codesize = (size_t) n;
  chunk->code = buf;
  chunk->ktable = kt;
  chunk->rpl_major = 0;		/* TODO */
  chunk->rpl_minor = 0;	        /* TODO */
  chunk->file_version = 0;	/* TODO */
  chunk->filename = strndup(filename, MAXPATHLEN);
  
  fclose(in);
  return FILE_OK;
}  

