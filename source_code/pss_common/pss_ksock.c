#include <linux/version.h>	/* Comment #1 */
#include "pss_ksock.h"

////////////////////////////////////////////////////////////
/* Comment #2 */
struct socket *pss_ksock_create(int domain, int type, int protocol)
{
	struct socket *sk = NULL;
	int ret = 0;

	ret = sock_create(domain, type, protocol, &sk);
	if (ret < 0){
		pr_info("[%s] Failed to sock_create\n", __FUNCTION__);
		return NULL;
	}
	pr_debug("[%s] sock_create sk= 0x%p\n", __FUNCTION__, sk);
	return sk;
}

////////////////////////////////////////////////////////////
/* Comment #3 */
int pss_ksock_connect(struct socket * sk, struct sockaddr *addr, int len)
{
	int ret = -1;

	if (sk) {
		ret = sk->ops->connect(sk, addr, len, 0);
	}
	return ret;
}

////////////////////////////////////////////////////////////
/* Comment #4 */
ssize_t pss_ksock_send(struct socket * sk, const void *buff, size_t len, int flags)
{
	struct 	msghdr msg;
	struct 	iovec iov;
	int 	ret = 0;

	iov.iov_base = (void *)buff;
	iov.iov_len = (__kernel_size_t)len;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,0,0)
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
#else
	iov_iter_init(&msg.msg_iter, WRITE, &iov, 1, len);
#endif
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,0,0)
	// Send message
	ret = sock_sendmsg(sk, &msg, len);
#else
	// Send message
	ret = sock_sendmsg(sk, &msg);
#endif

	return ret;
}

////////////////////////////////////////////////////////////
/* Comment #5 */
ssize_t pss_ksock_recv(struct socket * sk, void *buff, size_t len, int flags)
{
	struct msghdr msg;
	struct iovec iov;
	int ret;
	
	iov.iov_base = (void *)buff;
	iov.iov_len = (__kernel_size_t)len;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,0,0)
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
#else
	iov_iter_init(&msg.msg_iter, READ, &iov, 1, len);
#endif
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags;
	
	ret = sock_recvmsg(sk, &msg, len, flags);

	return ret;
}

////////////////////////////////////////////////////////////
/* Comment #6 */
int pss_ksock_shutdown(struct socket *sk, int how)
{
	int ret = 0;

	if (sk) {
		ret = sk->ops->shutdown(sk, how);
	}
	
	return ret;
}

/* Comment #7 */
void pss_ksock_close(struct socket *sk)
{
	if(sk){
		sk->ops->release(sk);	
	}
	if(sk) {
		sock_release(sk);
	}
}
