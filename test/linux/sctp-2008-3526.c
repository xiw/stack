// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp32
// RUN: %linuxcc -m64 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s --prefix=exp32
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
//
// http://git.kernel.org/linus/30c2235cbc477d4629983d440cdc4f496fec9246

#include "linux.h"

struct sctp_auth_bytes {
	u32	len;
	u8	data[];
};

struct sctp_auth_bytes *sctp_auth_create_key(u32 key_len, gfp_t gfp)
{
	struct sctp_auth_bytes *key;

#ifdef __PATCH__
	/* Verify that we are not going to overflow INT_MAX */
	if ((INT_MAX - key_len) < sizeof(struct sctp_auth_bytes))
		return NULL;
#endif
	/* Allocate the shared key */
	key = kmalloc(sizeof(struct sctp_auth_bytes) + key_len, gfp); // exp32: {{uadd}}
	if (!key)
		return NULL;

	key->len = key_len;

	return key;
}
