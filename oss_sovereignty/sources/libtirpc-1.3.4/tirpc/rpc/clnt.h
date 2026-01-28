





#ifndef _TIRPC_CLNT_H_
#define _TIRPC_CLNT_H_

#include <rpc/clnt_stat.h>
#include <rpc/auth.h>

#include <netconfig.h>
#include <sys/un.h>


#define RPCB_MULTICAST_ADDR "ff02::202"


#define IS_UNRECOVERABLE_RPC(s) (((s) == RPC_AUTHERROR) || \
	((s) == RPC_CANTENCODEARGS) || \
	((s) == RPC_CANTDECODERES) || \
	((s) == RPC_VERSMISMATCH) || \
	((s) == RPC_PROCUNAVAIL) || \
	((s) == RPC_PROGUNAVAIL) || \
	((s) == RPC_PROGVERSMISMATCH) || \
	((s) == RPC_CANTDECODEARGS))


struct rpc_err {
	enum clnt_stat re_status;
	union {
		int RE_errno;		
		enum auth_stat RE_why;	
		struct {
			rpcvers_t low;	
			rpcvers_t high;	
		} RE_vers;
		struct {		
			int32_t s1;
			int32_t s2;
		} RE_lb;		
	} ru;
#define	re_errno	ru.RE_errno
#define	re_why		ru.RE_why
#define	re_vers		ru.RE_vers
#define	re_lb		ru.RE_lb
};



typedef struct __rpc_client {
	AUTH	*cl_auth;			
	struct clnt_ops {
		
		enum clnt_stat	(*cl_call)(struct __rpc_client *,
				    rpcproc_t, xdrproc_t, void *, xdrproc_t,
				        void *, struct timeval);
		
		void		(*cl_abort)(struct __rpc_client *);
		
		void		(*cl_geterr)(struct __rpc_client *,
					struct rpc_err *);
		
		bool_t		(*cl_freeres)(struct __rpc_client *,
					xdrproc_t, void *);
		
		void		(*cl_destroy)(struct __rpc_client *);
		
		bool_t          (*cl_control)(struct __rpc_client *, u_int,
				    void *);
	} *cl_ops;
	void 			*cl_private;	
	char			*cl_netid;	
	char			*cl_tp;		
} CLIENT;



struct rpc_timers {
	u_short		rt_srtt;	
	u_short		rt_deviate;	
	u_long		rt_rtxcur;	
};


#define FEEDBACK_REXMIT1	1	
#define FEEDBACK_OK		2	    


  
#define CLCR_SET_LOWVERS	3
#define CLCR_GET_LOWVERS	4
 
#define RPCSMALLMSGSIZE 400	




#define	CLNT_CALL(rh, proc, xargs, argsp, xres, resp, secs) \
	((*(rh)->cl_ops->cl_call)(rh, proc, xargs, \
		argsp, xres, resp, secs))
#define	clnt_call(rh, proc, xargs, argsp, xres, resp, secs) \
	((*(rh)->cl_ops->cl_call)(rh, proc, xargs, \
		argsp, xres, resp, secs))


#define	CLNT_ABORT(rh)	((*(rh)->cl_ops->cl_abort)(rh))
#define	clnt_abort(rh)	((*(rh)->cl_ops->cl_abort)(rh))


#define	CLNT_GETERR(rh,errp)	((*(rh)->cl_ops->cl_geterr)(rh, errp))
#define	clnt_geterr(rh,errp)	((*(rh)->cl_ops->cl_geterr)(rh, errp))



#define	CLNT_FREERES(rh,xres,resp) ((*(rh)->cl_ops->cl_freeres)(rh,xres,resp))
#define	clnt_freeres(rh,xres,resp) ((*(rh)->cl_ops->cl_freeres)(rh,xres,resp))


#define	CLNT_CONTROL(cl,rq,in) ((*(cl)->cl_ops->cl_control)(cl,rq,in))
#define	clnt_control(cl,rq,in) ((*(cl)->cl_ops->cl_control)(cl,rq,in))


#define CLSET_TIMEOUT		1	
#define CLGET_TIMEOUT		2	
#define CLGET_SERVER_ADDR	3	
#define CLGET_FD		6	
#define CLGET_SVC_ADDR		7	
#define CLSET_FD_CLOSE		8	
#define CLSET_FD_NCLOSE		9	
#define CLGET_XID 		10	
#define CLSET_XID		11	
#define CLGET_VERS		12	
#define CLSET_VERS		13	
#define CLGET_PROG		14	
#define CLSET_PROG		15	
#define CLSET_SVC_ADDR		16	
#define CLSET_PUSH_TIMOD	17	
#define CLSET_POP_TIMOD		18	

#define CLSET_RETRY_TIMEOUT 4   
#define CLGET_RETRY_TIMEOUT 5   
#define CLSET_ASYNC		19
#define CLSET_CONNECT		20	


#define	CLNT_DESTROY(rh)	((*(rh)->cl_ops->cl_destroy)(rh))
#define	clnt_destroy(rh)	((*(rh)->cl_ops->cl_destroy)(rh))




#define RPCTEST_PROGRAM		((rpcprog_t)1)
#define RPCTEST_VERSION		((rpcvers_t)1)
#define RPCTEST_NULL_PROC	((rpcproc_t)2)
#define RPCTEST_NULL_BATCH_PROC	((rpcproc_t)3)



#define NULLPROC ((rpcproc_t)0)




#ifdef __cplusplus
extern "C" {
#endif
extern CLIENT *clnt_create(const char *, const rpcprog_t, const rpcvers_t,
			   const char *);


 
extern CLIENT * clnt_create_timed(const char *, const rpcprog_t,
	const rpcvers_t, const char *, const struct timeval *);



extern CLIENT *clnt_create_vers(const char *, const rpcprog_t, rpcvers_t *,
				const rpcvers_t, const rpcvers_t,
				const char *);



extern CLIENT * clnt_create_vers_timed(const char *, const rpcprog_t,
	rpcvers_t *, const rpcvers_t, const rpcvers_t, const char *,
	const struct timeval *);



extern CLIENT *clnt_tp_create(const char *, const rpcprog_t,
			      const rpcvers_t, const struct netconfig *);



extern CLIENT * clnt_tp_create_timed(const char *, const rpcprog_t,
	const rpcvers_t, const struct netconfig *, const struct timeval *);




extern CLIENT *clnt_tli_create(const int, const struct netconfig *,
			       struct netbuf *, const rpcprog_t,
			       const rpcvers_t, const u_int, const u_int);



extern CLIENT *clnt_vc_create(const int, const struct netbuf *,
			      const rpcprog_t, const rpcvers_t,
			      u_int, u_int);

extern CLIENT *clntunix_create(struct sockaddr_un *,
			       u_long, u_long, int *, u_int, u_int);



extern CLIENT *clnt_dg_create(const int, const struct netbuf *,
			      const rpcprog_t, const rpcvers_t,
			      const u_int, const u_int);



extern CLIENT *clnt_raw_create(rpcprog_t, rpcvers_t);

#ifdef __cplusplus
}
#endif



#ifdef __cplusplus
extern "C" {
#endif
extern void clnt_pcreateerror(const char *);			
extern char *clnt_spcreateerror(const char *);			
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern void clnt_perrno(enum clnt_stat);		
extern char *clnt_sperrno(enum clnt_stat);		
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern void clnt_perror(CLIENT *, const char *);	 	
extern char *clnt_sperror(CLIENT *, const char *);		
#ifdef __cplusplus
}
#endif



struct rpc_createerr {
	enum clnt_stat cf_stat;
	struct rpc_err cf_error; 
};

#ifdef __cplusplus
extern "C" {
#endif
extern struct rpc_createerr	*__rpc_createerr(void);
#ifdef __cplusplus
}
#endif
#define get_rpc_createerr()	(*(__rpc_createerr()))
#define rpc_createerr		(*(__rpc_createerr()))


#ifdef __cplusplus
extern "C" {
#endif
extern enum clnt_stat rpc_call(const char *, const rpcprog_t,
			       const rpcvers_t, const rpcproc_t,
			       const xdrproc_t, const char *,
			       const xdrproc_t, char *, const char *);
#ifdef __cplusplus
}
#endif



typedef bool_t (*resultproc_t)(caddr_t, ...);

#ifdef __cplusplus
extern "C" {
#endif
extern enum clnt_stat rpc_broadcast(const rpcprog_t, const rpcvers_t,
				    const rpcproc_t, const xdrproc_t,
				    caddr_t, const xdrproc_t, caddr_t,
				    const resultproc_t, const char *);
extern enum clnt_stat rpc_broadcast_exp(const rpcprog_t, const rpcvers_t,
					const rpcproc_t, const xdrproc_t,
					caddr_t, const xdrproc_t, caddr_t,
					const resultproc_t, const int,
					const int, const char *);
#ifdef __cplusplus
}
#endif


#include <rpc/clnt_soc.h>

#endif 
