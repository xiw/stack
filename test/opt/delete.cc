// RUN: %cc %s | optck | diagdiff --prefix=exp %s

struct C {
  void f();
  bool x;
};

void C::f() {
  if (x) {
    delete this;
  } 
}
