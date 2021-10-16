/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  capture.c  The byte and debug (printing) capture processors              */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  © Copyright IBM Corporation 2017.                                        */
/*  Portions Copyright 2007, Lua.org & PUC-Rio (via lpeg)                    */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#if defined(__GNUC__)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "config.h"
#include "buf.h"
#include "ktable.h" 
#include "capture.h"

/* With all output encoders, the caller must make a balanced sequence
 * of calls to Open and Close.  In vm.c, caploop() ensures this.
 */


/* ---------------------------------------------------------------------------------------- 
 * The 'debug' output encoder simply prints the details of the match results.
 *
 */

static void print_capture(CapState *cs) {
  size_t len;
  Capture *c = cs->cap;
  printf("  kind = %s\n", CAPTURE_NAME(capkind(c)));
  printf("  pos (1-based) = %lu\n", c->s ? (c->s - cs->s + 1) : 0);
  const char *str = ktable_element_name(cs->kt, capidx(c), &len);
  if (str) {
    printf("  idx = %u\n", capidx(c));
    printf("  ktable[idx] = %.*s\n", (int) len, str);
  }
}

static void print_constant_capture(CapState *cs) {
  size_t len;
  const char *str = ktable_element_name(cs->kt, capidx(cs->cap), &len);
  printf("  constant match: %.*s\n", (int) len, str);
}

int debug_Close(CapState *cs, Buffer *buf, int count, const char *start) {
  UNUSED(buf); UNUSED(count); UNUSED(start);
  if (isopencap(cs->cap)) return MATCH_CLOSE_ERROR;
  if (capkind(cs->cap) == Ccloseconst)
    print_constant_capture(cs);
  printf("CLOSE:\n");
  print_capture(cs);
  return MATCH_OK;
}

int debug_Open(CapState *cs, Buffer *buf, int count) {
  UNUSED(buf); UNUSED(count);
  if (!acceptable_capture(capkind(cs->cap))) return MATCH_OPEN_ERROR;
  printf("OPEN:\n");
  print_capture(cs);
  return MATCH_OK;
}

/* ---------------------------------------------------------------------------------------- 
 * The 'byte' output encoder emits a compact (and simple) linear encoding:
 *
 * Cap := -start_pos -shortlen <name> shortlen <constdata> end_pos
 *     |= -start_pos shortlen <name> end_pos
 *
 * Where start_pos, end_pos are 32-bit unsigned ints and shortlen is unsigned 16-bit int.
 */


/* TODO: We are encoding ktable indices as shorts, but they can NOW be 21 bits!  */
/* TODO: Expand the addressing of input strings so that strings larger
   than 4GB can be used as input.  Today, these might be regular
   heap-allocated strings, or memory mapped files.  In the future, who
   knows?

   IDEA: In the byte encoding header, indicate whether positions are
   encoded using 1, 2, 3, or 4 16-bit quantities. One bit is used by
   the encoding process, so positions are encoded in 15, 31, 47, or 63
   bits. When decoding the byte format, the decoder must ensure that
   they have enough bits, e.g. that size_t is big enough if in C.
*/

/* The byte array encoding assumes that the input text length fits
 * into 2^31, i.e. a signed int, and that the name length fits into
 * 2^15, i.e. a signed short.  It is the responsibility of caller to
 * ensure this. 
*/

static void encode_pos(size_t pos, int negate, Buffer *buf) {
  int intpos = (int) pos;
  if (negate) intpos = - intpos;
  buf_addint(buf, intpos);
}

static void encode_string(const char *str, size_t len,
			  int shortflag, int negflag, Buffer *buf) {
  assert(str);
  /* encode size as a short or an int */
  if (shortflag) buf_addshort(buf, (short) (negflag ? -len : len));
  else buf_addint(buf, (int) (negflag ? -len : len));
  /* encode the string by copying it into the buffer */
  buf_addlstring(buf, str, len); 
}

static void encode_ktable_element(CapState *cs, byte negflag, Buffer *buf) {
  size_t len;
  const char *name = ktable_element_name(cs->kt, capidx(cs->cap), &len); 
  assert( name );
  //assert( len > 0 );  //len could be zero?
  encode_string(name, len, 1, negflag, buf); /* shortflag is set */
}

int byte_Close(CapState *cs, Buffer *buf, int count, const char *start) {
  size_t e;
  UNUSED(count); UNUSED(start);
  if (isopencap(cs->cap)) return MATCH_CLOSE_ERROR;
  if (capkind(cs->cap) == Ccloseconst) encode_ktable_element(cs, 0, buf);
  e = cs->cap->s - cs->s + 1;	/* 1-based end position */
  encode_pos(e, 0, buf);
  return MATCH_OK;
}

int byte_Open(CapState *cs, Buffer *buf, int count) {
  size_t s;
  UNUSED(count);
  if (!acceptable_capture(capkind(cs->cap))) {
    printf("byte_Open: capkind is %d (%s)\n", capkind(cs->cap), CAPTURE_NAME(capkind(cs->cap)));
    return MATCH_OPEN_ERROR;
  }
  s = cs->cap->s - cs->s + 1;	/* 1-based start position */
  assert(capidx(cs->cap) >= 0);
  encode_pos(s, 1, buf);
  encode_ktable_element(cs, (capkind(cs->cap) == Crosieconst), buf);
  return MATCH_OK;
}


Encoder debug_encoder = { debug_Open, debug_Close };
Encoder byte_encoder = { byte_Open, byte_Close };

