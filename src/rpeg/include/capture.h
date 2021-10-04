/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  capture.h                                                                */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  © Copyright IBM Corporation 2017.                                        */
/*  Portions Copyright 2007, Lua.org & PUC-Rio (via lpeg)                    */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */


#if !defined(capture_h)
#define capture_h

#include "config.h"
#include "rplx.h"
#include "vm.h"
#include "buf.h"

#define acceptable_capture(kind) (((kind) == Crosiecap) || ((kind) == Crosieconst) || ((kind) == Cbackref))

/* FUTURE: 
   () Revise to expand range of representable numbers 
   () Use a faster itoa algorithm
*/

/* Signed 32-bit integers: from −2,147,483,648 to 2,147,483,647  */
#define MAXNUMBER2STR 16
#define INT_FMT "%d"
#define r_inttostring(s, i) (snprintf((char *)(s), (MAXNUMBER2STR), (INT_FMT), (i)))

int debug_Close(CapState *cs, Buffer *buf, int count, const char *start);
int debug_Open(CapState *cs, Buffer *buf, int count);

int byte_Close(CapState *cs, Buffer *buf, int count, const char *start);
int byte_Open(CapState *cs, Buffer *buf, int count);

int noop_Close(CapState *cs, Buffer *buf, int count, const char *start);
int noop_Open(CapState *cs, Buffer *buf, int count);

#endif
