/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  rbuf.h   (now uses buf.c)                                                */
/*                                                                           */
/*  © Copyright Jamie A. Jennings, 2018.                                     */
/*  © Copyright IBM Corporation 2017.                                        */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#if !defined(rbuf_h)
#define rbuf_h

#include "buf.h"

#define ROSIE_BUFFER "ROSIE_BUFFER"

typedef Buffer *RBuffer;

int r_lua_newbuffer (lua_State *L);
int r_lua_buffreset (lua_State *L, int pos);
int r_lua_getdata (lua_State *L);
int r_lua_add (lua_State *L);
int r_lua_writedata(lua_State *L);

RBuffer *r_newbuffer (lua_State *L);
RBuffer *r_newbuffer_wrap (lua_State *L, char *data, size_t len);

/* These functions  DO NOT use the stack */
char *r_prepbuffsize (lua_State *L, RBuffer *buf, size_t sz);
void r_addlstring (lua_State *L, RBuffer *buf, const char *s, size_t l);
void r_addint (lua_State *L, RBuffer *buf, int i);
int r_readint(const char **s);
int r_peekint(const char *s);
void r_addshort (lua_State *L, RBuffer *buf, short i);
int r_readshort(const char **s);
     
#define r_addstring(L, rbuf, s) (r_addlstring)((L), (rbuf), (s), strlen(s))
#define r_addchar(L, rbuf, c) (r_addlstring)((L), (rbuf), &(c), sizeof(char))
#define r_addlstring_UNSAFE(L, rb, s, l)  buf_addlstring_UNSAFE(*rb, s, l)
#define r_addchar_UNSAFE(L, rbuf, c) r_addlstring_UNSAFE((L), (rbuf), &(c), sizeof(char))


#endif
