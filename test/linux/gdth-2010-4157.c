// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
//
// http://git.kernel.org/linus/f63ae56e4e97fb12053590e41a4fa59e7daa74a4

#include "linux.h"

typedef struct {
	u16 ionode;
	/* ... */
	unsigned long data_len;
	unsigned long sense_len;
	/* ... */
} gdth_ioctl_general;

typedef struct {
	/* ... */
} gdth_ha_str;

gdth_ha_str *gdth_find_ha(int hanum);
char *gdth_ioctl_alloc(gdth_ha_str *ha, int size, int scratch, u64 *paddr);
void gdth_ioctl_free(gdth_ha_str *ha, int size, char *buf, u64 paddr);

int ioc_general(void __user *arg, char *cmnd)
{
	gdth_ioctl_general gen;
	char *buf = NULL;
	u64 paddr; 
	gdth_ha_str *ha;

	if (copy_from_user(&gen, arg, sizeof(gdth_ioctl_general)))
		return -EFAULT;
	ha = gdth_find_ha(gen.ionode);
	if (!ha)
		return -EFAULT;
#ifdef __PATCH__
	if (gen.data_len > INT_MAX)
		return -EINVAL;
	if (gen.sense_len > INT_MAX)
		return -EINVAL;
	if (gen.data_len + gen.sense_len > INT_MAX)
		return -EINVAL;
#endif
	if (gen.data_len + gen.sense_len != 0) { // exp: {{uadd}}
		if (!(buf = gdth_ioctl_alloc(ha, gen.data_len + gen.sense_len,
					FALSE, &paddr)))
			return -EFAULT;
	}
	return 0;
}
