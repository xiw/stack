// RUN: %kcc -o - %s | %kundef --check-prefix=exp
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=30475

int foo(int a)
{
	if (!(a + 100 > a)) // exp: {{undefined behavior}}
		__builtin_trap();
	return a;
}
