// RUN: %cc %s | cmpck | diagdiff %s
// http://git.kernel.org/linus/44b0052c5cb4e75389ed3eb9e98c29295a7dadfb

#define BIT(n)		(1UL << (n))

#define PCH_EPASSIVE	BIT(5)
#define PCH_EWARN	BIT(6)

#define PCH_REC		0x00007f00
#define PCH_TEC		0x000000ff

typedef unsigned int u32;

int foo(u32 status, u32 errc)
{
	if (status & PCH_EWARN)	{
		if (((errc & PCH_REC) >> 8) > 96)
			return -1;
		if ((errc & PCH_TEC) > 96)
			return -2;
	}
	if (status & PCH_EPASSIVE) {
		if (((errc & PCH_REC) >> 8) > 127) // CHECK: {{comparison always false}}
			return -3;
		if ((errc & PCH_TEC) > 127)
			return -4;
	}
	return 0;
}
