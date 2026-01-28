

#ifndef _TIRPC_AUTH_GSS_H
#define _TIRPC_AUTH_GSS_H

#include <rpc/clnt.h>
#include <gssapi/gssapi.h>


typedef enum {
	RPCSEC_GSS_DATA = 0,
	RPCSEC_GSS_INIT = 1,
	RPCSEC_GSS_CONTINUE_INIT = 2,
	RPCSEC_GSS_DESTROY = 3
} rpc_gss_proc_t;


typedef enum {
	RPCSEC_GSS_SVC_NONE = 1,
	RPCSEC_GSS_SVC_INTEGRITY = 2,
	RPCSEC_GSS_SVC_PRIVACY = 3
} rpc_gss_svc_t;

#define RPCSEC_GSS_VERSION	1


struct rpc_gss_sec {
	gss_OID		mech;		
	gss_qop_t	qop;		
	rpc_gss_svc_t	svc;		
	gss_cred_id_t	cred;		
	u_int		req_flags;	
	int		major_status;
	int		minor_status;
};


struct authgss_private_data {
	gss_ctx_id_t	pd_ctx;		
	gss_buffer_desc	pd_ctx_hndl;	
	u_int		pd_seq_win;	
};


extern gss_OID_desc krb5oid;
extern gss_OID_desc spkm3oid;


struct rpc_gss_cred {
	u_int		gc_v;		
	rpc_gss_proc_t	gc_proc;	
	u_int		gc_seq;		
	rpc_gss_svc_t	gc_svc;		
	gss_buffer_desc	gc_ctx;		
};


struct rpc_gss_init_res {
	gss_buffer_desc		gr_ctx;		
	u_int			gr_major;	
	u_int			gr_minor;	
	u_int			gr_win;		
	gss_buffer_desc		gr_token;	
};


#define MAXSEQ		0x80000000


#ifdef __cplusplus
extern "C" {
#endif
bool_t	xdr_rpc_gss_cred	(XDR *xdrs, struct rpc_gss_cred *p);
bool_t	xdr_rpc_gss_init_args	(XDR *xdrs, gss_buffer_desc *p);
bool_t	xdr_rpc_gss_init_res	(XDR *xdrs, struct rpc_gss_init_res *p);
bool_t	xdr_rpc_gss_data	(XDR *xdrs, xdrproc_t xdr_func,
				 caddr_t xdr_ptr, gss_ctx_id_t ctx,
				 gss_qop_t qop, rpc_gss_svc_t svc,
				 u_int seq);

AUTH   *authgss_create		(CLIENT *, gss_name_t, struct rpc_gss_sec *);
AUTH   *authgss_create_default	(CLIENT *, char *, struct rpc_gss_sec *);
bool_t authgss_service		(AUTH *auth, int svc);
bool_t authgss_get_private_data	(AUTH *auth, struct authgss_private_data *);
bool_t authgss_free_private_data (struct authgss_private_data *);

void	gss_log_debug		(const char *fmt, ...);
void	gss_log_status		(char *m, OM_uint32 major, OM_uint32 minor);
void	gss_log_hexdump		(const u_char *buf, int len, int offset);

bool_t	is_authgss_client	(CLIENT *);

#ifdef __cplusplus
}
#endif

#endif 
