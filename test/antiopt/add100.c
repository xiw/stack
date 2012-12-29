// RUN: %cc %s | antiopt -S -bugon-int -anti-simplify -simplifycfg | FileCheck %s
//
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=30475

void bar(void);

// CHECK: i32 @foo
int foo(int a)
{
	// CHECK-NOT: call void @bar
	if (!(a + 100 > a))
		bar();
	return a;
}
