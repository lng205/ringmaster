#include <stdio.h>
#include <stdlib.h>
#include "jerasure.h"

#define talloc(type, num) (type *) malloc(sizeof(type)*(num))

static void usage(char *s)
{
  fprintf(stderr, "usage: jerasure_01 r c w - creates and prints out a matrix in GF(2^w).\n\n");
  fprintf(stderr, "       Element i,j is equal to 2^(i*c+j)\n");
  fprintf(stderr, "       \n");
  fprintf(stderr, "This demonstrates jerasure_print_matrix().\n");
  if (s != NULL) fprintf(stderr, "%s\n", s);
  exit(1);
}

int main(int argc, char **argv)
{
  int r, c, w, i, n;
  int *matrix;

  if (argc != 4) usage(NULL);
  if (sscanf(argv[1], "%d", &r) == 0 || r <= 0) usage("Bad r");
  if (sscanf(argv[2], "%d", &c) == 0 || c <= 0) usage("Bad c");
  if (sscanf(argv[3], "%d", &w) == 0 || w <= 0) usage("Bad w");

  matrix = talloc(int, r*c);

  n = 1;
  for (i = 0; i < r*c; i++) {
    matrix[i] = n;
    n = galois_single_multiply(n, 2, w);
  }

  printf("<HTML><TITLE>jerasure_01");
  for (i = 1; i < argc; i++) printf(" %s", argv[i]);
  printf("</TITLE>\n");
  printf("<h3>jerasure_01");
  for (i = 1; i < argc; i++) printf(" %s", argv[i]);
  printf("</h3>\n");
  printf("<pre>\n");

  jerasure_print_matrix(matrix, r, c, w);
  return 0;
}