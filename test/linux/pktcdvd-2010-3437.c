// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
//
// http://git.kernel.org/linus/252a52aa4fa22a668f019e55b3aac3ff71ec1c29

#include "linux.h"

#define MAX_WRITERS		8

struct pkt_ctrl_command {
	u32 command;
	u32 dev_index;
	u32 dev;
	u32 pkt_dev;
	u32 num_devices;
	u32 padding;
};

struct block_device {
	dev_t			bd_dev;
};

struct pktcdvd_device {
	struct block_device	*bdev;
	dev_t			pkt_dev;
};

struct pktcdvd_device *pkt_devs[MAX_WRITERS];

#ifndef __PATCH__
static struct pktcdvd_device *pkt_find_dev_from_minor(int dev_minor)
#else
static struct pktcdvd_device *pkt_find_dev_from_minor(unsigned int dev_minor)
#endif
{
	if (dev_minor >= MAX_WRITERS)
		return NULL;
	return pkt_devs[dev_minor]; // exp: {{array}}
}

void pkt_get_status(struct pkt_ctrl_command *ctrl_cmd)
{
	struct pktcdvd_device *pd;

	pd = pkt_find_dev_from_minor(ctrl_cmd->dev_index);
	if (pd) {
		ctrl_cmd->dev = new_encode_dev(pd->bdev->bd_dev);
		ctrl_cmd->pkt_dev = new_encode_dev(pd->pkt_dev);
	} else {
		ctrl_cmd->dev = 0;
		ctrl_cmd->pkt_dev = 0;
	}
	ctrl_cmd->num_devices = MAX_WRITERS;
}
