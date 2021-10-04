/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  rplx.h  RPLX pattern matching code data structure                        */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  Portions Copyright 2007, Lua.org & PUC-Rio (via lpeg)                    */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#if !defined rplx_h
#define rplx_h

#include <limits.h> 
#include <inttypes.h>
#include "config.h"
#include "ktable.h"

#define BITSPERCHAR		8
#define CHARSETSIZE		((UCHAR_MAX/BITSPERCHAR) + 1)
/* size (in elements) for an instruction plus extra l bytes */
#define instsize(l)  (((l) + sizeof(Instruction) - 1)/sizeof(Instruction) + 1)
/* size (in elements) for a ISet instruction */
#define CHARSETINSTSIZE		instsize(CHARSETSIZE)

typedef struct Charset {
  byte cs[CHARSETSIZE];
} Charset;

#define opcode(pc) ((pc)->i.code)
#define setopcode(pc, op) ((pc)->i.code) = (op)

#define addr(pc) ((pc+1)->offset)
#define setaddr(pc, addr) (pc+1)->offset = (addr)

#define index(pc) ((pc)->i.aux)
#define setindex(pc, idx) (pc)->i.aux = ((idx) & 0xFFFFFF)
#define ichar(pc) ((pc)->i.aux & 0xFF)
#define setichar(pc, c) (pc)->i.aux = ((pc)->i.aux & 0xFFFF00) | (c & 0xFF)

typedef struct CodeAux {
  uint8_t code;		   /* opcode */
  unsigned int aux : 24;   /* [0 .. 16,777,215] ktable index, or chars */
} CodeAux;

typedef union Instruction {
  CodeAux i;
  int32_t offset;	  /* follows an opcode that needs one */
  uint8_t buff[1];        /* char set following an opcode that needs one */
} Instruction;


// Most common instructions (totaling 98%):
//   ITestSet offset, charset
//   IAny
//   IPartialCommit offset

// Reference:
//  unsigned 16-bit (short) 65,536
//  signed 24-bit        8,388,607  
//  unsigned 24-bit     16,777,216
//  signed int32     2,147,483,647  (2Gb)
//  uint32_t         4,294,967,296  (4Gb)

// TESTS show that accessing the 24-bit field as a signed or unsigned
// int takes time indistinguishable from accessing a 32-bit int value.
// Storing the 24-bit value takes significantly longer (> 2x) than
// storing a 32-bit int, but we only store the ktable index when we
// are compiling, not at runtime in the vm.

/* Desirable:
 *   Byte-addressable input data up to 4Gb (affects runtime & output encoding, not instruction coding)
 *   Ktable as large as 8M elements, at least
 *   Instructions in compilation unit at least 1M (= 20 bits, ==> 21 bits offset)
 *   Room for many new instructions, particularly multi-char ones
 *   Room for more capture kinds, at least 6 bits' worth
 */

typedef enum Opcode {
  /* Bare instruction ------------------------------------------------------------ */
  IGiveup = 0,               /* for internal use by the vm */
  IAny,			     /* if no char, fail */
  IRet,			     /* return from a rule */
  IEnd,			     /* end of pattern */
  IHalt,		     /* abnormal end (abort the match) */
  IFailTwice,		     /* pop one choice from stack and then fail */
  IFail,                     /* pop stack (pushed on choice), jump to saved offset */
  ICloseCapture,	     /* push close capture marker onto cap list */
  /* Aux ------------------------------------------------------------------------- */
  IBehind,                   /* walk back 'aux' characters (fail if not possible) */
  IBackref,		     /* match same data as prior capture (key is 'aux')*/
  IChar,                     /* if char != aux, fail */
  ICloseConstCapture,        /* push const close capture and index onto cap list */
  /* Charset --------------------------------------------------------------------- */
  ISet,		             /* if char not in buff, fail */
  ISpan,		     /* read a span of chars in buff */
  /* Offset ---------------------------------------------------------------------- */
  IPartialCommit,            /* update top choice to current position and jump */
  ITestAny,                  /* if no chars left, jump to 'offset' */
  IJmp,	                     /* jump to 'offset' */
  ICall,                     /* call rule at 'offset' */
  IOpenCall,                 /* call rule number 'key' (must be closed to a ICall) */
  IChoice,                   /* stack a choice; next fail will jump to 'offset' */
  ICommit,                   /* pop choice and jump to 'offset' */
  IBackCommit,		     /* "fails" but jumps to its own 'offset' */
  /* Offset and aux -------------------------------------------------------------- */
  IOpenCapture,		     /* start a capture (kind is 'aux', key is 'offset') */
  ITestChar,                 /* if char != aux, jump to 'offset' */
  /* Offset and charset ---------------------------------------------------------- */
  ITestSet,                  /* if char not in buff, jump to 'offset' */
  /* Offset and aux and charset -------------------------------------------------- */
  /* none (so far) */
} Opcode;

#define OPCODE_NAME(code) (OPCODE_NAMES[code])
static const char *const OPCODE_NAMES[] = {
  "giveup",
  "any",
  "ret",
  "end",
  "halt",
  "failtwice",
  "fail",
  "closecapture",
  "behind",
  "backref",
  "char",
  "closeconstcapture",
  "set",
  "span",
  "partialcommit",
  "testany",
  "jmp",
  "call",
  "opencall",
  "choice",
  "commit",
  "backcommit",
  "opencapture",
  "testchar",
  "testset",
};

typedef struct Chunk {
  size_t codesize;	        /* number of Instructions */
  Instruction *code;		/* code vector */
  Ktable *ktable;		/* capture table */
  unsigned short rpl_major;     /* rpl major version */
  unsigned short rpl_minor;     /* rpl minor version */
  char *filename;		/* origin (could be NULL) */
  unsigned short file_version;	/* file format version */
} Chunk;


void rplx_free(Chunk *c);


#endif
