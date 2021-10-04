/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  rpeg.h                                                                   */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2019.                                      */
/*  © Copyright IBM Corporation 2017, 2018.                                  */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */


#if !defined(rpeg_h)
#define rpeg_h

#include "lauxlib.h"
#include "lualib.h"
#include "rbuf.h"

typedef struct r_encoder_type {
  const char *name;
  int code;
} r_encoder_t;

/* Non-zero value => implemented in C
 * Zero => implemented in Lua
 */
#define ENCODE_DEBUG -1
#define ENCODE_JSON 1
#define ENCODE_LINE 2
#define ENCODE_BYTE 3

__attribute__((unused))
static const r_encoder_t r_encoders[] = { 
     {"json",   ENCODE_JSON},
     {"line",   ENCODE_LINE},
     {"byte",   ENCODE_BYTE},
     {"debug",  ENCODE_DEBUG},
     {NULL, 0}
};

int r_match_C (lua_State *L);

#endif
