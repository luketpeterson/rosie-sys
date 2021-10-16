/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  str.h                                                                    */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2021.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

/* 
   IMPORTANT: byte_ptr, rosie_string, and str definitions must be kept
   in sync with the same definitions in librosie.h.
*/

#if !defined(str_h)
#define str_h

/* Immutable pointer to immutable byte sequence */
typedef char const * const byte_ptr_const;

/* Mutable pointer to immutable byte sequence */
typedef char const * byte_ptr;

/* Immutable */
typedef struct rosie_string {
     uint32_t len;
     byte_ptr ptr;
} str;

typedef struct rosie_matchresult {
     str data;
     int leftover;
     int abend;
     int ttotal;
     int tmatch;
} match;

#endif






