// TODO: %cc %s | antiopt -S -loop-rotate -bugon-bounds -anti-simplify -simplifycfg | FileCheck %s
//
// http://blog.regehr.org/archives/767

enum {N = 32};
static int a[N], pfx[N];

void prefix_sum (void)
{
  int i, accum;
  // CHECK: for.body:
  // CHECK: br label %for.body
  for (i = 0, accum = a[0]; i < N; i++, accum += a[i])
    pfx[i] = accum;
}

void foo(int *);
void init(void)
{
	foo(a);
	foo(pfx);
}
