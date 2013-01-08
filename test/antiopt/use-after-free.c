// RUN: %cc %s | optck | diagdiff --prefix=exp %s

void foo() {
  char *p = malloc(8);
  p[2] = 1;
  free(p);
}

void bar() {
  char *p = malloc(8); // exp: {{anti-dce}}
  free(p);
  p[4] = 2;
}
