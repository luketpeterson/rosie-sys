/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  vm.h                                                                     */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  Portions Copyright 2007, Lua.org & PUC-Rio (via lpeg)                    */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#if !defined(vm_h)
#define vm_h

#include "config.h"
#include "rplx.h"
#include "buf.h"
#include "ktable.h"
#include "rpeg.h"
#include "str.h"

typedef enum MatchErr {
  /* Match vm: */
  MATCH_OK,
  MATCH_HALT,
  MATCH_ERR_STACK,
  MATCH_ERR_BADINST,
  MATCH_ERR_CAP,
  MATCH_ERR_INPUT_LEN,
  MATCH_ERR_STARTPOS,
  MATCH_ERR_ENDPOS,
  MATCH_ERR_OUTPUT_MEM,
  MATCH_ERR_NULL_PATTERN,
  MATCH_ERR_NULL_INPUT,
  MATCH_ERR_NULL_OUTPUT,
  MATCH_ERR_NULL_MATCHRESULT,
  /* Capture processing: */
  MATCH_OPEN_ERROR,
  MATCH_CLOSE_ERROR,
  MATCH_FULLCAP_ERROR,
  MATCH_STACK_ERROR,
  MATCH_INVALID_ENCODER,
  /* General: */
  MATCH_IMPL_ERROR,
  MATCH_OUT_OF_MEM,  
} MatchErr;

static const char *MATCH_MESSAGES[] __attribute__ ((unused)) = {
  /* Match vm: */
  "ok",
  "halt/abend",
  "backtracking stack limit exceeded",
  "invalid instruction for matching vm",
  "capture limit exceeded (or insufficient memory for captures)",
  "input too large",
  "start position beyond end of input",
  "end position beyond end of input",
  "insufficient memory for match data",
  /* Capture processing: */
  "open capture error in rosie match",
  "close capture error in rosie match",
  "full capture error in rosie match",
  "capture stack overflow in rosie match",
  "invalid encoder in rosie match",
  "null pattern argument",
  "null input argument",
  "null output buffer argument",
  "null match result argument",
  /* General: */
  "implementation error (bug)",
  "out of memory",
};

typedef struct Stats {
  unsigned int total_time;
  unsigned int match_time;
  unsigned int insts;	   /* number of vm instructions executed */
  unsigned int backtrack;  /* max len of backtrack stack used by vm */
  unsigned int caplist;    /* max len of capture list used by vm */
  unsigned int capdepth;   /* max len of capture stack used by walk_captures */
} Stats;

typedef struct Match {
  short matched;	  /* boolean; if 0, then ignore data field */
  short abend;		  /* boolean; meaningful only if matched==1 */
  unsigned int leftover;  /* leftover characters, in bytes */
  Buffer *data;
} Match;

/* Kinds of captures 
 *
 * Stored in 'offset', which is 32 bits (way more than we ever need).
 * We will use only the low 8 bits, assume a max of 256 capture types,
 * and reserve bit 8 to indicate a closing capture.
 */
typedef enum CapKind { 
  Crosiecap, Crosieconst, Cbackref,  
  Cclose = 0x80,		/* high bit set */
  Cfinal, Ccloseconst,
} CapKind; 

/* N.B. The CAPTURE_NAME macro needs to be changed when new capture types are added. */
#define CAPTURE_NAME(c) (((c) & 0x80) ? CLOSE_CAPTURE_NAMES[(c) & 0x3] : OPEN_CAPTURE_NAMES[(c & 0x3)])
static const char *const OPEN_CAPTURE_NAMES[] = {
  "RosieCap", "RosieConst", "Backref",
};
static const char *const CLOSE_CAPTURE_NAMES[] = {
  "Close",
  "Final", "CloseConst",
};

/* Capture 'kind' is 8 bits (unsigned), and 'idx' 24 bits (unsigned).  See rplx.h. */
#define capidx(cap) ((cap)->c.aux)
#define setcapidx(cap, newidx) (cap)->c.aux = (newidx)
#define capkind(cap) ((cap)->c.code) 
#define setcapkind(cap, kind) (cap)->c.code = (kind)

typedef struct Capture {
  byte_ptr s;	    /* subject position */
  CodeAux c;	    /* .c.code is 'kind' and .c.aux is ktable index */
} Capture;

typedef struct CapState {
  Capture *cap;			/* current capture */
  Capture *ocap;		/* (original) capture list */
  byte_ptr s;			/* original string */
  Ktable *kt;			/* ktable */
} CapState;

#define testchar(st,c)	(((int)(st)[((c) >> 3)] & (1 << ((c) & 7))))

#define isopencap(cap)	(!(capkind((cap)) & 0x80)) /* test high bit */
#define isfinalcap(cap)	(capkind(cap) == Cfinal)
#define isclosecap(cap)	(capkind(cap) == Cclose)

typedef struct {  
  int (*Open)(CapState *cs, Buffer *buf, int count);
  int (*Close)(CapState *cs, Buffer *buf, int count, byte_ptr start);
} Encoder;
 
int sizei (const Instruction *i);

int vm_match2 (/* inputs: */
	       Chunk *chunk,
	       struct rosie_string *input, uint32_t startpos, uint32_t endpos,
	       Encoder encode,
	       uint8_t collect_times,
	       Buffer *output,
	       /* output: */
	       struct rosie_matchresult *match);
#endif

