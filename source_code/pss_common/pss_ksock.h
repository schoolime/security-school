#ifndef __PSS_KSOCK_H_
#define __PSS_KSOCK_H_

#include <linux/kernel.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/inet.h>

struct socket *pss_ksock_create(int domain, int type, int protocol);
int pss_ksock_connect(struct socket *sk, struct sockaddr *addr, int len);
ssize_t pss_ksock_send(struct socket *sk, const void *buff, size_t len, int flags);
ssize_t pss_ksock_recv(struct socket *sk, void *buff, size_t len, int flags);
int pss_ksock_shutdown(struct socket *sk, int how);
void pss_ksock_close(struct socket *sk);

#endif // __PSS_KSOCK_H_
