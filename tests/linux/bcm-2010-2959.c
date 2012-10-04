// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp32
// RUN: %linuxcc -m64 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/5b75c4973ce779520b9d1e392483207d6f842cde

#include "linux.h"

#define MAX_NFRAMES	256
#define CFSIZ		sizeof(struct can_frame)
#define MHSIZ		sizeof(struct bcm_msg_head)

struct bcm_msg_head {
	u32 nframes;
};

struct can_frame { char dummy[6]; };

struct bcm_op {
	struct can_frame *frames;
};

int bcm_tx_setup(struct bcm_msg_head *msg_head, struct bcm_op *op)
{
#ifndef __PATCH__
	if (msg_head->nframes < 1)
		return -EINVAL;
#else
	/* check nframes boundaries - we need at least one can_frame */
	if (msg_head->nframes < 1 || msg_head->nframes > MAX_NFRAMES)
		return -EINVAL;
#endif

	if (msg_head->nframes > 1) {
		op->frames = kmalloc(msg_head->nframes * CFSIZ, GFP_KERNEL); // exp32: {{umul}}
		if (!op->frames) {
			kfree(op);
			return -ENOMEM;
		}
	}

	return 0;
}
