// RUN: %cc %s | antiopt -S -bugon-null -bugon-alias -anti-simplify -simplifycfg | FileCheck %s
//
// http://blog.regehr.org/archives/767

#include <stdio.h>
#include <stdlib.h>
 
int main()
{
  // CHECK: @malloc
  // CHECK: @realloc
  int *p = (int*)malloc(sizeof(int));
  int *q = (int*)realloc(p, sizeof(int));
  *p = 1;
  *q = 2;
  // CHECK-NOT: call i32 @printf
  if (p == q)
    printf("%d %d\n", *p, *q);
}
