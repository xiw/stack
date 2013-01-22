// RUN: %cc %s | optck | diagdiff --prefix=exp %s

#include <stdlib.h>

int foo(char *p) {
  char *q = malloc(8);
  char *r = malloc(4);

  r[0] = 0;

  if (p == q)
    return 1;
  if (p == r) // exp: {{anti-simplify}}
    return 2;
  if (q == r) // exp: {{anti-simplify}}
    return 3;

  char *s = realloc(r, 8);
  if (!s)
    return 4;

  r[0] = 0;  // exp: {{anti-dce}}
  return 5;
}
