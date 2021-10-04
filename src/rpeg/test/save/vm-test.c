/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  vm-test.c                                                                */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/errno.h>

#include "../config.h"
#include "../rplx.h"
#include "../file.h"
#include "../buf.h"
#include "../vm.h"
#include "../json.h"
#include "../capture.h"

void error(const char *message, const char *additional) {
  fprintf(stderr, "Error: %s; %s\n", message, additional);
  fflush(stderr);
  exit(-1);
}

/* --------------------------------- TODO: Move these into a common code file. --------------------------------- */

/* We will not worry about freeing the error message allocated on the
 * heap by strerror_error, because this function is a debugging aid.
 * It is only invoked when there is no message (string) defined for a
 * particular code (int) in the message list.
 */
const char *strerror_error(const char *filename, int line, int code) {
  char *error_message;
  if (!asprintf(&error_message, "%s:%d: INVALID ERROR CODE %d", filename, line, code))
    return "ERROR: asprintf failed";
  return error_message;
}

#define MESSAGES_LEN(array) ((int) ((sizeof (array) / sizeof (const char *))))

#define STRERROR(code, message_list)					\
  ( (((code) > 0) && ((code) < MESSAGES_LEN(message_list)))		\
    ? (message_list)[(code)]						\
    : strerror_error(__FILE__, __LINE__, code) )

/* ------------------------------------------------------------------------------------------------------------- */

void dumpMatch(Match *m) {
  printf(m->matched ? "Match" : "No match");
  if (m->matched && m->abend) printf(" ABEND");
  printf(", leftover = %d bytes\n", m->leftover);
  if (m->matched) { 
    if (!m->data) printf("NO DATA!!!\n");
    else {
      printf("Match data has length %zu\n", m->data->n);
      printf("%*s\n", (int) m->data->n, m->data->data); 
    } 
  } else {
    printf("No match\n");
  }
}

/* ------------------------------------------------------------------------------------------------------------- */

int main(int argc, char **argv) {

  int err;
  Chunk c;

  printf("\nPart 1: test the conversion of each FILE error code to a useful message\n");
  for (err = -1; err < 10; err++)
    printf("code %d: %s\n", err, STRERROR(err, FILE_MESSAGES));

  printf("\nPart 2: read the file net.any.rplx and check its stats\n");

  err = file_load("data/net.any.rplx",  &c);
  if (err) error(STRERROR(err, FILE_MESSAGES), "expected net.any.rplx to load successfully");

  /* verify rplx has correct attributes */
  assert( c.codesize == 134261 );
  assert( c.codesize * sizeof(Instruction) == 1074088 );
  assert( c.ktable->blocksize == 3288 );
  assert( c.ktable->blocknext == 3288 );
/*   assert( c.ktable->size == 347 ); */
  assert( c.ktable->next == 348 );

  char *text = "jamie@plaintextlabs.com";
  Buffer *input = buf_from(text, strlen(text));
  Match *match = match_new();
  assert( match->data == NULL );
  
  Encoder json_encoder = { json_Open, json_Fullcapture, json_Close };
  Encoder noop_encoder = { noop_Open, noop_Fullcapture, noop_Close };

  printf("\nPart 3: match an email address using net.any\n");

  printf("Input data: %.*s\n", (int) input->n, input->data);

  err = vm_match(&c, input, 1, noop_encoder /*json_encoder*/, match, NULL);
  if (err) printf("\n*** err is %d\n\n", err);
  if (err) error(STRERROR(err, MATCH_MESSAGES), "expected successful match");

  dumpMatch(match);

  printf("\nPart 4: read the file all.things.rplx and use it\n");

  rplx_free(&c);
  
  err = file_load("data/all.things.rplx",  &c);
  if (err) error(STRERROR(err, FILE_MESSAGES), "expected all.things.rplx to load successfully");

  FILE *f = fopen("/etc/resolv.conf", "r");
  if (!f) error(strerror(errno), "expected to open /etc/resolv.conf to use as sample input");
  fseek(f, 0, SEEK_END);
  size_t len = ftell(f);
  printf("length of input file is %zu\n", len);
  fseek(f, 0, SEEK_SET);
  buf_free(input);
  input = buf_new(len);
  if (!input) error("out of memory", "expected to allocate buffer big enough for input file");
  printf("input buffer: input->capacity = %zu, input->n = %zu\n", input->capacity, input->n);
  fread(input->data, 1, len, f);
  input->n = len;
  fclose(f);

  /* Now match all.things against /etc/resolv.conf */

  buf_reset(match->data);
  err = vm_match(&c, input, 1, json_encoder, match, NULL);
  if (err) error(STRERROR(err, MATCH_MESSAGES), "expected successful match");

  dumpMatch(match);
  

  printf("\nDone.\n");

}

