/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  stack.c                                                                  */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

/* 
 * Here, a stack is an array of structs (not pointers to structs).
 * Supported operations:
 *   akdlakdlaksd
 *   akdlakdalk
 * 
 * Initially, a statically allocated array is used.  In addition to
 * this storage space, a stack contains:
 * 
 *   base -- pointer to first entry in the array
 *   next -- pointer to next free entry in the array (see 'limit')
 *   limit -- pointer to one struct AFTER array
 * 
 * When next == limit, the stack is full, and must be expanded before
 * another struct can be pushed.  Expansion means dynamically
 * allocating a new (bigger) array, and copying the contents of the
 * current array into the new space.  If the old space was also
 * dynamically allocated it is then freed.  If the old space was the
 * initial (statically allocated) space, it is simply no longer used.
 *
 * Because the values above are actual pointers, and not integer
 * indices into the array, the user of a stack MUST NOT store the
 * values of base, next, or limit.  These pointers will all CHANGE when
 * the stack is expanded.
 *
 * In order to measure how much space is actually used during the
 * lifetime of the stack, for tuning purposes, the stack takes a
 * configuration option as follows:
 * 
 *   STACK_TRACK_NOTHING
 *   STACK_TRACK_MAXTOP   max number of structs actually used
 *
 * To know the maximum capacity of the stack at any time (such as
 * before freeing it, to see how much it had expanded), use the
 * capacity API.
 *
 */

#define TOP(stack) ((stack).next - 1)
#define PEEK(stack, offset) ((stack).next + offset - 1)
#define STACK_CAPACITY(stack) ((stack).limit - (stack).base)
#define STACK_SIZE(stack) ((stack).next - (stack).base)

/* ----------------------------------------------------------------------------- */

#ifndef STACKDEBUG
#define STACKDEBUG 0
#else
#define STACKDEBUG 1
#endif

#if STACKDEBUG
#define Announce_stack_expand(stack) do {				\
    fprintf(stderr, "*** expanding %s stack (current size is %ld)\n",	\
	    stack->name, STACK_CAPACITY(*(stack)));			\
  } while(0);
#define Announce_stack_free(stack) do {					\
    fprintf(stderr, "*** freeing %s stack: %s, capacity %ld\n",		\
	    stack->name,						\
	    (stack->base == &stack->init[0]) ? "static" : "dynamic",	\
	    STACK_CAPACITY(*(stack)));					\
  } while(0);
#else
#define Announce_stack_expand(stack)
#define Announce_stack_free(stack)
#endif

/* ----------------------------------------------------------------------------- */

#define STACK_TYPE(entry_type) entry_type ## _stack

#define STACK_OF(entry_type, initial_size)				\
  typedef struct STACK_TYPE(entry_type) {				\
    entry_type *limit;							\
    entry_type *next;							\
    /* base points either to initb or a malloc'd stack */		\
    entry_type *base;							\
    const char *name;							\
    int maxtop;								\
    entry_type init[(initial_size)];					\
  } STACK_TYPE(entry_type);

#define STACK_INIT_FUNCTION(entry_type)					\
  static void entry_type ## _stack_init(STACK_TYPE(entry_type) *stack) { \
    /* start off using statically allocated storage */			\
    stack->base = &stack->init[0];					\
    stack->next = stack->base;						\
    stack->limit = stack->base + (sizeof(stack->init) / sizeof(entry_type)); \
    stack->maxtop = 0;							\
    stack->name = #entry_type;						\
  }

#define STACK_FREE_FUNCTION(entry_type)					\
  static void entry_type ## _stack_free(STACK_TYPE(entry_type) *stack) { \
    Announce_stack_free(stack);						\
    if (stack->base != &stack->init[0]) free(stack->base);		\
  }

#define STACK_EXPAND_FUNCTION(entry_type, maximum_size)			\
  static int entry_type ## _stack_expand(STACK_TYPE(entry_type) *stack) { \
    Announce_stack_expand(stack);					\
    entry_type *newstack;						\
    /* current stack size */						\
    int n = STACK_CAPACITY(*stack);					\
    if (n >= (maximum_size)) return 0;					\
    int newsize = 2 * n;						\
    if (newsize > (maximum_size)) newsize = (maximum_size);		\
    newstack = (entry_type *)malloc(newsize * sizeof(entry_type));	\
    if (!newstack) return 0;						\
    memcpy(newstack, stack->base, n * sizeof(entry_type));		\
    if (stack->base != stack->init) free(stack->base);			\
    stack->base = newstack;						\
    stack->limit = newstack + newsize;					\
    stack->next = newstack + n;						\
    stack->maxtop = n;							\
    return 1;								\
  }

#define STACK_PUSH_FUNCTION(entry_type, track_max)			\
  static inline int entry_type ## _stack_push(STACK_TYPE(entry_type) *stack, entry_type frame) { \
    if (0)								\
      printf("entering PUSH: next=%ld, limit=%ld, base==init: %s\n",	\
	     stack->next - stack->base,					\
	     STACK_CAPACITY(*stack),					\
	     (stack->base == &stack->init[0]) ? "static" : "dynamic");	\
    if (stack->next == stack->limit) {					\
      if (!entry_type ## _stack_expand(stack)) return 0;		\
    }									\
    *(stack->next) = frame;						\
    stack->next++;							\
    if (track_max) { 							\
      if ((stack->next - stack->base) > stack->maxtop)			\
	stack->maxtop = stack->next - stack->base;			\
    }									\
    return 1;								\
  }

#define STACK_POP_FUNCTION(entry_type)					\
  static inline void entry_type ## _stack_pop(STACK_TYPE(entry_type) *stack) { \
    stack->next--;							\
    assert( stack->next >= stack->base );				\
  }


  
    
