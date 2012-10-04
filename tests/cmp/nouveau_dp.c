// RUN: %cc %s | cmpck | diagdiff %s
// http://git.kernel.org/linus/44ab8cc56c45ca781371a4a77f35da19cf5db028

#define DP_TRAIN_PRE_EMPHASIS_9_5	(3 << 3)
#define DP_TRAIN_VOLTAGE_SWING_1200	(3 << 0)

typedef unsigned char u8;

int foo(u8 lane)
{
	u8 lpre = (lane & 0x0c) >> 2;
	u8 lvsw = (lane & 0x03) >> 0;
	if (lpre == DP_TRAIN_PRE_EMPHASIS_9_5) // CHECK: {{comparison always false}}
		return -1;
	if ((lpre << 3) == DP_TRAIN_PRE_EMPHASIS_9_5)
		return -2;
	if (lvsw == DP_TRAIN_VOLTAGE_SWING_1200)
		return -3;
	return 0;
}
