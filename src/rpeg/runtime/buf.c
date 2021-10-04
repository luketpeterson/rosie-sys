/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  buf.c   Growable buffers to hold binary data                             */
/*                                                                           */
/*  © Copyright Jamie A. Jennings, 2018.                                     */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

/*
 * This is the usual binary data buffer that C code needs, tailored to
 * use by the Rosie Pattern Language project.
 *
 * A fresh buffer comes with a statically allocated data block.  When
 * that initial block overflows, a larger block is malloc'd and the
 * current data is copied there.  If the new dynamic block overflows,
 * it is realloc'd.
 *
 * Also supports wrapping an existing block of data to make it behave
 * like a buffer. A wrapped block will not be freed with buf_free().
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buf.h"

#ifndef BUFDEBUG
#define BUFDEBUG 0
#else
#define BUFDEBUG 1
#endif

#if BUFDEBUG

#define Announce_free(b) do {						\
    fprintf(stderr, "*** freeing buffer->data %p (capacity was %ld)\n", (void *)(b->data), b->capacity); \
  } while(0);

#define Announce_resize(b, temp) do {					\
    if (temp) fprintf(stderr, "*** resized buffer %p to new capacity %ld\n", (void *)b, newsize); \
    if (b->data != temp) fprintf(stderr, "*** buf->data changed from %p to %p\n", (void *)b->data, (void *)temp); \
  } while(0);

#define Announce_prepsize(b, additional) do {				\
  fprintf(stderr, "*** not enough space for buffer %p (%ld/%ld %s): open capacity is %ld, looking for %ld\n", \
	  (void *)b, b->n, b->capacity, (bufferisdynamic(b) ? "DYNAMIC" : "STATIC"), b->capacity - b->n, additional); \
  } while(0);

#else

#define Announce_free(b)
#define Announce_resize(b, temp)
#define Announce_prepsize(b, additional)

#endif 

/* ---------------------------------------------------------------------------------------- */

/* true when buffer's data has overflowed initb and is now allocated elswhere */
#define bufferisdynamic(B)	((B)->data != (B)->initb)

/* true when buffer is a "lite" buffer (a wrapper around existing immutable data) */
#define bufferislite(B)	        ((B)->initb == NULL)

int buf_info(Buffer *b) {
  int flags = 0;
  if (bufferislite(b)) flags = flags | BUF_IS_LITE;
  if (bufferisdynamic(b)) flags = flags | BUF_IS_DYNAMIC;
  return flags;
}

/* 
 * Dynamically allocate new storage, returning NULL if error.
 */
static Buffer *buf_resize (Buffer *b, size_t newsize) {
  char *temp;
  if (bufferislite(b)) return NULL;
  if (bufferisdynamic(b)) {
    temp = realloc(b->data, newsize);
  } else {
    /* not dynamic means that data is in initb (static allocation) */
    temp = malloc(newsize);
    /* copy original content, or as much as will fit if newsize is smaller */ 
    if (temp)
      memcpy(temp, b->initb, sizeof(char) * (newsize > b->n ? b->n : newsize));
  }
  Announce_resize(b, temp);
  if (temp == NULL && newsize > 0) {	     /* allocation error? */
    if bufferisdynamic(b) free(b->data);
    return NULL;
  }
  b->data = temp;
  b->capacity = newsize;
  return b;
}

Buffer *buf_new (size_t minimum_size) {
  Buffer *buf = (Buffer *)malloc(sizeof(Buffer));
  buf->initb = buf->initialbuff; /* the extra pointer to initialbuff enables r_newbuffer_wrap */
  buf->data = buf->initb;        /* intially, data storage is statically allocated in initb  */
  buf->n = 0;			 /* contents length is 0 */
  buf->capacity = INITIAL_BUFFER_SIZE;	 /* size of initb */
  if (minimum_size > INITIAL_BUFFER_SIZE)
    buf = buf_resize(buf, minimum_size);
  return buf;
}

/* We suppress the warning because buf_from_const produces an
   immutable buffer.  It cannot be extended, and we do not provide
   operations to modify the contents of a buffer (only to add to it).
*/
#pragma GCC diagnostic push  /* GCC 4.6 and up */
#pragma GCC diagnostic ignored "-Wcast-qual"

Buffer *buf_from_const (const char *data, size_t size) {
/* Removed earlier optimizations because they depended on undefined C behavior. */
  Buffer *buf = buf_new(0);
  buf->data = (char *) data; /* Without the pragma, this causes a warning. */
  buf->n = size;
  buf->capacity = size;
  buf->initb = NULL;         /* This marks a buffer that cannot be extended */
  return buf;
}
#pragma GCC diagnostic pop

/* Return a pointer to first unused byte, assuring at least
 * 'additional' bytes are available.
 */
char *buf_prepsize (Buffer *b, size_t additional) {
  if (bufferislite(b)) return NULL; /* immutable */
  if ((b->capacity - b->n) < additional) {
    /* double buffer size */
    size_t newsize = b->capacity * 2;
    /* still not big enough? increase buffer size just enough to fit 'additional' bytes */
    if (newsize - b->n < additional) newsize = b->n + additional;
    Announce_prepsize(b, additional);
    if (!buf_resize(b, newsize)) return NULL;
  }
  return &b->data[b->n];
}

void buf_reset (Buffer *b) {
  b->n = 0;
}

void buf_free (Buffer *b) {
  if (bufferisdynamic(b) && !bufferislite(b)) { 
    Announce_free(b);
    free(b->data);
  } 
}

Buffer *buf_addlstring (Buffer *b, const char *s, size_t len) {
  if (bufferislite(b)) return NULL; /* immutable */
  if (len > 0) {		    /* noop when 's' is an empty string */
    char *space = buf_prepsize(b, len * sizeof(char));
    if (!space) return NULL;
    memcpy(space, s, len * sizeof(char));
    b->n += len;
  }
  return b;
}

/* Indices follow Lua substring rules:
 *   1-based, with negative value counting from end of list;
 *   0 means default (start => 1, finish => last char);
 * Returns pointer into buffer's data, and sets len.
 */
char *buf_substring (Buffer *b, int j, int k, size_t *len) {
  /* defaults */
  if (j == 0) j = 1; 
  if (k == 0) k = b->n; 
  /* Rules of string.sub according to the Lua 5.3 reference */
  if (j < 0) j = b->n + j + 1;
  if (j < 1) j = 1;
  if (k < 0) k = b->n + k + 1;
  if (k > (int) b->n) k = b->n;
  if ((j > k) || (j > (int) b->n)) {
    *len = 0;
    return b->data;
  }
  *len = (size_t) k - j + 1;
  return b->data + j - 1;
}

static void int_to_string(int i, unsigned char str[]) {
  unsigned int iun = i;
  str[3] = (iun >> 24) & 0xFF;
  str[2] = (iun >> 16) & 0xFF;
  str[1] = (iun >> 8) & 0xFF;
  str[0] = iun & 0xFF;
}

Buffer *buf_addint (Buffer *b, int i) {
  unsigned char str[4];
  int_to_string(i, str);
  if (buf_addlstring(b, (const char *)str, 4)) return b;
  return NULL;
}

static int string_to_int(const char *sun) {
  int tmp = (unsigned char) *sun;
  tmp |= ((unsigned char) *(sun+1)) << 8;
  tmp |= ((unsigned char) *(sun+2)) << 16;
  tmp |= (unsigned int) ((unsigned char) *(sun+3)) << 24;
  return (int) tmp;
}

/* Unsafe: caller to ensure that read will not pass end of buffer */
int buf_peekint(const char *s) {
  return string_to_int(s);
}

/* Unsafe: caller to ensure that read will not pass end of buffer */
int buf_readint (const char **s) {
  int i = buf_peekint(*s);
  (*s) += 4;
  return i;
}

Buffer *buf_addshort (Buffer *b, short i) {
  char str[2];
  short iun = (short) i;
  str[1] = (iun >> 8) & 0xFF;
  str[0] = iun & 0xFF;
  if (buf_addlstring(b, str, 2))
    return b;
  return NULL;
}

/* Unsafe: caller to ensure that read will not pass end of buffer */
short buf_peekshort (const char *sun) {
  short i = *sun | (((unsigned char) *(sun+1)) <<8 );
  return i;
}

/* Unsafe: caller to ensure that read will not pass end of buffer */
short buf_readshort (const char **s) {
  short i = buf_peekshort(*s);
  (*s) += 2;
  return i;
}

/* ---------------------------------------------------------------------------------------- */

int buf_writelen(FILE *file, Buffer *b) {
  unsigned char str[4];
  int_to_string((int) b->n, str);
  size_t items = fwrite((void *) str, 4, 1, file);
  if (items != 1) return BUF_ERR_WRITE;
  return BUF_OK;
}

int buf_write(FILE *file, Buffer *b) {
  if (b->n==0) return 0;
  size_t items = fwrite((void *) b->data, b->n, 1, file);
  if (items != 1) return BUF_ERR_WRITE;
  return BUF_OK;
}

int buf_readlen(FILE *file, size_t *len) {
  char str[4];
  size_t items = fread((void *) str, 4, 1, file);
  if (items != 1) return BUF_ERR_READ;
  *len = (size_t) string_to_int(str);
  return BUF_OK;
}

Buffer *buf_read(FILE *file, size_t len) {
  Buffer *b = buf_new(len);
  if (len == 0) return b;
  size_t items = fread((void *) b->data, len, 1, file);
  if (items != 1) {
    buf_free(b);
    free(b);
    return NULL;
  }
  b->n = len;
  return b;
}

