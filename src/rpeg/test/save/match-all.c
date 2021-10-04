/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  match-all.c                                                              */
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
/* #include "../capture.h" */

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
/*   if (m->matched) { */
    if (!m->data) printf("NO DATA!\n");
    else {
      printf("Match data has length %zu\n", m->data->n);
      printf("%*s\n", (int) m->data->n, m->data->data); 
/*     } */
  }
}

/* ------------------------------------------------------------------------------------------------------------- */

int main(int argc, char **argv) {

  if ((argc != 1) && (argc != 2)) {
    printf("Usage: %s [input_file]\n", argv[0]);
    exit(-1);
  }

  int err;
  Chunk c;

  Encoder json_encoder = { json_Open, json_Fullcapture, json_Close };

  err = file_load("data/all.things.rplx",  &c);
  if (err) error(STRERROR(err, FILE_MESSAGES), "expected all.things.rplx to load successfully");

  FILE *f;
  if (argc == 1) f = stdin;
  else { f = fopen(argv[1], "r"); }
  if (!f) error(strerror(errno), "expected to open input file");

  char *line;
  size_t len;
  Match *match = match_new();
  Buffer *input;
  
  while ((line = fgetln(f, &len)) != NULL) {
    if ((len > 0) && (*(line + len - 1) == '\n')) len -= 1; 
    input = buf_from(line, len);
/*     printf("%.*s\n", (int) input->n, input->data); */
    if (match->data) buf_reset(match->data);  
    err = vm_match(&c, input, 1, json_encoder, match, NULL); 
    if (err) error(STRERROR(err, MATCH_MESSAGES), "expected successful match"); 
    printf("%.*s\n", (int) match->data->n, match->data->data); 
    free(input);
  }
  
  fclose(f);

}

