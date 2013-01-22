// TODO: %cc %s | optck | diagdiff --prefix=exp %s
//
// http://blog.regehr.org/archives/767

enum {N = 32};
static int a[N], pfx[N];

void prefix_sum (void)
{
  int i, accum;
  for (i = 0, accum = a[0]; i < N; i++, accum += a[i]) // exp: {{anti-simplify}}
    pfx[i] = accum;
}

void foo(int *);
void init(void)
{
	foo(a);
	foo(pfx);
}
