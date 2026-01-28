







#ifndef _RPC_RPCCOM_H
#define	_RPC_RPCCOM_H





#define	RPC_MAXDATASIZE 9000
#define	RPC_MAXADDRSIZE 1024

#define __RPC_GETXID(now) ((u_int32_t)getpid() ^ (u_int32_t)(now)->tv_sec ^ \
    (u_int32_t)(now)->tv_usec)

#ifdef __cplusplus
extern "C" {
#endif
extern u_int __rpc_get_a_size(int);
extern int __rpc_dtbsize(void);
extern int _rpc_dtablesize(void);
extern struct netconfig * __rpcgettp(int);
extern  int  __rpc_get_default_domain(char **);

char *__rpc_taddr2uaddr_af(int, const struct netbuf *);
struct netbuf *__rpc_uaddr2taddr_af(int, const char *);
int __rpc_fixup_addr(struct netbuf *, const struct netbuf *);
int __rpc_sockinfo2netid(struct __rpc_sockinfo *, const char **);
int __rpc_seman2socktype(int);
int __rpc_socktype2seman(int);
void *rpc_nullproc(CLIENT *);
int __rpc_sockisbound(int);

struct netbuf *__rpcb_findaddr(rpcprog_t, rpcvers_t, const struct netconfig *,
			       const char *, CLIENT **);
bool_t rpc_control(int,void *);

char *_get_next_token(char *, int);

#ifdef __cplusplus
}
#endif

#endif 
