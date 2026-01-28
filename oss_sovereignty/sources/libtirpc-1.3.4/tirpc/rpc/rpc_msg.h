





#ifndef _TIRPC_RPC_MSG_H
#define _TIRPC_RPC_MSG_H

#define RPC_MSG_VERSION		((u_int32_t) 2)
#define RPC_SERVICE_PORT	((u_short) 2048)

#include <rpc/auth.h>



enum msg_type {
	CALL=0,
	REPLY=1
};

enum reply_stat {
	MSG_ACCEPTED=0,
	MSG_DENIED=1
};

enum accept_stat {
	SUCCESS=0,
	PROG_UNAVAIL=1,
	PROG_MISMATCH=2,
	PROC_UNAVAIL=3,
	GARBAGE_ARGS=4,
	SYSTEM_ERR=5
};

enum reject_stat {
	RPC_MISMATCH=0,
	AUTH_ERROR=1
};




struct accepted_reply {
	struct opaque_auth	ar_verf;
	enum accept_stat	ar_stat;
	union {
		struct {
			rpcvers_t low;
			rpcvers_t high;
		} AR_versions;
		struct {
			caddr_t	where;
			xdrproc_t proc;
		} AR_results;
		
	} ru;
#define	ar_results	ru.AR_results
#define	ar_vers		ru.AR_versions
};


struct rejected_reply {
	enum reject_stat rj_stat;
	union {
		struct {
			rpcvers_t low;
			rpcvers_t high;
		} RJ_versions;
		enum auth_stat RJ_why;  
	} ru;
#define	rj_vers	ru.RJ_versions
#define	rj_why	ru.RJ_why
};


struct reply_body {
	enum reply_stat rp_stat;
	union {
		struct accepted_reply RP_ar;
		struct rejected_reply RP_dr;
	} ru;
#define	rp_acpt	ru.RP_ar
#define	rp_rjct	ru.RP_dr
};


struct call_body {
	rpcvers_t cb_rpcvers;	
	rpcprog_t cb_prog;
	rpcvers_t cb_vers;
	rpcproc_t cb_proc;
	struct opaque_auth cb_cred;
	struct opaque_auth cb_verf; 
};


struct rpc_msg {
	u_int32_t		rm_xid;
	enum msg_type		rm_direction;
	union {
		struct call_body RM_cmb;
		struct reply_body RM_rmb;
	} ru;
#define	rm_call		ru.RM_cmb
#define	rm_reply	ru.RM_rmb
};
#define	acpted_rply	ru.RM_rmb.ru.RP_ar
#define	rjcted_rply	ru.RM_rmb.ru.RP_dr

#ifdef __cplusplus
extern "C" {
#endif

extern bool_t	xdr_callmsg(XDR *, struct rpc_msg *);


extern bool_t	xdr_callhdr(XDR *, struct rpc_msg *);


extern bool_t	xdr_replymsg(XDR *, struct rpc_msg *);



extern bool_t	xdr_accepted_reply(XDR *, struct accepted_reply *);


extern bool_t	xdr_rejected_reply(XDR *, struct rejected_reply *);


extern void	_seterr_reply(struct rpc_msg *, struct rpc_err *);
#ifdef __cplusplus
}
#endif

#endif 
