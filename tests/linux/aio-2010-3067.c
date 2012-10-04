// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/75e1c70fc31490ef8a373ea2a4bea2524099b478

#include "linux.h"

struct iocb;

long do_io_submit(long nr, struct iocb __user *__user *iocbpp)
{
	long ret = 0;

	if (unlikely(nr < 0))
		return -EINVAL;
#ifdef __PATCH__
	if (unlikely(nr > LONG_MAX/sizeof(*iocbpp)))
		nr = LONG_MAX/sizeof(*iocbpp);
#endif
	if (unlikely(!access_ok(VERIFY_READ, iocbpp, (nr*sizeof(*iocbpp))))) // exp: {{umul}}
		return -EFAULT;

	return ret;
}
