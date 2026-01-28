





#ifndef _RPC_SVC_AUTH_H
#define _RPC_SVC_AUTH_H


typedef struct SVCAUTH {
	struct svc_auth_ops {
		int     (*svc_ah_wrap)(struct SVCAUTH *, XDR *, xdrproc_t,
				       caddr_t);
		int     (*svc_ah_unwrap)(struct SVCAUTH *, XDR *, xdrproc_t,
					 caddr_t);
		int     (*svc_ah_destroy)(struct SVCAUTH *);
		} *svc_ah_ops;
	caddr_t svc_ah_private;
} SVCAUTH;

#define SVCAUTH_WRAP(auth, xdrs, xfunc, xwhere) \
	((*((auth)->svc_ah_ops->svc_ah_wrap))(auth, xdrs, xfunc, xwhere))
#define SVCAUTH_UNWRAP(auth, xdrs, xfunc, xwhere) \
	((*((auth)->svc_ah_ops->svc_ah_unwrap))(auth, xdrs, xfunc, xwhere))
#define SVCAUTH_DESTROY(auth) \
	((*((auth)->svc_ah_ops->svc_ah_destroy))(auth))


#ifdef __cplusplus
extern "C" {
#endif
extern enum auth_stat _gss_authenticate(struct svc_req *, struct rpc_msg *,
		bool_t *);
extern enum auth_stat _authenticate(struct svc_req *, struct rpc_msg *);
extern int svc_auth_reg(int, enum auth_stat (*)(struct svc_req *,
			  struct rpc_msg *));

#ifdef __cplusplus
}
#endif

#endif 
