




#ifndef _TIRPC_RPCCOM_H
#define	_TIRPC_RPCCOM_H

#include <rpc/rpc_com.h>

#ifdef __cplusplus
extern "C" {
#endif

struct netbuf *__rpc_set_netbuf(struct netbuf *, const void *, size_t);

struct netbuf *__rpcb_findaddr_timed(rpcprog_t, rpcvers_t,
    const struct netconfig *, const char *host, CLIENT **clpp,
    struct timeval *tp);

bool_t __rpc_control(int,void *);

bool_t __svc_clean_idle(fd_set *, int, bool_t);
bool_t __xdrrec_setnonblock(XDR *, int);
bool_t __xdrrec_getrec(XDR *, enum xprt_stat *, bool_t);
void __xprt_unregister_unlocked(SVCXPRT *);
void __xprt_set_raddr(SVCXPRT *, const struct sockaddr_storage *);


extern int __svc_maxrec;

#ifdef __cplusplus
}
#endif

#endif 
