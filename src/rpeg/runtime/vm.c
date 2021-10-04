/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  vm.c  Matching vm                                                        */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  Portions Copyright 2007, Lua.org & PUC-Rio (via lpeg)                    */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "config.h"
#include "rplx.h"
#include "buf.h"
#include "vm.h"

#ifndef VMDEBUG
#define VMDEBUG 0
#else
#define VMDEBUG 1
#endif

#if VMDEBUG 

    #define Announce_doublecap(oldsize) do {				\
	fprintf(stderr, "*** doubling current capture array size of %d\n", (oldsize)); \
      } while(0);
    extern void printcode(Instruction *p);

#else

    #define Announce_doublecap(oldsize)
    static void printcode(Instruction *p) {UNUSED(p);}

#endif 

static const Instruction giveup = {.i.code = IGiveup, .i.aux = 0};

typedef struct BTEntry {
  const char *s;	      /* saved position (or NULL for calls) */
  const Instruction *p;	      /* next instruction */
  int caplevel;
} BTEntry;

/*
 * Size of an instruction
 */
int sizei (const Instruction *pc) {
  switch((Opcode) opcode(pc)) {
  case IPartialCommit: case ITestAny: case IJmp:
  case ICall: case IOpenCall: case IChoice:
  case ICommit: case IBackCommit: case IOpenCapture:
  case ITestChar: 
    return 2;
  case ISet: case ISpan:
    return CHARSETINSTSIZE;
  case ITestSet:
    return 1 + CHARSETINSTSIZE;
  default:
    return 1;
  }
}

#include "stack.c"            /* macros defining 'static inline' procedures */

STACK_OF(BTEntry, INIT_BACKTRACKSTACK)
STACK_INIT_FUNCTION(BTEntry)
STACK_FREE_FUNCTION(BTEntry)
STACK_EXPAND_FUNCTION(BTEntry, MAX_BACKTRACK)
STACK_PUSH_FUNCTION(BTEntry, RECORD_VMSTATS)
STACK_POP_FUNCTION(BTEntry)

/*
 * Double the size of the array of captures
 */
static Capture *doublecap (Capture *cap, Capture *initial_capture, int captop) {
  if (captop >= MAX_CAPLISTSIZE) return NULL;
  Announce_doublecap(captop);
  Capture *newc;
  int newcapsize = 2 * captop;
  if (newcapsize > MAX_CAPLISTSIZE) newcapsize = MAX_CAPLISTSIZE;
  newc = (Capture *)malloc(newcapsize * sizeof(Capture));
  memcpy(newc, cap, captop * sizeof(Capture));
  if (cap != initial_capture) free(cap); 
  return newc;
}

static void BTEntry_stack_print (BTEntry_stack *stack, const char *o, Instruction *op) {
  BTEntry *top;
  for (top = (stack->next - 1); top >= stack->base; top--)
    fprintf(stderr,
	    "%ld: pos=%ld, pc %ld: %s, caplevel=%d\n",
	   top - stack->base,
	   (top->s == NULL) ? -1 : (top->s - o), 
	   (top->p == &giveup) ? -1 : (top->p - op), 
	   OPCODE_NAME(opcode(top->p)),
	   top->caplevel);
}

#define BACKREF_DEBUG 0

#if BACKREF_DEBUG
static void print_caplist(Capture *capture, int captop, Ktable *kt) {
  for(int i = 0; i < captop; i++) {
    if (isopencap(&capture[i])) {
      printf("(%d %s ", i, ktable_element(kt, capidx(&capture[i])));
    } else {
      if (isclosecap(&capture[i])) {
	printf("%d) ", i);
      } else {
	printf("** %d ** ", i);
      }
    }
  }      
  printf("\n");
}
#endif

static int find_prior_capture(Capture *capture, int captop, int target_idx,
			      const char **s, const char **e, Ktable *kt) {
  UNUSED(kt);
  if (captop == 0) return 0;

#if BACKREF_DEBUG
  print_caplist(capture, captop, kt);
  printf("Target is [%d]%s, captop = %d\n", target_idx, ktable_element(kt, target_idx), captop);
#endif

  /* Skip backwards past any immediate OPENs. */
  int i;
  for (i = captop - 1; i > 0; i--) {
    if (!isopencap(&capture[i])) break;
  }
  int cap_end = i;
#if BACKREF_DEBUG
  printf("declaring this to be the end of the cap list: cap_end = %d\n", cap_end);
#endif
  /* Scan backwards for the first OPEN without a CLOSE. */
  int outer_cap = 0;
  int outer_capidx = 0;
  int balance = 0;
  /* Recall that capture[0] is always an OPEN for the outermost
     capture, which cannot have a matching CLOSE. */
  for (; i > 0; i--) {
#if BACKREF_DEBUG
    printf("looking for first unclosed open, i = %d\n", i);
#endif
    if (isopencap(&capture[i])) {
      if (balance == 0) break;
      balance += 1;
    } else {
      if (isclosecap(&capture[i])) {
	balance -= 1;
      } 
    }
  }
  outer_cap = i;
  outer_capidx = capidx(&capture[i]);
#if BACKREF_DEBUG
  printf("Found FIRST unclosed open at %d: [%d]%s\n", outer_cap, outer_capidx, ktable_element(kt, outer_capidx));
#endif
  /* Now search backward from the end for the target, skipping any
     other instances of outer_capidx */

  for (i = cap_end; i >= outer_cap; i--) {
#if BACKREF_DEBUG
    printf("looking for target at i=%d\n", i);
#endif
    if (isopencap(&capture[i]) && capidx(&capture[i]) == target_idx) {
#if BACKREF_DEBUG
      printf("found candidate target; now determining if it is inside [%d]%s\n",
         outer_cap, ktable_element(kt, outer_capidx));
#endif
      balance = 0;
      int j;
      for (j = i - 1; j >= outer_cap; j--) {
	if (isopencap(&capture[j])) {
#if BACKREF_DEBUG
	  printf("looking at open capture j = %d\n", j);
#endif
	  if ((balance >= 0) && capidx(&capture[j]) == outer_capidx) break;
	  balance += 1;
	} else {
	  if (isclosecap(&capture[j])) {
	    balance -= 1;
	  }
	}
      }
      if (j == outer_cap) {
#if BACKREF_DEBUG
	printf("No other instances of outer_cap to skip over\n");
#endif
	break; /* Nothing to skip over */
      }
    }
  }
  if (i == outer_cap - 1) {
#if BACKREF_DEBUG
    printf("did not find target; continuing the backwards search\n");
#endif
    for (i = outer_cap; i >= 0; i--) {
      if (isopencap(&capture[i]) && capidx(&capture[i]) == target_idx) break;
    }
    if (i < 0) return 0;
    if (! (isopencap(&capture[i]) && capidx(&capture[i]) == target_idx)) {
      return 0;
    }
  }

  /* This the open capture we are looking for */
/*   assert (isopencap(&capture[i]) && capidx(&capture[i]) == outer_capidx); */
#if BACKREF_DEBUG
  printf("FOUND open capture at i = %d, [%d]%s\n", i,
    	 capidx(&capture[i]), ktable_element(kt, capidx(&capture[i])));
#endif  
  *s = capture[i].s;                      /* start position */
  /* Now look for the matching close */
  i++;
  int j = 0;
  while (i <= captop) {
#if BACKREF_DEBUG
    printf("looking at i = %d (captop = %d)\n", i, captop);
#endif
    if (isclosecap(&capture[i])) {
      if (j == 0) {
	/* This must be the matching close capture */
#if BACKREF_DEBUG
	printf("i = %d: found close capture\n", i);
#endif
	*e = capture[i].s;               /* end position */
	return 1;	                 /* success */
      } else {
	j--;
	assert( j >= 0 );
      }
    } else {
      assert( isopencap(&capture[i]) );
      j++;
    }
    i++;
  } /* while looking for matching close*/
  /* Did not find the matching close */
#if BACKREF_DEBUG
  printf("did not find matching close!\n");
#endif
  return 0;
}

#if 0
#define PRINT_VM_STATE do {						\
    fprintf(stderr, "Next instruction: %ld %s\n",			\
	   p - op,							\
	   OPCODE_NAME(p->i.code));					\
    fprintf(stderr, "Stack: next=%ld, limit=%ld, base==init: %s\n",	\
	   stack.next - stack.base,					\
	   STACK_CAPACITY(stack),					\
	   (stack.base == &stack.init[0]) ? "static" : "dynamic");	\
    BTEntry_stack_print(&stack, o, op, &giveup_code);			\
  } while(0)
#else
#define PRINT_VM_STATE UNUSED(BTEntry_stack_print)
#endif

#if (RECORD_VMSTATS)
#define INCR_STAT(action, var) if ((action)) (var)++
#define UPDATE_STAT(action, var, value) if ((action)) (var) = (value)
#define UPDATE_CAPSTATS(inst) capstats[opcode(inst)==IOpenCapture ? addr(inst) : Cclose]++
#else
#define INCR_STAT(action, var) UNUSED((var))
#define UPDATE_STAT(action, var, value) UNUSED((var))
#define UPDATE_CAPSTATS(inst) UNUSED(capstats)
#endif

#define PUSH_CAPLIST						\
  if (++captop >= capsize) {					\
    capture = doublecap(capture, initial_capture, captop);	\
    if (!capture) {						\
      return MATCH_ERR_CAP;					\
    }								\
    *capturebase = capture;					\
    capsize = 2 * captop;					\
  }

#define JUMPBY(delta) pc = pc + (delta)

static int vm (const char **r,
	       const char *o, const char *s, const char *e,
	       Instruction *op, Capture **capturebase,
	       Stats *stats, int capstats[], Ktable *kt) {
  BTEntry_stack stack;
  BTEntry_stack_init(&stack);
  Capture *initial_capture = *capturebase;
  Capture *capture = *capturebase;
  int capsize = INIT_CAPLISTSIZE;
  int captop = 0;  /* point to first empty slot in captures */

  const Instruction *pc = op;  /* current instruction */
  BTEntry_stack_push(&stack, (BTEntry) {s, &giveup, 0});
  for (;;) {
    PRINT_VM_STATE;
    INCR_STAT(stats, stats->insts); 
    switch (opcode(pc)) {
      /* Mark S. reports that 98% of executed instructions are
       * ITestSet, IAny, IPartialCommit (in that order).  So we put
       * them first here, in case it speeds things up.  But with
       * branch prediction, it probably makes no difference.
       */
    case ITestSet: {
      assert(sizei(pc)==1+CHARSETINSTSIZE);
      assert(addr(pc));
      if (s < e && testchar((pc+2)->buff, (int)((byte)*s)))
	JUMPBY(1+CHARSETINSTSIZE); /* sizei */
      else JUMPBY(addr(pc));
      continue;
    }
    case IAny: {
      assert(sizei(pc)==1);
      if (s < e) { JUMPBY(1); s++; }
      else goto fail;
      continue;
    }
    case IPartialCommit: {
      assert(sizei(pc)==2);
      assert(addr(pc));
      assert(stack.next > stack.base && TOP(stack)->s != NULL);
      TOP(stack)->s = s;
      TOP(stack)->caplevel = captop;
      JUMPBY(addr(pc));
      continue;
    }
    case IEnd: {
      assert(sizei(pc)==1);
      assert(stack.next == stack.base + 1);
      /* This Cclose capture is a sentinel to mark the end of the
       * linked caplist.  If it is the only capture on the list,
       * then walk_captures will see it and not go any further.
       */
      setcapkind(&capture[captop], Cclose);
      capture[captop].s = NULL;
      UPDATE_STAT(stats, stats->backtrack, stack.maxtop);
      BTEntry_stack_free(&stack);
      *r = s;
      return MATCH_OK;
    }
    case IGiveup: {
      assert(sizei(pc)==1);
      assert(stack.next == stack.base);
      UPDATE_STAT(stats, stats->backtrack, stack.maxtop);
      BTEntry_stack_free(&stack);
      *r = NULL;
      return MATCH_OK;
    }
    case IRet: {
      assert(sizei(pc)==1);
      assert(stack.next > stack.base);
      assert(TOP(stack)->s == NULL);
      pc = TOP(stack)->p;
      BTEntry_stack_pop(&stack);
      continue;
    }
    case ITestAny: {
      assert(sizei(pc)==2);
      assert(addr(pc));
      if (s < e) JUMPBY(2);
      else JUMPBY(addr(pc));
      continue;
    }
    case IChar: {
      assert(sizei(pc)==1);
      if (s < e && ((byte)*s == ichar(pc))) { JUMPBY(1); s++; }
      else goto fail;
      continue;
    }
    case ITestChar: {
      assert(sizei(pc)==2);
      assert(addr(pc));
      if (s < e && ((byte)*s == ichar(pc))) JUMPBY(2);
      else JUMPBY(addr(pc));
      continue;
    }
    case ISet: {
      assert(sizei(pc)==CHARSETINSTSIZE);
      if (s < e && testchar((pc+1)->buff, (int)((byte)*s)))
	{ JUMPBY(CHARSETINSTSIZE); /* sizei */
	  s++;
	}
      else { goto fail; }
      continue;
    }
    case IBehind: {
      assert(sizei(pc)==1);
      int n = index(pc);
      if (n > s - o) goto fail;
      s -= n; JUMPBY(1);
      continue;
    }
    case ISpan: {
      assert(sizei(pc)==CHARSETINSTSIZE);
      for (; s < e; s++) {
	if (!testchar((pc+1)->buff, (int)((byte)*s))) break;
      }
      JUMPBY(CHARSETINSTSIZE);	/* sizei */
      continue;
    }
    case IJmp: {
      assert(sizei(pc)==2);
      assert(addr(pc));
      JUMPBY(addr(pc));
      continue;
    }
    case IChoice: {
      assert(sizei(pc)==2);
      assert(addr(pc));
      if (!BTEntry_stack_push(&stack, (BTEntry) {s, pc + addr(pc), captop}))
	return MATCH_ERR_STACK;
      JUMPBY(2);
      continue;
    }
    case ICall: {
      assert(sizei(pc)==2);
      assert(addr(pc));
      if (!BTEntry_stack_push(&stack, (BTEntry) {NULL, pc + 2, 0}))
	return MATCH_ERR_STACK;
      JUMPBY(addr(pc));
      continue;
    }
    case ICommit: {
      assert(sizei(pc)==2);
      assert(addr(pc));
      assert(stack.next > stack.base && TOP(stack)->s != NULL);
      BTEntry_stack_pop(&stack);
      JUMPBY(addr(pc));
      continue;
    }
    case IBackCommit: {
      assert(sizei(pc)==2);
      assert(addr(pc));
      assert(stack.next > stack.base && TOP(stack)->s != NULL);
      s = TOP(stack)->s;
      captop = TOP(stack)->caplevel;
      BTEntry_stack_pop(&stack);
      JUMPBY(addr(pc));
      continue;
    }
    case IFailTwice:
      assert(stack.next > stack.base);
      BTEntry_stack_pop(&stack);
      /* fallthrough */
    case IFail:
      assert(sizei(pc)==1);
    fail: { /* pattern failed: try to backtrack */
        do {  /* remove pending calls */
          assert(stack.next > stack.base);
          s = TOP(stack)->s;
	  BTEntry_stack_pop(&stack);
        } while (s == NULL);
        captop = PEEK(stack, 1)->caplevel;
        pc = PEEK(stack, 1)->p;
        continue;
      }
    case IBackref: {
      assert(sizei(pc)==1);
      /* Now find the prior capture that we want to reference */
      const char *startptr = NULL;
      const char *endptr = NULL;
      int target = index(pc);
      //printf("Entering IBackref, target = %d\n", target);
      int have_prior = find_prior_capture(capture, captop, target, &startptr, &endptr, kt);
      //printf("%s:%d: have_prior is %s\n", __FILE__, __LINE__, have_prior ? "true" : "false");
      if (have_prior) {
	assert( startptr && endptr );
	assert( endptr >= startptr );
	size_t prior_len = endptr - startptr;
	//printf("%s:%d: prior data is at %zu (0-based) is '%.*s'\n", __FILE__, __LINE__,
	//     (startptr - o),
	//     (int) prior_len, startptr);
	/* And check to see if the input at the current position */
	/* matches that prior captured text. */
	//printf("%s:%d: looking at %zu (0-based) '%.*s'\n", __FILE__, __LINE__,
	//       (e - o),
	//     (int) (e - s), s);
	if ( ((size_t)(e - s) >= prior_len) && (memcmp(s, startptr, prior_len) == 0) ) {
	  s += prior_len;
	  JUMPBY(1);
	  //printf("%s:%d: input matched prior!\n", __FILE__, __LINE__);
	  continue;
	} /* if input matches prior */
	//printf("%s:%d: input did not match prior\n", __FILE__, __LINE__);
      }	/* if have a prior match at all */
      //printf("%s:%d: input did not match or found no prior\n", __FILE__, __LINE__);
      /* Else no match. */
      goto fail;
    }
    case ICloseConstCapture: {
      assert(sizei(pc)==1);
      assert(index(pc));
      assert(captop > 0);
      capture[captop].s = s;
      setcapidx(&capture[captop], index(pc)); /* second ktable index */
      setcapkind(&capture[captop], Ccloseconst);
      goto pushcapture;
    }
    case ICloseCapture: {
      assert(sizei(pc)==1);
      assert(captop > 0);
      /* Roberto's lpeg checks to see if the item on the stack can
	 be converted to a full capture.  We skip that check,
	 because we have removed full captures.  This makes the
	 capture list 10-15% longer, but saves almost 2% in time.
      */
      capture[captop].s = s;
      setcapkind(&capture[captop], Cclose);
      pushcapture:		/* push, jump by 1 */
      UPDATE_CAPSTATS(pc);
      PUSH_CAPLIST;
      UPDATE_STAT(stats, stats->caplist, captop);
      JUMPBY(1);
      continue;
    }
    case IOpenCapture: {
      assert(sizei(pc)==2);
      capture[captop].s = s;
      setcapidx(&capture[captop], index(pc)); /* ktable index */
      setcapkind(&capture[captop], addr(pc)); /* kind of capture */
      UPDATE_CAPSTATS(pc);
      PUSH_CAPLIST;
      UPDATE_STAT(stats, stats->caplist, captop);
      JUMPBY(2);
      continue;
    }
    case IHalt: {				    /* rosie */
      assert(sizei(pc)==1);
      /* We could unwind the stack, committing everything so that we
	 can return everything captured so far.  Instead, we simulate
	 the effect of this in caploop() in lpcap.c.  (And that loop
	 is something we should be able to eliminate!)
      */
      setcapkind(&capture[captop], Cfinal);
      capture[captop].s = s;
      *r = s;
      UPDATE_STAT(stats, stats->backtrack, stack.maxtop);
      BTEntry_stack_free(&stack);
      return MATCH_OK;
    }
    default: {
      if (VMDEBUG) {
	fprintf(stderr, "Illegal opcode at %d: %d\n", (int) (pc - op), opcode(pc));
	printcode(op);		/* print until IEnd */
      }
      assert(0);
      BTEntry_stack_free(&stack);
      return MATCH_ERR_BADINST;
    } }
  }
}

/*
 * Get the initial position for the match, interpreting negative
 * values from the end of the input string, using Lua convention,
 * including 1-based indexing.
 */
static size_t initposition (int pos, size_t len) {
  if (pos > 0) {		/* positive index? */
    if ((size_t)pos <= len)	/* inside the string? */
      return (size_t)pos - 1;	/* correct to 0-based indexing */
    else return len;		/* crop at the end */
  }
  else {			     /* negative index */
    if ((size_t)(-pos) <= len)	     /* inside the string? */
      return len - ((size_t)(-pos)); /* return position from the end */
    else return 0;		     /* crop at the beginning */
  }
}


/* -------------------------------------------------------------------------- */


typedef struct Cap {
  const char *start;
  int count;
} Cap;

STACK_OF(Cap, INIT_CAPDEPTH)
STACK_INIT_FUNCTION(Cap)
STACK_FREE_FUNCTION(Cap)
STACK_EXPAND_FUNCTION(Cap, MAX_CAPDEPTH)
STACK_PUSH_FUNCTION(Cap, RECORD_VMSTATS)
STACK_POP_FUNCTION(Cap)

/* caploop() processes the sequence of captures created by the vm.
   This sequence encodes a nested, balanced list of Opens and Closes.

   caploop() would naturally be written recursively, but a few years
   ago, I rewrote it in the iterative form it has now, where it
   maintains its own stack.

   The stack is used to match up a Close capture (when we encounter it
   as we march along the capture sequence) with its corresponding Open
   (which we have pushed on our stack).

   The 'count' parameter contains the number of captures inside the
   Open at the top of the stack.  When it is not zero, the JSON
   encoder starts by emitting a comma "," because it is encoding a
   capture that is within a list of nested captures (but is not the
   first in that list).  Without 'count', a spurious comma would
   invalidate the JSON output.

   Note that the stack grows with the nesting depth of captures.  As
   of this writing (Friday, July 27, 2018), this depth rarely exceeds
   7 in the patterns we are seeing.
 */

#define capstart(cs) (capkind((cs)->cap)==Crosieconst ? NULL : (cs)->cap->s)

static int caploop (CapState *cs, Encoder encode, Buffer *buf, unsigned int *max_capdepth) {
  int err;
  const char *start;
  int count = 0;
  Cap_stack stack;
  Cap_stack_init(&stack);
  if (!Cap_stack_push(&stack, (Cap) {capstart(cs), 0})) {
    Cap_stack_free(&stack);
    return MATCH_STACK_ERROR;
  }
  err = encode.Open(cs, buf, 0);
  if (err) { Cap_stack_free(&stack); return err; }
  cs->cap++;
  while (STACK_SIZE(stack) > 0) {
    //    while (!isclosecap(cs->cap) && !isfinalcap(cs->cap)) {
    while (isopencap(cs->cap)) {
      if (!Cap_stack_push(&stack, (Cap) {capstart(cs), count})) {
	Cap_stack_free(&stack);
	return MATCH_STACK_ERROR;
      }
      err = encode.Open(cs, buf, count);
      if (err) { Cap_stack_free(&stack); return err; }
      count = 0;
      cs->cap++;
    }
    count = TOP(stack)->count;
    start = TOP(stack)->start;
    Cap_stack_pop(&stack);
    /* We cannot assume that every Open will be followed by a Close,
     * due to the (Rosie) introduction of a non-local exit (throw) out
     * of the lpeg vm.  We use a sentinel, a special Close different
     * from the one inserted by IEnd.  Here (below), we will look to
     * see if the Close is that special sentinel.  If so, then for
     * every still-open capture, we will synthesize a Close that was
     * never created because a non-local exit occurred.
     *
     * FUTURE: Maybe skip the creation of the closes?  Leave the
     * sentinel for the code that processes the captures to deal with.
     * I.e. emulate all the missing Closes there.  This is an
     * optimization that will only come into play when Halt is used,
     * though.  So it is NOT a high priority.
     */
    if (isfinalcap(cs->cap)) {
      Capture synthetic;
      synthetic.s = cs->cap->s;
      setcapidx(&synthetic, 0);
      setcapkind(&synthetic, Cclose);
      //      synthetic.siz = 1;	/* 1 means closed */
      cs->cap = &synthetic;
      while (1) {
	err = encode.Close(cs, buf, count, start);
	if (err) { Cap_stack_free(&stack); return err; }
	if (STACK_SIZE(stack)==0) break;
	Cap_stack_pop(&stack);
	count = TOP(stack)->count;
	start = TOP(stack)->start;
      }
      *max_capdepth = stack.maxtop;
      Cap_stack_free(&stack);
      return MATCH_HALT;
    }
    assert(!isopencap(cs->cap));
    err = encode.Close(cs, buf, count, start);
    if (err) { Cap_stack_free(&stack); return err; }
    cs->cap++;
    count++;
  }
  *max_capdepth = stack.maxtop;
  Cap_stack_free(&stack);
  return MATCH_OK;
}

/*
 * Prepare a CapState structure and traverse the entire list of
 * captures in the stack pushing its results. 's' is the subject
 * string. Call the output encoder functions for each capture (open,
 * close, or full).
 */
static int walk_captures (Capture *capture, const char *s,
			  Ktable *kt, Encoder encode,
			  /* outputs: */
			  Buffer *buf, int *abend, Stats *stats) {
  int err;
  *abend = 0;		       /* 0 => normal completion; 1 => halt/throw */
  if (isfinalcap(capture)) {
    *abend = 1;
    goto done;
  }
  if (!isclosecap(capture)) {  /* Any captures? */
    CapState cs;
    cs.ocap = cs.cap = capture;
    cs.s = s;
    cs.kt = kt;
    /* Rosie ensures that the pattern has an outer capture.  So
     * if we see a full capture, it is because the outermost
     * open/close was converted to a full capture.  And it must be the
     * only capture in the capture list (except for the sentinel
     * Cclose put there by the IEnd instruction.
     */
    unsigned int max_capdepth = 0;
    err = caploop(&cs, encode, buf, &max_capdepth);
    UPDATE_STAT(stats, stats->capdepth, max_capdepth);
    if (err == MATCH_HALT) {
      *abend = 1;
      goto done;
    }
    else
      if (err) return err;
  }
 done:
  return MATCH_OK;
}

int vm_match (Chunk *chunk, Buffer *input, int start, Encoder encode,
	      /* outputs: */ Match *match, Stats *stats) {
  Capture initial_capture[INIT_CAPLISTSIZE];
  Capture *capture = initial_capture;
  int err, abend;
  int t0, tmatch, tfinal;
  const char *r, *s;
  size_t i, l;

  t0 = clock();
  
  s = input->data;
  l = input->n;

  if (l > UINT_MAX) return MATCH_ERR_INPUT_LEN;

  i = initposition(start, l);
  int capstats[256] = {0};

  // PASSING KTABLE TO VM ONLY FOR DEBUGGING
  err = vm(&r, s, s + i, s + l, chunk->code, &capture, stats, capstats, chunk->ktable);

#if (VMDEBUG) 
  printf("vm() completed with err code %d, r as position = %ld\n", err, r ? r - s : 0);
  if (stats) printf("vm executed %d instructions\n", stats->insts); 
  printf("capstats from vm: Close %d, Rosiecap %d\n", capstats[Cclose], capstats[Crosiecap]); 
  for(int ii=0; ii<256; ii++)
    if (!(ii==Cclose || ii==Crosiecap || ii==Crosieconst || ii==Cbackref))
      assert(capstats[ii]==0); 
#endif

  tmatch = clock();

  if (err != MATCH_OK) return err;
  if (stats) stats->match_time += tmatch - t0;
  if (r == NULL) {
    /* We leave match->data alone, because it may be reused over
     * successive calls to match().
     */
    match->matched = 0;			/* no match */
    match->leftover = (unsigned int) l; /* leftover value is len */
    match->abend = 0;
    if (stats) stats->total_time += tmatch - t0; /* match time (vm only) */
    return MATCH_OK;
  }
  match->matched = 1;		/* match */
  if (!match->data) match->data = buf_new(0);
  if (!match->data) return MATCH_ERR_OUTPUT_MEM;

  err = walk_captures(capture, s, chunk->ktable, encode, match->data, &abend, stats);
  if (capture != initial_capture) free(capture);
  if (err != MATCH_OK) return err;

  tfinal = clock();
  match->leftover = (unsigned int) l - (r - s); /* leftover chars, in bytes */
  match->abend = abend;
  if (stats) stats->total_time += tfinal - t0;  /* total time (includes capture processing) */

  return MATCH_OK;
}

Match *match_new() {
  Match *m = malloc(sizeof(Match));
  m->data = NULL;
  return m;
}

void match_free(Match *m) {
  if (!m) return;
  /* N.B. The data buffer is NOT FREED HERE because its extent is
     independent of the match struct that points to it.
  */
  free(m);
}
