





#ifndef _TIRPC_SVC_H
#define _TIRPC_SVC_H




#define SVCGET_VERSQUIET	1
#define SVCSET_VERSQUIET	2
#define SVCGET_CONNMAXREC	3
#define SVCSET_CONNMAXREC	4


#define RPC_SVC_CONNMAXREC_SET  0	
#define RPC_SVC_CONNMAXREC_GET  1

enum xprt_stat {
	XPRT_DIED,
	XPRT_MOREREQS,
	XPRT_IDLE
};


typedef struct __rpc_svcxprt {
	int		xp_fd;
#define	xp_sock		xp_fd
	u_short		xp_port;	 
	const struct xp_ops {
	    
	    bool_t	(*xp_recv)(struct __rpc_svcxprt *, struct rpc_msg *);
	    
	    enum xprt_stat (*xp_stat)(struct __rpc_svcxprt *);
	    
	    bool_t	(*xp_getargs)(struct __rpc_svcxprt *, xdrproc_t,
				void *);
	    
	    bool_t	(*xp_reply)(struct __rpc_svcxprt *, struct rpc_msg *);
	    
	    bool_t	(*xp_freeargs)(struct __rpc_svcxprt *, xdrproc_t,
				void *);
	    
	    void	(*xp_destroy)(struct __rpc_svcxprt *);
	} *xp_ops;
	int		xp_addrlen;	 
	struct sockaddr_in6 xp_raddr;	 
	
	const struct xp_ops2 {
		
		bool_t  (*xp_control)(struct __rpc_svcxprt *, const u_int,
				void *);
	} *xp_ops2;
	char		*xp_tp;		 
	char		*xp_netid;	 
	struct netbuf	xp_ltaddr;	 
	struct netbuf	xp_rtaddr;	 
	struct opaque_auth xp_verf;	 
	void		*xp_p1;		 
	void		*xp_p2;		 
	void		*xp_p3;		 
	int		xp_type;	 
} SVCXPRT;


struct svc_req {
	
	u_int32_t	rq_prog;	
	u_int32_t	rq_vers;	
	u_int32_t	rq_proc;	
	struct opaque_auth rq_cred;	
	void		*rq_clntcred;	
	SVCXPRT		*rq_xprt;	

	
	caddr_t		rq_clntname;	
	caddr_t		rq_svcname;	
};


#define svc_getrpccaller(x) (&(x)->xp_rtaddr)


#define SVC_RECV(xprt, msg)				\
	(*(xprt)->xp_ops->xp_recv)((xprt), (msg))
#define svc_recv(xprt, msg)				\
	(*(xprt)->xp_ops->xp_recv)((xprt), (msg))

#define SVC_STAT(xprt)					\
	(*(xprt)->xp_ops->xp_stat)(xprt)
#define svc_stat(xprt)					\
	(*(xprt)->xp_ops->xp_stat)(xprt)

#define SVC_GETARGS(xprt, xargs, argsp)			\
	(*(xprt)->xp_ops->xp_getargs)((xprt), (xargs), (argsp))
#define svc_getargs(xprt, xargs, argsp)			\
	(*(xprt)->xp_ops->xp_getargs)((xprt), (xargs), (argsp))

#define SVC_REPLY(xprt, msg)				\
	(*(xprt)->xp_ops->xp_reply) ((xprt), (msg))
#define svc_reply(xprt, msg)				\
	(*(xprt)->xp_ops->xp_reply) ((xprt), (msg))

#define SVC_FREEARGS(xprt, xargs, argsp)		\
	(*(xprt)->xp_ops->xp_freeargs)((xprt), (xargs), (argsp))
#define svc_freeargs(xprt, xargs, argsp)		\
	(*(xprt)->xp_ops->xp_freeargs)((xprt), (xargs), (argsp))

#define SVC_DESTROY(xprt)				\
	(*(xprt)->xp_ops->xp_destroy)(xprt)
#define svc_destroy(xprt)				\
	(*(xprt)->xp_ops->xp_destroy)(xprt)

#define SVC_CONTROL(xprt, rq, in)			\
	(*(xprt)->xp_ops2->xp_control)((xprt), (rq), (in))



#ifdef __cplusplus
extern "C" {
#endif
extern bool_t	svc_reg(SVCXPRT *, const rpcprog_t, const rpcvers_t,
			void (*)(struct svc_req *, SVCXPRT *),
			const struct netconfig *);
#ifdef __cplusplus
}
#endif



#ifdef __cplusplus
extern "C" {
#endif
extern void	svc_unreg(const rpcprog_t, const rpcvers_t);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern void	xprt_register(SVCXPRT *);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern void	xprt_unregister(SVCXPRT *);
#ifdef __cplusplus
}
#endif




#ifdef __cplusplus
extern "C" {
#endif
extern bool_t	svc_sendreply(SVCXPRT *, xdrproc_t, void *);
extern void	svcerr_decode(SVCXPRT *);
extern void	svcerr_weakauth(SVCXPRT *);
extern void	svcerr_noproc(SVCXPRT *);
extern void	svcerr_progvers(SVCXPRT *, rpcvers_t, rpcvers_t);
extern void	svcerr_auth(SVCXPRT *, enum auth_stat);
extern void	svcerr_noprog(SVCXPRT *);
extern void	svcerr_systemerr(SVCXPRT *);
extern int	rpc_reg(rpcprog_t, rpcvers_t, rpcproc_t,
			char *(*)(char *), xdrproc_t, xdrproc_t,
			char *);
#ifdef __cplusplus
}
#endif




extern int svc_maxfd;
extern fd_set svc_fdset;
#define svc_fds svc_fdset.fds_bits[0]	
extern struct pollfd *svc_pollfd;
extern int svc_max_pollfd;


#ifdef __cplusplus
extern "C" {
#endif
extern void rpctest_service(void);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern void	svc_getreq(int);
extern void	svc_getreqset(fd_set *);
extern void	svc_getreq_common(int);
struct pollfd;
extern void	svc_getreq_poll(struct pollfd *, int);

extern void	svc_run(void);
extern void	svc_exit(void);
#ifdef __cplusplus
}
#endif


#define	RPC_ANYSOCK	-1
#define RPC_ANYFD	RPC_ANYSOCK



#ifdef __cplusplus
extern "C" {
#endif

extern int svc_create(void (*)(struct svc_req *, SVCXPRT *),
			   const rpcprog_t, const rpcvers_t, const char *);





extern SVCXPRT *svc_tp_create(void (*)(struct svc_req *, SVCXPRT *),
				   const rpcprog_t, const rpcvers_t,
				   const struct netconfig *);
        



extern SVCXPRT *svc_tli_create(const int, const struct netconfig *,
			       const struct t_bind *, const u_int,
			       const u_int);




extern SVCXPRT *svc_vc_create(const int, const u_int, const u_int);



extern SVCXPRT *svcunix_create(int, u_int, u_int, char *);

extern SVCXPRT *svc_dg_create(const int, const u_int, const u_int);
        



extern SVCXPRT *svc_fd_create(const int, const u_int, const u_int);



extern SVCXPRT *svcunixfd_create(int, u_int, u_int);


extern SVCXPRT *svc_raw_create(void);


int svc_dg_enablecache(SVCXPRT *, const u_int);

int __rpc_get_local_uid(SVCXPRT *_transp, uid_t *_uid);

#ifdef __cplusplus
}
#endif



#include <rpc/svc_soc.h>



#endif 
