/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  ktable.h                                                                 */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#if !defined(ktable_h)
#define ktable_h

typedef struct Ktable_element {
  int32_t start;		/* index into block */
  int32_t len;			/* length of string in block */
  int32_t entrypoint;		/* index into code vector, or -1 for none */
} Ktable_element;

typedef struct Ktable {
  char *block;			/* storage for null-terminated strings */
  size_t blocksize;		/* size of storage block */
  int32_t blocknext;		/* next available char is block[blocknext] */
  Ktable_element *elements;
  int32_t size;			/* capacity of elements array */
  int32_t next;			/* next available ktable entry is elements[next] */
} Ktable;

/* Parameters that can be tuned for performance:
 *
 * The average element length is a guess, and a good guess will
 * optimize storage allocation.
 */
#define KTABLE_INIT_SIZE 64 
#define KTABLE_MAX_SIZE KTABLE_INDEX_T_MAX
#define KTABLE_AVG_ELEMENT_LEN 34 
#define KTABLE_MAX_ELEMENT_LEN 1024

typedef enum KtableErr {
  KTABLE_OK,
  KTABLE_ERR_MEM, KTABLE_ERR_SIZE, KTABLE_ERR_NULL,
} KtableErr;

static const char __attribute__ ((unused)) *KTABLE_MESSAGES[] = {
  "OK",
  "Out of memory",
  "Too many captures",
  "Null ktable",
};

#define ktable_messages ((int) ((sizeof KTABLE_MESSAGES) / sizeof (const char *)))

#define blockspace(kt) ( (size_t) (kt)->blocksize - (kt)->blocknext )
#define elementlen(e) ( (e)->len )


Ktable *ktable_new(int initial_size, size_t initial_blocksize);
void ktable_free(Ktable *kt);
int ktable_concat(Ktable *kt1, Ktable *kt2, int *n);
int ktable_add(Ktable *kt, const char *element, size_t len);
int ktable_len(Ktable *kt);
Ktable_element *ktable_element(Ktable *kt, int i);
const char *ktable_element_name(Ktable *kt, int i, size_t *len);
Ktable_element *ktable_sorted_index(Ktable *kt);
Ktable *ktable_compact(Ktable *orig);
int ktable_compact_search(Ktable *kt, const char *target, size_t len);
int ktable_name_cmp(const char *a, size_t alen, const char *b, size_t blen);
int ktable_entry_name_compare(Ktable *kt, const Ktable_element *k1, const Ktable_element *k2);
  
/* debugging */
void ktable_dump(Ktable *kt);

#endif
