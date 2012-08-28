// RUN: %kcc -nostdinc -m32 -o - %s | %kint --check-prefix=exp
// RUN: %kcc -nostdinc -m64 -o - %s | %kint --check-prefix=exp
// RUN: %kcc -nostdinc -D__PATCH__ -m32 -o - %s | %kint --check-prefix=exp-patch
// RUN: %kcc -nostdinc -D__PATCH__ -m64 -o - %s | %kint --check-prefix=exp-patch

// http://git.kernel.org/linus/b7058842c940ad2c08dd829b21e5c92ebe3b8758

#include "linux.h"

#define IFNAMSIZ	16
#define SO_BINDTODEVICE	25

struct socket;

int ax25_setsockopt(struct socket *sock, int level, int optname,
#ifndef __PATCH__
	char __user *optval, int optlen)
#else
	char __user *optval, unsigned int optlen)
#endif
{
	char devname[IFNAMSIZ];
	int res = 0;

	if (optlen < sizeof(int))
		return -EINVAL;

	switch (optname) {
	case SO_BINDTODEVICE:
		if (optlen > IFNAMSIZ)
			optlen = IFNAMSIZ;
		if (copy_from_user(devname, optval, optlen)) { // exp: {{size}}
			res = -EFAULT;
			break;
		}
	default:
		res = -ENOPROTOOPT;
	}
	return res;
}
