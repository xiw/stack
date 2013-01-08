// RUN: %cc %s | optck | diagdiff --prefix=exp %s

#include <string.h>

void f1(char *x) {
  memcpy(&x[2], &x[4], 3); // exp: {{anti-dce}}
}

void f2(char *x) {
  memcpy(&x[2], &x[4], 2);
}

void f3(char *x) {
  memcpy(&x[2], &x[4], 1);
}

void f4(int *x) {
  memcpy(&x[2], &x[4], 9); // exp: {{anti-dce}}
}

void f5(int *x) {
  memcpy(&x[2], &x[4], 8);
}

void f6(int *x) {
  memcpy(&x[2], &x[4], 7);
}
