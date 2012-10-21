// RUN: %cc -DNORETURN= %s | antiopt -S -bugon-int -anti-dce | FileCheck %s
// RUN: %cc %s | antiopt -S -bugon-int -anti-dce | FileCheck %s --check-prefix=NORETURN
//
// http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=616180

#ifndef NORETURN
#define NORETURN __attribute__((noreturn))
#endif
void my_raise(void) NORETURN;

int foo(int x, int y)
{
	// CHECK-NOT: call void @my_raise
	// NORETURN:  call void @my_raise
	if (y == 0)
		my_raise();
	return x / y;
}
