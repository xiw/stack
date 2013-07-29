// RUN: %cc %s | optck | diagdiff --prefix=exp %s

#include <stdio.h>
#include <stdlib.h>

int foo(int n)
{
  char *p = malloc(1);
  p[0] = 1;

  char *q = realloc(p, 2);
  if (q == 0)
    return p[0];

  return 2;
}
