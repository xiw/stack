// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/cb26a24ee9706473f31d34cc259f4dcf45cd0644

#include "linux.h"

#define CA_CI				1
#define CA_CI_LINK			2
#define FW_CI_LL_SUPPORT(arm_app)	((arm_app) & 0x80000000)

typedef struct ca_slot_info_t {
	int num;
	int type;
	unsigned int flags;
} ca_slot_info_t;

struct av7110 {
	u32		arm_app;
	ca_slot_info_t	ci_slot[2];
};

int dvb_ca_ioctl(struct av7110 *av7110, void *parg)
{
	ca_slot_info_t *info = (ca_slot_info_t *)parg;

#ifndef __PATCH__
	if (info->num > 1)
		return -EINVAL;
#else
	if (info->num < 0 || info->num > 1)
		return -EINVAL;
#endif
	av7110->ci_slot[info->num].num = info->num; // exp: {{array}}
	av7110->ci_slot[info->num].type = FW_CI_LL_SUPPORT(av7110->arm_app) ?
						CA_CI_LINK : CA_CI;
	memcpy(info, &av7110->ci_slot[info->num], sizeof(ca_slot_info_t));
	return 0;
}
