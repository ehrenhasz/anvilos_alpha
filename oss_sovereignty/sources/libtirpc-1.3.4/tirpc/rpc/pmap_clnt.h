







#ifndef _RPC_PMAP_CLNT_H_
#define _RPC_PMAP_CLNT_H_

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/clnt.h>

#ifdef __cplusplus
extern "C" {
#endif
extern bool_t		pmap_set(u_long, u_long, int, int);
extern bool_t		pmap_unset(u_long, u_long);
extern struct pmaplist	*pmap_getmaps(struct sockaddr_in *);
extern enum clnt_stat	pmap_rmtcall(struct sockaddr_in *,
				     u_long, u_long, u_long,
				     xdrproc_t, caddr_t,
				     xdrproc_t, caddr_t,
				     struct timeval, u_long *);
extern enum clnt_stat	clnt_broadcast(u_long, u_long, u_long,
				       xdrproc_t, void *,
				       xdrproc_t, void *,
				       resultproc_t);
extern u_short		pmap_getport(struct sockaddr_in *,
				     u_long, u_long, u_int);
#ifdef __cplusplus
}
#endif

#endif 
