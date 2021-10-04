/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  file-test.c                                                              */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include "rplx.h"
#include "file.h"

void error(const char *msg) {
  fprintf(stderr, "Error: %s\n", msg);
  fflush(stderr);
  exit(-1);
}

/* TEMPORARY */
#define MESSAGES_LEN(array) ((int) ((sizeof (array) / sizeof (const char *))))
void rplx_error(int code, const char *msg) {
  const char *explanation = "INVALID ERROR CODE";
  if ((code > 0) && (code < MESSAGES_LEN(FILE_MESSAGES)))
    explanation = FILE_MESSAGES[code];
  fprintf(stderr, "Error: %s; %s\n", explanation, msg);
  fflush(stderr);
  exit(-1);
}

int main(int argc, char **argv) {

  int err;
  Chunk c;

  err = file_load("data/THISFILEDOESNOTEXIST",  &c);
  assert( err == FILE_ERR_NOFILE );

  err = file_load("data/empty.rplx",  &c);
  assert( err == FILE_ERR_READ );

  err = file_load("data/net.any.rplx",  &c);
  if (err) rplx_error(err, "expected net.any.rplx to load successfully");

  /* verify rplx has correct attributes */
  assert( c.codesize == 33516 );
  assert( c.codesize * sizeof(Instruction) == 268128 );
  assert( c.ktable->blocksize == 1636 );
  assert( c.ktable->blocknext == 1636 );
  assert( c.ktable->size == 173 );
  assert( c.ktable->next == 173 );

  /* free rplx */
  rplx_free(&c);



/*   printf("%zu; %d\n", c.ktable->blocksize, c.ktable->blocknext); */
/*   printf("%d; %d\n", c.ktable->size, c.ktable->next); */

  printf("Done.\n");

}

