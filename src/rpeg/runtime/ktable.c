/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  ktable.c                                                                 */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

/* TODO: ktable holds constant captures (Crosieconst) but we want
   those to be unicode, and ktable cannot hold unicode because it is
   implemented using null-terminated strings.  In future, even rosie
   pattern names could be unicode, and that is what ktable was meant
   for.  So we should change the impl to store string length, too.

   ALSO: For ahead-of-time compilation (specifically, to produce
   library files), we need an index of entry points into a code
   vector. Since the entry points are pattern names, all of which are
   in the ktable, we will enhance the ktable to store an unsigned
   32-bit index into the instruction vector.  That index will be used
   by 'call' instructions.

   N.B. It's possible for two distinct captures to have the same
   capture name, and we compact the ktable to remove duplicates. It
   should NOT be possible for two entry points in the same namespace
   to have the same capture name, so we can continue to compact the
   ktable.  When consolidating multiple entries that have the same
   name, we should observe that only one of them has an entry point
   defined. 

   New capture table format:
   'size' is the number of entries;
   'block' is a byte array that holds the capture names;
   'element' is an array of structures (see below);
   Each element contains:
   'start' is the first character of the capture name in 'block';
   'len' is the number of bytes in the capture name;
   'entrypoint' is an index into the code vector IFF this name is an entry point;

   Ktable indexing begins at 1, which reserves the 0th element for RPL
   library use.  The first element (at index 0) points to the default
   prefix name for the library, stored in 'block'.  When there is not
   a prefix (such as for a top-level namespace), the length field will
   be 0.
   
*/


/*
 * Capture table
 *
 * In lpeg, this is a Lua table, used as an array, with values of any
 * type.  In Rosie, the value type is always string.  In order to have
 * a Rosie Pattern Matching engine that can be independent of Lua, we
 * provide here a stand-alone ktable implementation.
 * 
 * Operations 
 *
 * new, free, element, len, concat
 *
 */

/*
 * 'block' holds consecutive null-terminated strings;
 * 'block[elements[i]]' is the first char of the element i;
 * 'blocknext' is the offset into block of the first free (unused) character;
 * 'element[next]' is the first open slot, iff len <= size;
 * 'size' is the number of slots in the element array, size > 0;
 *
 *  NOTE: indexes into Ktable are 1-based
 */

#if defined(__GNUC__)
#define _GNU_SOURCE
#endif

#if defined(__linux__)
#define qsort_r_common(base, n, sz, context, compare) qsort_r((base), (n), (sz), (compare), (context))
#else
#define qsort_r_common qsort_r
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "ktable.h"

#if !defined(KTABLE_DEBUG)
#define KTABLE_DEBUG 0
#endif 

#define LOG(msg) \
     do { if (KTABLE_DEBUG) fprintf(stderr, "%s:%d:%s(): %s", __FILE__, \
			       __LINE__, __func__, msg); \
	  fflush(stderr); } while (0)

#define LOGf(fmt, ...) \
     do { if (KTABLE_DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
			       __LINE__, __func__, __VA_ARGS__); \
	  fflush(stderr); } while (0)

Ktable *ktable_new(int initial_size, size_t initial_blocksize) {
  Ktable *kt = malloc(sizeof(Ktable));
  if (!kt) return NULL;
  kt->size = 1 + ((initial_size > 0) ? initial_size : KTABLE_INIT_SIZE);
  kt->elements = calloc(kt->size, sizeof(Ktable_element));
  if (!kt->elements) goto fail_kt;
  kt->next = 1;
  kt->blocksize = ((initial_blocksize > 0) ? ((size_t) initial_blocksize) : ((size_t) kt->size * KTABLE_AVG_ELEMENT_LEN));
  kt->block = (char *) malloc(kt->blocksize);
  if (!kt->block) goto fail_kt_elements;
  kt->blocknext = 0;
  return kt;

fail_kt_elements:
  free(kt->elements);
fail_kt:
  free(kt);
  return NULL;
}

void ktable_free(Ktable *kt) {
  LOG("in ktable_free\n");
  if (kt) {
    if (kt->block) free(kt->block);
    if (kt->elements) free(kt->elements);
    free(kt);
  }
}

int ktable_len (Ktable *kt) {
  if (!kt) {
    LOG("null kt\n");
    return 0;
  }
  return kt->next - 1;
}

/* 1-based indexing */
Ktable_element *ktable_element (Ktable *kt, int i) {
  if (!kt) {
    LOG("null kt\n");
    return NULL;
  }
  return ((i < 1) || (i >= kt->next)) ? NULL : &(kt->elements[i]);
}

const char *ktable_element_name (Ktable *kt, int i, size_t *len) {
  Ktable_element *el = ktable_element(kt, i);
  if (!el) return NULL;
  *len = (size_t) el->len;
  return &(kt->block[el->start]);
}

/* 
 * Concatentate the contents of table 'kt1' into table 'kt2'.
 * Return the original length of table 'kt2' (or 0, if no
 * element was added, as there is no need to correct any index).
 */

int ktable_concat(Ktable *kt1, Ktable *kt2, int *ret_n2) {
  if (!kt1 || !kt2) {
    if (!kt1) LOG("null k1\n");
    else {
      LOG("null k2\n");
    }
    return KTABLE_ERR_NULL;
  }
  int n1 = ktable_len(kt1);
  assert( n1 >= 0 );
  int n2 = ktable_len(kt2);
  assert( n2 >= 0 );
  LOGf("kt1 len %d, kt2 len %d\n", n1, n2);
  int lastidx;
  size_t len = 0;
  const char *ptr;
  if ((n1 + n2) > KTABLE_MAX_SIZE) return KTABLE_ERR_SIZE;
  if (n1 == 0) {
    /* kt1 is empty, and no indexes to correct */
    *ret_n2 = 0;
    return KTABLE_OK;
  }
  for (int i = 1; i <= n1; i++) {
    ptr = ktable_element_name(kt1, i, &len);
    lastidx = ktable_add(kt2, ptr, len);
    if (lastidx < 0) return KTABLE_ERR_MEM;
    assert( lastidx == (n2 + i) );
  }
  assert( ktable_len(kt2) == (n1 + n2) );
  assert( n2 >= 0 );
  *ret_n2 = n2;
  return KTABLE_OK;
}
  
static int extend_ktable_elements(Ktable *kt) {
  if (!kt) {
    LOG("null kt\n");
    return KTABLE_ERR_NULL;
  }
  size_t newsize = 2 * sizeof(Ktable_element) * kt->size;
  void *temp = realloc(kt->elements, newsize);
  if (!temp) return KTABLE_ERR_MEM;
  kt->elements = temp;
  kt->size = 2 * kt->size;
  LOGf("extending element array to %d slots (%d usable)\n", kt->size, kt->size - 1);
  return KTABLE_OK;
}

static int extend_ktable_block(Ktable *kt, size_t add_at_least) {
  if (!kt) {
    LOG("null kt\n");
    return KTABLE_ERR_NULL;
  }
  size_t newsize = 2 * kt->blocksize;
  if (add_at_least > kt->blocksize) newsize += add_at_least;
  void *temp = realloc(kt->block, newsize);
  if (!temp) return KTABLE_ERR_MEM;
  kt->block = temp;
  kt->blocksize = newsize;
  LOGf("extending block to %zu bytes\n", kt->blocksize);
  return KTABLE_OK;
}

/* 
 * Return index of new element (1-based).  If element's string value
 * is too long, it will be truncated.  Length limit should be checked
 * before we get here.
 */
int ktable_add(Ktable *kt, const char *element, size_t len) {
  if (!kt) {
    LOG("null kt\n");
    return KTABLE_ERR_NULL;
  }
  assert( element != NULL );
  assert( kt->next > 0 );

  if (kt->next == kt->size)
    if (extend_ktable_elements(kt) < 0) return KTABLE_ERR_MEM;
  if (blockspace(kt) < len)
    if (extend_ktable_block(kt, len) < 0) return KTABLE_ERR_MEM;
  assert( kt->next < kt->size );
  assert( blockspace(kt) >= len );

  memcpy(kt->block + kt->blocknext, element, len);
  
  Ktable_element *el = &(kt->elements[kt->next]);
  el->start = kt->blocknext;
  el->len = (int32_t) len;
  el->entrypoint = 0;
  kt->blocknext += len;
  kt->next++;
  LOGf("new element '%.*s' added in position %d/%d\n", (int) len, element, kt->next - 1, kt->size - 1);
  return kt->next - 1;
}

inline int ktable_name_cmp(const char *a, size_t alen, const char *b, size_t blen) {
  int a_shorter = (alen < blen);
  int res = memcmp(a, b, (a_shorter ? alen : blen));
  if (res == 0) {
    if (alen == blen) return 0;
    else return a_shorter ? -1 : 1;
  }
  return res;
}

inline int ktable_entry_name_compare(Ktable *kt, const Ktable_element *k1, const Ktable_element *k2) {
  const int start1 = ((const Ktable_element *) k1)->start;
  const int start2 = ((const Ktable_element *) k2)->start;
  const int len1 = ((const Ktable_element *) k1)->len;
  const int len2 = ((const Ktable_element *) k2)->len;
  return ktable_name_cmp(&(kt->block[start1]), len1, &(kt->block[start2]), len2);
}

#if defined(__linux__)
static inline int ktable_entry_cmp_internal(const void *k1, const void *k2, void *parm)
#else
static inline int ktable_entry_cmp_internal(void *parm, const void *k1, const void *k2)
#endif
{
  return ktable_entry_name_compare((Ktable *) parm, (const Ktable_element *) k1, (const Ktable_element *) k2);
}

/* Create a new index to an existing ktable, where the elements are in
   sorted order.  The new index will need to be freed by the caller.
*/
Ktable_element *ktable_sorted_index(Ktable *kt) {
  size_t sz = (ktable_len(kt)+1) * sizeof(Ktable_element);
  Ktable_element *elements = (Ktable_element *)malloc(sz);
  memcpy(&(elements[0]), &(kt->elements[0]), sz);
  qsort_r_common(&elements[1], (size_t) ktable_len(kt), sizeof(Ktable_element),
		 (void *)kt, &ktable_entry_cmp_internal);
#if 0
  /* DEBUGGING */
  printf("ktable_len is %d\n", ktable_len(kt)); 
  for(int i=1; i<=ktable_len(kt); i++) { 
    size_t len = elements[i].len; 
    const char *name = &(kt->block[elements[i].start]); 
    printf("%4d. %.*s\n", i, (int) len, name); 
  } 
#endif
  return elements;
}

/* Create a new ktable from orig, where the new one has no duplicates
   and whose elements are in sorted order.  The orig table is not
   modified or freed.
*/
Ktable *ktable_compact(Ktable *orig) {
  assert( orig != NULL );
  int orig_len = ktable_len(orig);
  Ktable_element *index = ktable_sorted_index(orig);
  Ktable *new = ktable_new(0, 0); /* use defaults */
  if (orig_len == 0) goto done;
  size_t prev_len = index[1].len;
  const char *prev = &(orig->block[index[1].start]);
  ktable_add(new, prev, prev_len);
  //printf("First entry in new ktable: %.*s\n", (int) prev_len, prev);
  for (int i = 2; i <= orig_len; i++) {
    /* TODO:
     - Create a ktable_add_with_entrypoint() to use in this loop below.
    */
    size_t len = index[i].len;
    const char *name = &(orig->block[index[i].start]);
    if (ktable_name_cmp(prev, prev_len, name, len) != 0) {
      ktable_add(new, name, len);
      //printf("Next entry in new ktable: %.*s\n", (int) len, name);
      prev = name;
      prev_len = len;
    }
  }
 done:
  free(index);
  return new;
}

/* Given a COMPACT, SORTED ktable, search for an element matching
   'target', returning its index or 0 (if not found).
*/
int ktable_compact_search(Ktable *kt, const char *target, size_t target_len) {
  // bsearch does not have a bsearch_r variant in C99?  Argh.
  // int *result = bsearch(target, &kt->elements[0], (size_t) ktable_len(kt), sizeof(int), &ktable_entry_cmp);
  int len = ktable_len(kt);
  if (len == 0) return 0;
  int i = 1;
  int j = len;
  int mid, test;
  const char *ptr;
  size_t entry_len;
  while (1) {
    mid = i + ((j - i) / 2);
    ptr = &(kt->block[kt->elements[mid].start]);
    entry_len = kt->elements[mid].len;
    test = ktable_name_cmp(target, target_len, ptr, entry_len);
    if (test == 0) return mid;
    if (i == j) return 0;	/* not found */
    if (test < 0) j = mid - 1;
    else { i = mid + 1; }
  }
}

void ktable_dump(Ktable *kt) {
  size_t len = 0;
  const char *ptr;
  if (!kt) printf("Ktable pointer is NULL\n");
  else {
    printf("Ktable: size = %d (%d used), blocksize = %lu (%lu used)\n",
	   (kt->size - 1), (kt->next - 1), kt->blocksize, kt->blocksize - blockspace(kt));
    printf("contents: ");
    for (int i = 1; i < kt->next; i++) {
      ptr = ktable_element_name(kt, i, &len);
      printf("%d: %.*s ", i, (int) len, ptr);
    }
    printf("\n");
  }
  fflush(stdout);
}


