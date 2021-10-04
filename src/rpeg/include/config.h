/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  config.h                                                                 */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#if !defined(config_h)
#define config_h

#ifndef DEBUG
#define DEBUG 0
#endif

#include <stdlib.h>		/* int32_t */
#include <limits.h>		/* USHRT_MAX */
#include <inttypes.h>		/* INT32_MAX */

/* Parms for call/backtrack stack in vm.c */
/* Typical usage with pattern all.things needs 12 backtrack stack frames. */
#define INIT_BACKTRACKSTACK	 21
#define MAX_BACKTRACK            USHRT_MAX

/* Capture list assembled by vm() grows and shrinks (when submatches fail) */
#define INIT_CAPLISTSIZE         1000
#define MAX_CAPLISTSIZE          INT32_MAX

/* Parms for capture nesting depth (stack used by caploop in walk_captures) */
#define INIT_CAPDEPTH            13
#define MAX_CAPDEPTH             USHRT_MAX 

/* Index into ktable must fit in 24-bits (unsigned), see rplx.h */
#define KTABLE_INDEX_T_MAX       16777215

/* 
 * Maximum number of rules in a grammar.
 * STACK ALLOCATED array of ints of this size in codegrammar() in lpcode.c
 */
#if !defined(MAXRULES)
#define MAXRULES                 9105     /* FUTURE: make this dynamically expandable? */
#endif

/* Whether or not statistics are kept */
#define RECORD_VMSTATS 1

typedef unsigned char byte;

#define UNUSED(x) (void)(x)

#endif
