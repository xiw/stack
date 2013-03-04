// RUN: %cc -DNORETURN= %s | optck | diagdiff --prefix=exp %s
// RUN: %cc %s | optck | diagdiff %s
//
// http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=616180

#ifndef NORETURN
#define NORETURN __attribute__((noreturn))
#endif
void my_raise(void) NORETURN;

int foo(int x, int y)
{
	if (y == 0)
		my_raise();	// exp: {{anti-dce}}
	return x / y;
}
