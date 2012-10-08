// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
//
// http://git.kernel.org/linus/b52a360b2aa1c59ba9970fb0f52bbb093fcc7a24

#include "linux.h"

#define XFS_ERROR(e)		(e)
#define XFS_IFINLINE		0x01
#define XFS_ILOCK_SHARED	(1<<3)
#define MAXPATHLEN		1024
#define EFSCORRUPTED		EUCLEAN

typedef int64_t xfs_fsize_t;

typedef struct xfs_ifork {
	unsigned char		if_flags;
	union {
		char		*if_data;
	} if_u1;
} xfs_ifork_t;

typedef struct xfs_icdinode {
	xfs_fsize_t		di_size;
} xfs_icdinode_t;

typedef struct xfs_inode {
	xfs_ifork_t		i_df;
	xfs_icdinode_t		i_d;
} xfs_inode_t;

void xfs_ilock(xfs_inode_t *, uint);
void xfs_iunlock(xfs_inode_t *, uint);

int xfs_readlink_bmap(xfs_inode_t *, char *);

int
xfs_readlink(
	xfs_inode_t     *ip,
	char		*link)
{
#ifndef __PATCH__
	int		pathlen;
#else
	xfs_fsize_t	pathlen;
#endif
	int		error = 0;

	xfs_ilock(ip, XFS_ILOCK_SHARED);

	pathlen = ip->i_d.di_size;
	if (!pathlen)
		goto out;

#ifdef __PATCH__
	if (pathlen < 0 || pathlen > MAXPATHLEN) {
		return XFS_ERROR(EFSCORRUPTED);
	}
#endif

	if (ip->i_df.if_flags & XFS_IFINLINE) {
		memcpy(link, ip->i_df.if_u1.if_data, pathlen); // exp: {{size}}
		link[pathlen] = '\0';
	} else {
		error = xfs_readlink_bmap(ip, link);
	}

 out:
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	return error;
}
