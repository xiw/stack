// RUN: %cc %s | optck | diagdiff --prefix=exp %s
//
// http://blog.regehr.org/archives/767

#include <stdio.h>
#include <stdlib.h>
 
int main()
{
  int *p = (int*)malloc(sizeof(int)); // exp: {{anti-dce}}
  int *q = (int*)realloc(p, sizeof(int));
  *p = 1;
  *q = 2;
  if (p == q)
    printf("%d %d\n", *p, *q);
}
