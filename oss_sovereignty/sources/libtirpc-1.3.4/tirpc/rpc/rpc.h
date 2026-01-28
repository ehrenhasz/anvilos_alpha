


#ifndef _TIRPC_RPC_H
#define _TIRPC_RPC_H

#include <rpc/types.h>		
#include <sys/socket.h>
#include <netinet/in.h>


#include <rpc/xdr.h>		


#include <rpc/auth.h>		


#include <rpc/clnt.h>		


#include <rpc/rpc_msg.h>	
#include <rpc/auth_unix.h>	


#include <rpc/auth_des.h>	


#include <rpc/svc_auth.h>	
#include <rpc/svc.h>		


#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#ifndef _KERNEL
#include <rpc/rpcb_clnt.h>	
#include <rpc/svc_mt.h>
#endif
#include <rpc/rpcent.h>

#ifndef UDPMSGSIZE
#define UDPMSGSIZE 8800
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern int get_myaddress(struct sockaddr_in *);
extern int bindresvport(int, struct sockaddr_in *);
extern int registerrpc(int, int, int, char *(*)(char [UDPMSGSIZE]),
    xdrproc_t, xdrproc_t);
extern int callrpc(const char *, int, int, int, xdrproc_t, void *,
    xdrproc_t , void *);
extern int getrpcport(char *, int, int, int);

char *taddr2uaddr(const struct netconfig *, const struct netbuf *);
struct netbuf *uaddr2taddr(const struct netconfig *, const char *);

struct sockaddr;
extern int bindresvport_sa(int, struct sockaddr *);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
int __rpc_nconf2fd(const struct netconfig *);
int __rpc_nconf2fd_flags(const struct netconfig *, int);
int __rpc_nconf2sockinfo(const struct netconfig *, struct __rpc_sockinfo *);
int __rpc_fd2sockinfo(int, struct __rpc_sockinfo *);
u_int __rpc_get_t_size(int, int, int);
#ifdef __cplusplus
}
#endif

#endif 
