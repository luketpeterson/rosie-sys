/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  decode-test.c                                                            */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

/* Mark S. reports that 98% of executed instructions are ITestSet,
   IAny, IPartialCommit (in that order).

   ITestSet is in the OffsetGroup: offset(22) opcode(7) 010
   IAny is in the BareGroup: 0 opcode(21) unused(6) 110
   IPartialCommit is in the OffsetGroup: offset(22) opcode(7) 010

   Want to generate a file of 32-bit values with this distribution:
   (Guessing at proportions within that 98%, since I don't have the data...)
   1. ITestSet       50% -> encode ITestSet with random offset
   2. IAny           40% -> encode IAny
   3. IPartialCommit  8% -> encode IPartialCommit with random offset
   4. all others      2%
      a.        IChar 1% -> encode IChar with 3 random chars
      b.    ITestChar 1% -> encode ITestChar with random char, random offset
   
   The "all others" category is not based on any data.  I just want to
   get instructions from those other two groups (MultiCharGroup and
   TestChar) into the mix.

*/

#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "../icode.h"

static void printInstRec (InstRec i) {
  printf("Opcode %d", i.opcode);
  if (InOffsetGroup(i.opcode) || InTestCharGroup(i.opcode) || (i.opcode==IOpenCapture))
    printf(", offset %d", i.offset);
  if (InMultiCharGroup(i.opcode))
    printf(", chars %d, %d, %d", i.data.chars[0], i.data.chars[1], i.data.chars[2]);
  if (InTestCharGroup(i.opcode))
    printf(", chars %d", i.data.chars[0]);
  if (i.opcode == IOpenCapture)
    printf(", kind %d", i.data.kind);
  printf("\n");
}

int main(int argc, char **argv) {

  if ( ((argc != 2) && (argc != 3)) ||
       ((argc > 1) &&
	(((*argv[1] != 'd') && (*argv[1] != 'n')) || ((*(argv[1]+1) != '\0') && (*(argv[1]+1) != 'x')))
	)
       ) {
    printf("Usage: %s d/dx/n/nx [file_of_printed_ints]\n", argv[0]);
    printf("  d = decode, dx = decode but do not print output\n");
    printf("  n = no decode, nx = no decode and do not print output\n");
    exit(-1);
  }

  InstRec i;
  int32_t value;

  /* ---------------------------------------------------------------------------------------- */

  printf("Encoding/decoding examples:\n");
  InstRec ex[] = { {IOpenCapture,    12345, {127}},
		   {IOpenCapture,        1,   {0}},
		   {IOpenCapture,  2097151,   {1}},
		   {IOpenCapture,  1,   {2}},
		   /* OffsetGroup */
		   {IBehind, -2097152, {0}},
		   {ITestSet, 1, {0}},
		   {IPartialCommit, 2, {0}},
		   {ITestAny, -1, {0}},
		   {IJmp, -2, {0}},
		   {ICall, 2097151, {0}},
		   {IOpenCall, -2097152, {0}},
		   {IChoice, 1, {0}},
		   {ICommit, 2, {0}},
		   {IBackCommit, 3, {0}},
		   {IBackref, -40000, {0}},
		   /* MultiCharGroup */
		   {IChar, 0, .data.chars = {255, 127, 1}},
		   /* BareGroup */
		   {IAny,  0, {0}},
		   {ISet, 0, {0}},
		   {ISpan, 0, {0}},
		   {IRet, 0, {0}},
		   {IEnd, 0, {0}},
		   {IHalt, 0, {0}},
		   {IFailTwice, 0, {0}},
		   {IFail, 0, {0}},
		   {IGiveup, 0, {0}},
		   {ICloseCapture, 0, {0}},
		   /* TestChar */
		   {ITestChar, 1234567, .data.chars[0] = 255},
		   {0, 0, {0}}
  };
  int n = -1;
  while( ex[++n].opcode != 0 ) {
    value = encode(ex[n]);
    printf("%12d  <--  ", value); printInstRec(ex[n]);
    i = decode(value);
    printf("                   "); printInstRec(i);
    assert( value != InvalidEncoding );
    assert( i.opcode == ex[n].opcode );
    if (InMultiCharGroup(i.opcode)) {
      assert( i.data.chars[0] == ex[n].data.chars[0] );
      assert( i.data.chars[1] == ex[n].data.chars[1] );
      assert( i.data.chars[2] == ex[n].data.chars[2] );
    } else if (InTestCharGroup(i.opcode)) {
      assert( i.data.chars[0] == ex[n].data.chars[0] );
    } else if (i.opcode==IOpenCapture) {
      assert( i.data.kind == ex[n].data.kind );
    }
    if (!InBareGroup(i.opcode) && !InMultiCharGroup(i.opcode)) {
      assert( i.offset == ex[n].offset );
    }
  }

  /* ---------------------------------------------------------------------------------------- */

  printf("Decoding input:\n");
  int decode_flag = (*argv[1] == 'd'); /* 'decode' or 'no decode' */
  int output_flag = !(*(argv[1]+1) == 'x'); /* 'output' or 'no output' */

  char *line;
  size_t len;
  FILE *in;

  if (argc == 2) in = stdin;
  else {
    in = fopen(argv[2], "r");
    if (!in) {
      printf("Could not open file %s\n", argv[2]);
      exit(-1);
    }
  }

  while ((line = fgetln(in, &len))) {
    line[len-1] = '\0';		/* overwrite the newline */
    value = strtoimax((const char *) line, NULL, 10);
    if (errno) {
      fprintf(stderr, "Error in strtoimax\n");
      exit(-1);
    }
    i.opcode = 0;
    i.offset = 0;
    i.data.kind = 0;
    if (decode_flag) i = decode(value);
    if (output_flag) printInstRec(i);
  }
  fclose(in);
}
