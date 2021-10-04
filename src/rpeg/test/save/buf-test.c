/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  buf-test.c                                                               */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../buf.h"

void *error(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  fflush(stderr);
  exit(-1);
}

int main(int argc, char **argv) {

  int ok;
  
  printf("Part 1: test all functions of one buffer\n");

  Buffer *b1 = buf_new(0);
  assert(b1->capacity == INITIAL_BUFFER_SIZE);
  assert(b1->n == 0);
  
  buf_addstring(b1, "Hello, world!");
  size_t len;
  char *s = buf_substring(b1, 1, 5, &len);
  assert( s == b1->data );
  assert( len == 5 );

  s = buf_substring(b1, 10, 10, &len);
  assert( len == 1 );
  assert( s = b1->data + 10 );

  s = buf_substring(b1, -2, -2, &len);
  assert( len == 1 );
  assert( s = b1->data + (b1->n - 2) );

  s = buf_substring(b1, -2, 0, &len);
  assert( len == 2 );
  assert( s = b1->data + (b1->n - 2) );
  
  s = buf_substring(b1, -100, 0, &len);
  assert( len == 13 );
  assert( s = b1->data );

  s = buf_substring(b1, 0, -2, &len);
  assert( len == 12 );
  assert( s = b1->data );

  buf_addstring(b1, "foobar");
  assert( b1->n == 19 );
  
  s = buf_substring(b1, 0, -2, &len);
  assert( len == 12 + 6 );
  assert( s = b1->data );

  buf_addint(b1, 1234567890);
  assert( b1->n == 19 + 4 );
  char *p = b1->data + b1->n - 4;
  int i = buf_peekint(p);
  assert( i == 1234567890 );
  char *save_p = p;
  int j = buf_readint((const char **)&p);
  assert( i == j );
  assert( p == save_p + 4 );

  /* now repeat that with SHORT */

  buf_addshort(b1, 16385);
  assert( b1->n == 19 + 4 + 2 );
  p = b1->data + b1->n - 2;
  short ii = buf_peekshort(p);
  assert( ii == 16385 );
  save_p = p;
  short jj = buf_readshort((const char **)&p);
  assert( ii == jj );
  assert( p == save_p + 2 );
  

  /* ---------------------------------------------------------------------------------------- */
  printf("Part 2: growing the buffer\n");

  char silly[INITIAL_BUFFER_SIZE];
  for (int i=0; i < INITIAL_BUFFER_SIZE; i++) silly[i] = (char)(i & 0xff);

  buf_addlstring(b1, silly, INITIAL_BUFFER_SIZE);
  /* capacity should have doubled */
  assert( b1->capacity == 2 * INITIAL_BUFFER_SIZE );
  assert( b1->n = 19 + 4 + 2 + INITIAL_BUFFER_SIZE );
  
  /* capacity should increase by 3 * INITIAL_BUFFER_SIZE */
  buf_prepsize(b1, 3 * INITIAL_BUFFER_SIZE);
  assert( b1->capacity == 19 + 4 + 2 + INITIAL_BUFFER_SIZE + (3 * INITIAL_BUFFER_SIZE) );

  buf_reset(b1);
  /* capacity should be unchanged */
  assert( b1->capacity == 19 + 4 + 2 + INITIAL_BUFFER_SIZE + (3 * INITIAL_BUFFER_SIZE) );
  assert( b1->n == 0 );
  

  /* ---------------------------------------------------------------------------------------- */
  printf("Part 3: wrapping existing data in a buffer\n");

  Buffer *b2 = buf_from(silly, 100);
  assert( b2->capacity == 100 );
  assert( b2->n == b2->capacity );
  Buffer *temp = buf_addlstring(b2, silly, INITIAL_BUFFER_SIZE);
  /* Cannot resize a wrapped buffer (error returned as NULL) */
  assert( temp == NULL );
  assert( b2->capacity == 100 );
  assert( b2->n = 100 );
  assert( b2->data[0] == '\0' );
  assert( b2->data[1] == '\1' );
	  
  /* ---------------------------------------------------------------------------------------- */
  printf("Part 4: freeing buffers\n");

  buf_free(b2);
  /* no changes, because freeing a wrapped buffer is a no-op */
  assert( b2->capacity == 100 );
  assert( b2->n = 100 );
  assert( b2->data[0] == '\0' );
  assert( b2->data[1] == '\1' );
  free(b2);
  
  /* run with valgrind to ensure no leaks */
  buf_free(b1);
  free(b1);
  
}
