/*  -*- Mode: C; -*-                                                        */
/*                                                                          */
/*  file.h                                                                  */
/*                                                                          */
/*  © Copyright Jamie A. Jennings 2018.                                     */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html) */
/*  AUTHOR: Jamie A. Jennings                                               */

#if !defined(file_h)
#define file_h

#include "rplx.h"

#define MAX_INSTLEN_BYTES (10 * 1000 * 1000)

#define FILE_MAGIC_NUMBER "RPLX"

typedef enum FileErr {
  FILE_OK,
  FILE_ERR_NOFILE, FILE_ERR_WRITE, FILE_ERR_READ,
  FILE_ERR_MAGIC_NUMBER, FILE_ERR_KTABLE_LEN, FILE_ERR_INST_LEN,
  FILE_ERR_MEM, FILE_ERR_KTABLE_SIZE, FILE_ERR_SENTINEL,
} FileErr;

static const char *FILE_MESSAGES[] __attribute__((unused)) = {
  "ok",                          /* 0 */
  "cannot open file",            /* 1 */
  "write error",                 /* 2 */
  "read error",                  /* 3 */
  "wrong magic number",          /* 4 */
  "too many ktable entries",     /* 5 */
  "instruction vector too long", /* 6 */
  "out of memory",               /* 7 */
  "ktable total size too long",  /* 8 */
};

int file_save (const char *filename, Chunk *c);
int file_load (const char *filename, Chunk *c);

#endif

