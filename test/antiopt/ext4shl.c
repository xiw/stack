// RUN: %cc %s | optck | diagdiff --prefix=exp %s
//
// https://bugzilla.kernel.org/show_bug.cgi?id=14287

void bar(void);

int foo(unsigned char log_groups_per_flex)
{
	unsigned int groups_per_flex;
	groups_per_flex = 1 << log_groups_per_flex;
	if (groups_per_flex == 0) {
		bar();		// exp: {{anti-dce}}
		return 1;
	}
	return 0;
}
