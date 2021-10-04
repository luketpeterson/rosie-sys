/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  ktable-test.c                                                            */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ktable.h"

void *error(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  fflush(stderr);
  exit(-1);
}

int main(int argc, char **argv) {

  int ok;
  
  printf("\nPart 1: test all functions of one table\n\n");

  Ktable *k1 = ktable_new(2, 12);
  if (!k1) error("failed to create new table");
  ktable_dump(k1);

  ok = ktable_add(k1, "first", 5);
  if (!ok) error("first add failed");
  ktable_dump(k1);

  ok = ktable_add(k1, "#2", 2);
  if (!ok) error("second add failed");
  ktable_dump(k1);

  ok = ktable_add(k1, "33", 2);
  if (!ok) error("third add failed");
  ktable_dump(k1);

  ok = ktable_add(k1, "3", 1);
  if (!ok) error("fourth add failed");
  ktable_dump(k1);
  
  for (int i = 1; i <= 20; i++) {
    ok = ktable_add(k1, "abcdef", 6);
    if (!ok) error("repeated add failed");
    ktable_dump(k1);
  }

  char justright[KTABLE_MAX_ELEMENT_LEN + 1];
  
  for (int i = 0; i < KTABLE_MAX_ELEMENT_LEN; i++)
    justright[i] = 'A';
  justright[KTABLE_MAX_ELEMENT_LEN] = '\0';

  size_t justright_len = strnlen(justright, 2 * KTABLE_MAX_ELEMENT_LEN);
  printf("Length of next string is %zu\n", justright_len);

  ok = ktable_add(k1, justright, justright_len);
  if (!ok) error("justright add failed");
  ktable_dump(k1);

  size_t element_len;
  const char *element = ktable_element_name(k1, ok, &element_len);
  int truncated = (element_len < justright_len);
  printf("Length of last element added is %zu and %s truncated... ",
	 element_len, truncated ? "WAS" : "was not");
  printf(!truncated ? "correct.\n" : "ERROR!\n");

  char toobig[KTABLE_MAX_ELEMENT_LEN + 2];
  for (int i = 0; i < KTABLE_MAX_ELEMENT_LEN + 1; i++)
    toobig[i] = 'B';

  toobig[KTABLE_MAX_ELEMENT_LEN + 1] = '\0';

  size_t toobig_len = strnlen(toobig, 2 * KTABLE_MAX_ELEMENT_LEN);
  printf("Length of next string is %zu\n", toobig_len);

  ok = ktable_add(k1, toobig, toobig_len);
  if (!ok) error("toobig add failed");
  ktable_dump(k1);

  element = ktable_element_name(k1, ok, &element_len);
  truncated = (element_len < toobig_len);
  printf("Length of last element added is %zu and %s truncated... ",
	 element_len, truncated ? "WAS" : "was not");
  printf(truncated ? "correct.\n" : "ERROR!\n");

  printf("Testing indexing into ktable with bad index values:\n");
  for (int i = -2; i < ktable_len(k1) + 2; i++) {
    element = ktable_element_name(k1, i, &element_len);
    printf("  index %d is %s\n", i, element==NULL ? "invalid" : "ok");
  }

  /* ---------------------------------------------------------------------------------------- */
  printf("\nPart 2: concat two tables\n\n");

  Ktable *k2 = ktable_new(4, 4*30);
  if (!k2) error("failed to create new table");

  printf("Length of k1 is %d; length of (new) k2 is %d\n", ktable_len(k1), ktable_len(k2));

  int n;
  if (ktable_concat(k1, k2, &n) != KTABLE_OK) error("failed to concat two tables");

  printf("moved contents of k1 into k2;  amount to correct by is %d\n", n);
  if (n != 0) {
    error("'amount to correct by' was wrong (should have been 0)!\n");
  }
  ktable_dump(k2);

  if (ktable_concat(k1, k2, &n) != KTABLE_OK) error("failed to concat two tables");

  printf("moved contents of k1 into k2;  amount to correct by is %d\n", n);
  if (n != 26) {
    error("'amount to correct by' was wrong (should have been 26)!\n");
  }
  ktable_dump(k2);

  Ktable *k3 = ktable_new(4, 4*30);

  if (ktable_concat(k3, k2, &n) != KTABLE_OK) error("failed to concat two tables");
  
  printf("moved contents of (empty) k3 into k2;  amount to correct by is %d\n", n);
  if (n != 0) {
    error("'amount to correct by' was wrong (should have been 0)!\n");
  }
  ktable_dump(k2);
  
  /* ---------------------------------------------------------------------------------------- */
  printf("\nPart 3: compact a table\n\n");

  ktable_free(k3);
  k3 = ktable_compact(k2);
  if (!k3) error("compact_ktable failed");
  printf("length of compacted ktable is %d\n", ktable_len(k3));
  ktable_dump(k3);

}
