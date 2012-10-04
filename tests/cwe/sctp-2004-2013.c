// RUN: %linuxcc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %linuxcc -D__PATCH__ -m64 %s | intck | diagdiff %s
//
// http://www.securityfocus.com/archive/1/362953

#include "linux.h"

#define sctp_sk(sk)	((struct sctp_sock *)(sk))

struct sctp_endpoint {
	char *debug_name;
};

struct sctp_sock {
	struct sctp_endpoint *ep;
};

int sctp_setsockopt(struct sock *sk, char *optval, int optlen)
{
#ifndef __PATCH__
	char *tmp;

	if (NULL == (tmp = kmalloc(optlen + 1, GFP_KERNEL))) // exp: {{sadd}}
		return -ENOMEM;
	if (copy_from_user(tmp, optval, optlen)) // exp: {{size}}
		return -EFAULT;
	tmp[optlen] = '\000';
	sctp_sk(sk)->ep->debug_name = tmp;
#else
	/* There is no patch; the option was removed. */
#endif

	return 0;
}
