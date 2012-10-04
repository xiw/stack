// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
// http://git.kernel.org/linus/2ebc3464781ad24474abcbd2274e6254689853b5

#include "linux.h"

struct inode {
	loff_t		i_size;
};

#define BTRFS_MAX_METADATA_BLOCKSIZE	65536

void btrfs_lookup_first_ordered_extent(struct inode * inode, u64 file_offset);

long btrfs_ioctl_clone(struct inode *src, u64 s_blocksize,
		       u64 off, u64 olen)
{
	u64 len = olen;
	u64 bs = s_blocksize;
	int ret;

	BUG_ON(s_blocksize < 4096);
	BUG_ON(s_blocksize > BTRFS_MAX_METADATA_BLOCKSIZE);
	BUG_ON(src->i_size < 0)
	BUG_ON(src->i_size > OFFSET_MAX);

	ret = -EINVAL;
#ifndef __PATCH__
	if (off >= src->i_size || off + len > src->i_size)
#else
	if (off + len > src->i_size || off + len < off)
#endif
		goto out;
	if (len == 0)
		olen = len = src->i_size - off;
	/* if we extend to eof, continue to block boundary */
	if (off + len == src->i_size)
		len = ((src->i_size + bs-1) & ~(bs-1))
			-off;

	/* verify the end result is block aligned */
	if ((off & (bs-1)) || ((off + len) & (bs-1)))	// exp: {{uadd}}
		goto out;

	btrfs_lookup_first_ordered_extent(src, off + len);

	ret = 0;
out:
	return ret;
}
