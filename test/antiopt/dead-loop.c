// RUN: %cc %s | optck | diagdiff --prefix=exp %s
//
// From krb5, krb5_ccache_copy() in clients/ksu/ccache.c.

void bar(void);

void foo()
{
	int *p = 0;
	int i = 0;
	if (p) {
		while (p[i]) {
			bar();
			++i;
		}
	}
}

