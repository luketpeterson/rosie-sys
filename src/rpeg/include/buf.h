/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  buf.h                                                                    */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#if !defined(buf_h)
#define buf_h

#include <stdio.h>

#define INITIAL_BUFFER_SIZE (8192 * sizeof(char))	  /* should experiment with different values */

typedef enum BufErr {
  BUF_OK,
  BUF_ERR_WRITE, BUF_ERR_READ,
} BufErr;

/* Our data, which will be freed by a call to buf_free(). */
typedef struct Buffer {
  char   *data;
  size_t capacity;
  size_t n;			/* number of bytes in use */
  char   *initb;
  char   initialbuff[INITIAL_BUFFER_SIZE];
} Buffer;

/* A wrapper around some else's immutable data.  Calls to modify this
 * data are not provided (duh).  Their data will NOT be freed by a
 * call to buf_free().
 */
typedef struct BufferLiteConst {
  const char *data;
  size_t     capacity;
  size_t     n;			/* number of bytes in use */
  const char *initb;		/* no initial buffer */
} BufferLiteConst;


/* For understanding the bits in the buf_info return value: */
#define BUF_IS_LITE 1
#define BUF_IS_DYNAMIC 2

int buf_info(Buffer *b);
Buffer *buf_new(size_t minimum_size);
Buffer *buf_from_const(const char *data, size_t size);

char *buf_prepsize(Buffer *b, size_t additional);
void buf_reset(Buffer *b);
void buf_free(Buffer *b);

Buffer *buf_addlstring (Buffer *b, const char *s, size_t len);
char *buf_substring (Buffer *b, int j, int k, size_t *len);
Buffer *buf_addint(Buffer *b, int i);
int  buf_peekint(const char *s);
int  buf_readint (const char **s);
Buffer *buf_addshort(Buffer *b, short i);
short buf_peekshort (const char *s);
short buf_readshort (const char **s);

#define buf_addstring(b, s) (buf_addlstring)((b), (s), strlen(s))
#define buf_addchar(b, c) (buf_addlstring)((b), &(c), sizeof(char))

#define buf_addlstring_UNSAFE(b, s, l) { memcpy(&((b)->data[(b)->n]), (s), (l) * sizeof(char)); (b)->n += (l); }
#define buf_addchar_UNSAFE(b, c) buf_addlstring_UNSAFE((b), &(c), sizeof(char))

int buf_writelen(FILE *file, Buffer *b);
int buf_write(FILE *file, Buffer *b);
int buf_readlen(FILE *file, size_t *len);
Buffer *buf_read(FILE *file, size_t len);

#endif
