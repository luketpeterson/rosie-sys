/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  match.c                                                                  */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018, 2019.                                */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

/*  THIS IS ONLY A TEST!  IT IS A PROOF OF CONCEPT ILLUSTRATING HOW SMALL
 *  AN EXECUTABLE COULD BE IF IT CONTAINS ONLY THE ROSIE RUN-TIME.
 */

//  gcc match.c -o match -I../include ../runtime/*.o
//  ./match data/findall:net.ipv4.rplx data/log10.txt | json_pp


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/errno.h>

#include "config.h" 
#include "rplx.h" 
#include "file.h"
#include "buf.h"
#include "vm.h"
#include "json.h"
#include "capture.h"

static void error(const char *message, const char *additional) {
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
static const char *strerror_error(const char *filename, int line, int code) {
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

int main(int argc, char **argv) {

  if ((argc != 2) && (argc != 3)) {
    printf("Usage: %s <rplx_file> [input_file]\n", argv[0]);
    exit(-1);
  }

  int err;
  Chunk c;

  Encoder json_encoder = { json_Open, json_Close };
  Encoder noop_encoder = { noop_Open, noop_Close };
  //  Encoder debug_encoder = { debug_Open, debug_Close };

  Encoder *encoder = &json_encoder;

  err = file_load(argv[1],  &c);
  if (err) error(STRERROR(err, FILE_MESSAGES), "expected rplx file to load successfully");

  FILE *f;
  if (argc == 2) f = stdin;
  else { f = fopen(argv[2], "r"); }
  if (!f) error(strerror(errno), "expected to open input file");

  char *line = NULL;
  size_t linelen = 0;
  int len;
  Match *match = match_new();
  Buffer input;
  Stats stats = {0};

  while ((len = getline(&line, &linelen, f)) != -1) {
    if ((len > 0) && (*(line + len - 1) == '\n')) len -= 1; 
    input.n = len;
    input.data = line;
    if (match->data) buf_reset(match->data);  
    err = vm_match(&c, &input, 1, *encoder, match, (DEBUG ? &stats : NULL)); 
    if (err) error(STRERROR(err, MATCH_MESSAGES), "expected successful match"); 
    if (match->matched) {
      if (encoder == &noop_encoder) {
	fwrite(input.data, 1, input.n, stdout); /* '-o data' */
      } else {
	fwrite(match->data->data, 1,  match->data->n, stdout);
      }
      puts("");
      #if DEBUG      
      printf("  Stats:  total time %d, match time %d, insts %d, backtrack %d, caplist %d, capdepth %d\n",
	     stats.total_time, stats.match_time, stats.insts,
	     stats.backtrack, stats.caplist, stats.capdepth);
      #endif
    }
  }
  match_free(match);
  fclose(f);
}

