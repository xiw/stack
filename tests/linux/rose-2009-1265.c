// RUN: %linuxcc %s | intck | diagdiff %s --prefix=exp
// RUN: %linuxcc -D__PATCH__ %s | intck | diagdiff %s
// http://git.kernel.org/linus/83e0bbcbe2145f160fbaa109b0439dae7f4a38a9

#include "linux.h"

#define AX25_MAX_DIGIS			8

#define AX25_BPQ_HEADER_LEN		16
#define AX25_KISS_HEADER_LEN		1

#define AX25_HEADER_LEN			17
#define AX25_ADDR_LEN			7
#define AX25_DIGI_HEADER_LEN		(AX25_MAX_DIGIS * AX25_ADDR_LEN)
#define AX25_MAX_HEADER_LEN		(AX25_HEADER_LEN + AX25_DIGI_HEADER_LEN)

#define ROSE_MIN_LEN			3

#define MSG_DONTWAIT			0x40

struct kiocb;

struct socket {
	struct sock	*sk;
};

struct msghdr {
	unsigned int	msg_flags;
};

int rose_sendmsg(struct kiocb *iocb, struct socket *sock,
		 struct msghdr *msg, size_t len)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	int err;
	int size;

#ifdef __PATCH__
	/* Build a packet */
	/* Sanity check the packet size */
	if (len > 65535)
		return -EMSGSIZE;
#endif
	size = len + AX25_BPQ_HEADER_LEN + AX25_MAX_HEADER_LEN + ROSE_MIN_LEN; // exp: {{uadd}}

	if ((skb = sock_alloc_send_skb(sk, size, msg->msg_flags & MSG_DONTWAIT, &err)) == NULL) // exp: {{size}}
		return err;

	return len;
}
