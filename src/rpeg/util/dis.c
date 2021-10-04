/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  dis.c                                                                    */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  Portions Copyright 2007, Lua.org & PUC-Rio (via lpeg)                    */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

/* For getopt() on Linux to be included by unistd, need a feature macro */
#ifndef _POSIX_C_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>

#include "file.h"
#include "ktable.h"
#include "vm.h"
//#include "print.h"

#define aux(pc) ((pc)->i.aux)

extern int sizei(const Instruction *p);

#define UNUSED(x) (void)(x)

static void error(const char *message, const char *additional) {
  fprintf(stderr, "Error: %s; %s\n", message, additional);
  fflush(stderr);
  exit(-1);
}

/* --------------------------------- TODO: Move these into a common code file. --------------------------------- */

/* We will not worry about freeing the error message allocated on the
 * heap by strerror_error, because this function is a debugging aid.
 * It is only invoked when there is no message (string) defined for a
 * particular code (int) in the message list.
 */
static const char *strerror_error(const char *filename, int line, int code) {
  char *error_message;
  if (!asprintf(&error_message, "%s:%d: INVALID ERROR CODE %d", filename, line, code))
    return "ERROR: asprintf failed";
  return error_message;
}

#define STRERROR_LEN(array) ((int) ((sizeof (array) / sizeof (const char *))))

#define STRERROR(code, message_list)					\
  ( (((code) > 0) && ((code) < STRERROR_LEN(message_list)))		\
    ? (message_list)[(code)]						\
    : strerror_error(__FILE__, __LINE__, code) )

/* ------------------------------------------------------------------------------------------------------------- */

/* TEMP: Branch aotcomp1 has a newer version of these print functions,
   but we can't use those here because this branch has an older
   revision of the ktable structure (and probably the instruction
   encodings, too). 
 */

static void print_ktable (Ktable *kt) {
  Ktable_element *elt;
  size_t entry_len;
  const char *entry_name;
  int n, i;
  if (!kt) return;		/* no ktable? */
  n = (int) ktable_len(kt);
  for (i = 1; i <= n; i++) {
    elt = ktable_element(kt, i);
    entry_name = ktable_element_name(kt, i, &entry_len);
    printf("%4d:", i);
    //    if (elt->entrypoint != KTABLE_NO_ENTRYPOINT)
    // printf(" [%4d]", elt->entrypoint);
    //    else 
    //      printf("       ");
    //printf(" (%04X)", elt->flags);
    printf(" %.*s", (int) entry_len, entry_name);
    printf("\n");
  }
}

static void print_charset (const byte *st) {
  int i;
  printf("[");
  for (i = 0; i <= UCHAR_MAX; i++) {
    int first = i;
    while (testchar(st, i) && i <= UCHAR_MAX) i++;
    if (i - 1 == first)  /* unary range? */
      printf("(%02x)", first);
    else if (i - 1 > first)  /* non-empty range? */
      printf("(%02x-%02x)", first, i - 1);
  }
  printf("]");
}

static void printcapkind (int kind) {
  printf("%s", CAPTURE_NAME(kind));
}

static void printjmp (const Instruction *op, const Instruction *p) {
  printf("JMP to %d", (int)(p + (p + 1)->offset - op));
}

/* Print instructions with their absolute addresses, showing jump
 * destinations with their relative addresses and also the computed
 * target addresses.
 */
static void *print_instruction (const Instruction *op, Instruction *p, void *context) {
  UNUSED(context);
  printf("%4ld  %s ", (long)(p - op), OPCODE_NAME(opcode(p)));
  switch (opcode(p)) {
/*   case IXCall: { */
/*     printf("pkg = #%d, localname = #%d", aux(p), addr(p)); */
/*     break; */
/*   } */
  case IChar: {
    printf("'%c'", aux(p));
    break;
  }
  case ITestChar: {
    printf("'%c'", aux(p)); printjmp(op, p);
    break;
  }
  case IOpenCapture: {
    printcapkind(addr(p));
    printf(" #%d", aux(p));
    break;
  }
  case ISet: {
    print_charset((p+1)->buff);
    break;
  }
  case ITestSet: {
    print_charset((p+2)->buff); printjmp(op, p);
    break;
  }
  case ISpan: {
    print_charset((p+1)->buff);
    break;
  }
  case IOpenCall: {
    printf("-> %d", addr(p));
    break;
  }
  case IBehind: {
    printf("#%d", aux(p));
    break;
  }
  case IJmp: case ICall: case ICommit: case IChoice:
  case IPartialCommit: case IBackCommit: case ITestAny: {
    printjmp(op, p);
    break;
  }
  default: break;
  }
  printf("\n");
  return NULL;
}

/* TODO: move this to a more appropriate source code file */
static void walk_instructions (Instruction *p,
			int codesize,
			void *(*operation)(const Instruction *, Instruction *, void *),
			void *context) {
  Instruction *op = p;
  int n = (int) codesize;
  while (p < op + n) {
    (*operation)(op, p, context);
    p += sizei(p);
  }
}

static void print_instructions (Instruction *p, int codesize) {
  walk_instructions(p, codesize, &print_instruction, NULL);
}



/* ------------------------------------------------------------------------------------------------------------- */

static int ktable_dups(Ktable *kt, int *distinct_dups, int *unique_elements) {
  Ktable_element *elements = ktable_sorted_index(kt);
  /* Now count the duplicates. */
  int dups = 0, distinct = 0, new = 0, unique = 1;
  for (int i = 1; i < ktable_len(kt); i++) {
    if (ktable_entry_name_compare(kt, &elements[i-1], &elements[i]) == 0) {
      if (new == 1) { distinct++; new = 0; }
      dups++;
    } else {
      unique++;
      new = 1;
    }
  }
  *distinct_dups = distinct;
  *unique_elements = unique;
  return dups;
}

static void print_usage_and_exit(char *progname) {
    printf("Usage: %s [-k] [-i] [-s] rplx_file [rplx_file ...]\n", progname);
    exit(-1);
}

int main(int argc, char **argv) {

  int flag;
  int kflag = 0;
  int iflag = 0;
  int sflag = 0;
  int cflag = 0;

  while ((flag = getopt(argc, argv, "kisc")) != -1)
    switch (flag) {
    case 'k':			/* print ktable (symbol table) */
        kflag = 1;
        break;
    case 'i':			/* print instruction vector */
        iflag = 1;
        break;
    case 's':			/* print summary */
        sflag = 1;
        break;
    case 'c':			/* compact the ktable (for testing) */
        cflag = 1;
        break;
    case '?':
      fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default:
      abort ();
    }

  int err;
  Chunk c;
  char *fn;

  if (optind == argc) print_usage_and_exit(argv[0]);
  
  if (!kflag && !iflag && !sflag && !cflag) kflag = iflag = sflag = 1; /* default is -kis */

  for (int i = optind; i < argc; i++) {
    fn = argv[i];
    printf("File: %s\n\n", fn);
    err = file_load(fn,  &c);
    if (err) error(STRERROR(err, FILE_MESSAGES), "expected rplx file to load successfully");
    if (kflag) {
      printf("Symbol table:\n");
      print_ktable(c.ktable);
      printf("\n");
    }
    if (iflag) {
      printf("Code:\n");
      print_instructions(c.code, c.codesize);
      printf("\n");
    }
    if (sflag) {
      int dups, distinct_dups, unique_elements;
      //printf("File: %s\n", fn);
      dups = ktable_dups(c.ktable, &distinct_dups, &unique_elements);
      printf("Codesize: %zu instructions, %zu bytes\n", c.codesize, c.codesize * sizeof(Instruction));
      printf("Symbols: %d symbols in a block of %zu bytes; ",
	     ktable_len(c.ktable), c.ktable->blocksize);
      printf("%d unique symbols, and %d are dups of %d distinct symbols\n",
	     unique_elements, dups, distinct_dups);
      printf("\n");
    }
    if (cflag) {
      const char *element_name;
      size_t element_len;
      int newidx;
      Ktable *ckt = ktable_compact(c.ktable);
      printf("Compacted ktable:\n");
      print_ktable(ckt);
      for (int idx = 1; idx <= ktable_len(c.ktable); idx++) {
	element_name = ktable_element_name(c.ktable, idx, &element_len);
	newidx = ktable_compact_search(ckt, element_name, element_len);
	if (newidx == 0) printf("*** ERROR:  ");
	printf("%4d --> %4d\n", idx, newidx);
      }
    }
  }
}

