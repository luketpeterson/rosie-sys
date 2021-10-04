/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  rplx.c                                                                   */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */


#include <stdlib.h>

#include "config.h"
#include "rplx.h"

void rplx_free(Chunk *c) {
  ktable_free(c->ktable);
  free(c->code);
  if (c->filename) free(c->filename);
}



