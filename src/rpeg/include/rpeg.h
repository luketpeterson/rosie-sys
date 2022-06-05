/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  rpeg.h  Bridge between lptree/lpcap/vm and librosie                      */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2019, 2021.                                */
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

/* Code:
 *   Non-zero value => implemented in C
 *   Zero => implemented in Lua
 * The codes below must match what it is in common.lua.
 */
#define ENCODE_JSON 1
#define ENCODE_LINE 2
#define ENCODE_BYTE 3
#define ENCODE_DEBUG 4
#define ENCODE_STATUS 5

/* These codes are returned in the length field of matchresult->data
 * (str) whose ptr is NULL as a cheap way to give the caller an
 * explanation when an error occurred.  Zero is reserved for the "no
 * match" situation (not an error, just an indication that a match did
 * not succeed).
 */
#define NO_MATCH           0   //-- For 'status' output encoder
#define MATCH_WITHOUT_DATA 1   //-- For 'status' output encoder
#define ERR_NO_ENCODER     2   //-- Or "no trace style"
#define ERR_NO_FILE        3   //-- No such file or directory
#define ERR_NO_PATTERN     4   //-- Not a valid rplx 
#define ERR_BAD_STARTPOS   5   // start position out of range
#define ERR_BAD_ENDPOS     6   // end position out of range
#define ERR_INTERNAL       7   // bug in implementation (prob. encoder)

__attribute__((unused))
static const r_encoder_t r_encoders[] = { 
     {"byte",   ENCODE_BYTE},
     {"status", ENCODE_STATUS},
     {"json",   ENCODE_JSON},
     {"line",   ENCODE_LINE},
     {"debug",  ENCODE_DEBUG},
     {NULL, 0}
};

int r_match_C (lua_State *L);
void *extract_pattern (lua_State *L, int idx);

/* Forward declarations */
struct rosie_string;
struct rosie_matchresult;

int r_match_C2 (void *pattern_as_void_ptr,
		struct rosie_string *input, uint32_t startpos, uint32_t endpos,
		uint8_t etype, uint8_t collect_times,
		Buffer *output, struct rosie_matchresult *match);

#endif
