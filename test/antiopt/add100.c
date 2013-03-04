// RUN: %cc %s | optck | diagdiff --prefix=exp %s
//
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=30475

void bar(void);

int foo(int a)
{
	if (!(a + 100 > a))
		bar();		// exp: {{anti-dce}}
	return a;
}
