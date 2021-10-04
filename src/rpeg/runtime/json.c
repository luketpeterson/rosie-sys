/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  json.c  The json capture processor (output encoder)                      */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  Portions Copyright (c) 2010-2012  Mark Pulford <mark@kyne.com.au>        */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "buf.h"
#include "ktable.h" 
#include "capture.h"
#include "json.h"

#define UNUSED(x) (void)(x)

static const char *char2escape[256] = {
    "\\u0000", "\\u0001", "\\u0002", "\\u0003",
    "\\u0004", "\\u0005", "\\u0006", "\\u0007",
    "\\b", "\\t", "\\n", "\\u000b",
    "\\f", "\\r", "\\u000e", "\\u000f",
    "\\u0010", "\\u0011", "\\u0012", "\\u0013",
    "\\u0014", "\\u0015", "\\u0016", "\\u0017",
    "\\u0018", "\\u0019", "\\u001a", "\\u001b",
    "\\u001c", "\\u001d", "\\u001e", "\\u001f",          /* 28 .. 31 */
    NULL, NULL, "\\\"", NULL, NULL, NULL, NULL, NULL,    /* 32 .. 39 */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,      /* 40 .. 47 */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,      /* 64 .. 71 */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, "\\\\", NULL, NULL, NULL,    /* 88 .. 95 */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, "\\u007f", /* 120 .. 127 */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};


/* Worst case is len * 6 (all unicode escapes). By reserving all of
 * this space in advance, we gain 15-20% performance improvement.
 */

static int addlstring_json(Buffer *buf, const char *str, size_t len)
{
  assert( sizeof(char2escape) == 256 * sizeof(char *) );
  static const char dquote = '\"';
  const char *escstr;
  size_t esclen;
  char c;
  size_t i;
  buf_prepsize(buf, 2 + 6*len);
  buf_addchar_UNSAFE(buf, dquote);
  for (i = 0; i < len; i++) { 
    c = str[i];
    /* this explicit test on c gives about a 5% speedup on typical data */
    if ((c=='\"') || (c=='\\') || (c>0 && c<32) || (c==127)) {
      escstr = char2escape[(unsigned char)c]; 
      esclen = strlen(escstr);	/* escstr is null terminated */ 
      buf_addlstring_UNSAFE(buf, escstr, esclen);
    }
    else buf_addchar_UNSAFE(buf, c);
  } 
  buf_addchar(buf, dquote);
  return MATCH_OK;
}

static void json_encode_pos(size_t pos, Buffer *buf) {
  char nb[MAXNUMBER2STR];
  size_t len;
  len = r_inttostring(nb, (int) pos);
  buf_addlstring(buf, nb, len);
}

static void json_encode_ktable_element(CapState *cs, Buffer *buf) {
  size_t len;
  const char *name = ktable_element_name(cs->kt, capidx(cs->cap), &len);
  buf_addlstring(buf, name, len);
}

int json_Close(CapState *cs, Buffer *buf, int count, const char *start) {
  size_t e;
  UNUSED(count);
  if (isopencap(cs->cap)) return MATCH_CLOSE_ERROR;
  e = cs->cap->s - cs->s + 1;	/* 1-based end position */
  if (!isopencap(cs->cap-1)) buf_addstring(buf, "]");
  buf_addstring(buf,  END_LABEL);
  json_encode_pos(e, buf);
  if (capkind(cs->cap) == Ccloseconst) {
      buf_addstring(buf, DATA_LABEL);
      buf_addstring(buf, "\"");
      json_encode_ktable_element(cs, buf);
      buf_addstring(buf, "\"");
  } else {
    assert(start);
    buf_addstring(buf, DATA_LABEL);
    int err = addlstring_json(buf, start, cs->cap->s - start);
    if (err != MATCH_OK) return err;
  }
  buf_addstring(buf, "}");
  return MATCH_OK;
}

int json_Open(CapState *cs, Buffer *buf, int count) {
  size_t s;
  if (!acceptable_capture(capkind(cs->cap))) {
    printf("%s:%d: capkind is %d\n", __FILE__, __LINE__, capkind(cs->cap));
    return MATCH_OPEN_ERROR;
  }
  if (count) buf_addstring(buf, ",");
  buf_addstring(buf, TYPE_LABEL);
  json_encode_ktable_element(cs, buf);
  buf_addstring(buf, "\"");
  s = cs->cap->s - cs->s + 1;	/* 1-based start position */
  buf_addstring(buf, START_LABEL);
  json_encode_pos(s, buf);
  /* introduce subs array if needed */
  if (isopencap(cs->cap+1)) buf_addstring(buf, COMPONENT_LABEL);
  return MATCH_OK;
}
