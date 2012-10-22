// RUN: %cc -DNORETURN= %s | antiopt -S -ideal-shift -bugon-int -anti-dce | FileCheck %s
//
// https://bugzilla.kernel.org/show_bug.cgi?id=14287

void bar(void);

int foo(unsigned char log_groups_per_flex)
{
	unsigned int groups_per_flex;
	groups_per_flex = 1 << log_groups_per_flex;
	// CHECK-NOT: call void @bar
	if (groups_per_flex == 0) {
		bar();
		return 1;
	}
	return 0;
}
