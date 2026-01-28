









#ifndef _RPC_RPCB_CLNT_H
#define	_RPC_RPCB_CLNT_H




#include <rpc/types.h>
#include <rpc/rpcb_prot.h>
#ifdef __cplusplus
extern "C" {
#endif
extern bool_t rpcb_set(const rpcprog_t, const rpcvers_t,
		       const struct netconfig  *, const struct netbuf *);
extern bool_t rpcb_unset(const rpcprog_t, const rpcvers_t,
			 const struct netconfig *);
extern rpcblist	*rpcb_getmaps(const struct netconfig *, const char *);
extern enum clnt_stat rpcb_rmtcall(const struct netconfig *,
				   const char *, const rpcprog_t,
				   const rpcvers_t, const rpcproc_t,
				   const xdrproc_t, const caddr_t,
				   const xdrproc_t, const caddr_t,
				   const struct timeval,
				   const struct netbuf *);
extern bool_t rpcb_getaddr(const rpcprog_t, const rpcvers_t,
			   const struct netconfig *, struct netbuf *,
			   const  char *);
extern bool_t rpcb_gettime(const char *, time_t *);
extern char *rpcb_taddr2uaddr(struct netconfig *, struct netbuf *);
extern struct netbuf *rpcb_uaddr2taddr(struct netconfig *, char *);
#ifdef __cplusplus
}
#endif

#endif	
