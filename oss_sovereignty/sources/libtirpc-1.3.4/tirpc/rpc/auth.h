





#ifndef _TIRPC_AUTH_H
#define  _TIRPC_AUTH_H

#include <rpc/xdr.h>
#include <rpc/clnt_stat.h>
#include <sys/socket.h>
#include <sys/types.h>


#define MAX_AUTH_BYTES	400
#define MAXNETNAMELEN	255	



typedef struct sec_data {
	u_int	secmod;		
	u_int	rpcflavor;	
	int	flags;		
	caddr_t data;		
} sec_data_t;

#ifdef _SYSCALL32_IMPL
struct sec_data32 {
	uint32_t secmod;	
	uint32_t rpcflavor;	
	int32_t flags;		
	caddr32_t data;		
};
#endif 


typedef struct des_clnt_data {
	struct netbuf	syncaddr;	
	struct knetconfig *knconf;	
					
	char		*netname;	
	int		netnamelen;	
} dh_k4_clntdata_t;

#ifdef _SYSCALL32_IMPL
struct des_clnt_data32 {
	struct netbuf32 syncaddr;	
	caddr32_t knconf;		
					
	caddr32_t netname;		
	int32_t netnamelen;		
};
#endif 


#define AUTH_F_RPCTIMESYNC	0x001	
#define AUTH_F_TRYNONE		0x002	



enum auth_stat {
	AUTH_OK=0,
	
	AUTH_BADCRED=1,			
	AUTH_REJECTEDCRED=2,		
	AUTH_BADVERF=3,			
	AUTH_REJECTEDVERF=4,		
	AUTH_TOOWEAK=5,			
	
	AUTH_INVALIDRESP=6,		
	AUTH_FAILED=7,			
	
	AUTH_KERB_GENERIC = 8,		
	AUTH_TIMEEXPIRE = 9,		
	AUTH_TKT_FILE = 10,		
	AUTH_DECODE = 11,		
	AUTH_NET_ADDR = 12,		
	
	RPCSEC_GSS_CREDPROBLEM = 13,
	RPCSEC_GSS_CTXPROBLEM = 14

};

typedef u_int32_t u_int32;	

union des_block {
	struct {
		u_int32_t high;
		u_int32_t low;
	} key;
	char c[8];
};
typedef union des_block des_block;
#ifdef __cplusplus
extern "C" {
#endif
extern bool_t xdr_des_block(XDR *, des_block *);
#ifdef __cplusplus
}
#endif


struct opaque_auth {
	enum_t	oa_flavor;		
	caddr_t	oa_base;		
	u_int	oa_length;		
};



typedef struct __auth {
	struct	opaque_auth	ah_cred;
	struct	opaque_auth	ah_verf;
	union	des_block	ah_key;
	struct auth_ops {
		void	(*ah_nextverf) (struct __auth *);
		
		int	(*ah_marshal) (struct __auth *, XDR *);
		
		int	(*ah_validate) (struct __auth *,
			    struct opaque_auth *);
		
		int	(*ah_refresh) (struct __auth *, void *);
		
		void	(*ah_destroy) (struct __auth *);
		
		int     (*ah_wrap) (struct __auth *, XDR *, xdrproc_t, caddr_t);
		
		int     (*ah_unwrap) (struct __auth *, XDR *, xdrproc_t, caddr_t);

	} *ah_ops;
	void *ah_private;
} AUTH;


#define AUTH_NEXTVERF(auth)		\
		((*((auth)->ah_ops->ah_nextverf))(auth))
#define auth_nextverf(auth)		\
		((*((auth)->ah_ops->ah_nextverf))(auth))

#define AUTH_MARSHALL(auth, xdrs)	\
		((*((auth)->ah_ops->ah_marshal))(auth, xdrs))
#define auth_marshall(auth, xdrs)	\
		((*((auth)->ah_ops->ah_marshal))(auth, xdrs))

#define AUTH_VALIDATE(auth, verfp)	\
		((*((auth)->ah_ops->ah_validate))((auth), verfp))
#define auth_validate(auth, verfp)	\
		((*((auth)->ah_ops->ah_validate))((auth), verfp))

#define AUTH_REFRESH(auth, msg)		\
		((*((auth)->ah_ops->ah_refresh))(auth, msg))
#define auth_refresh(auth, msg)		\
		((*((auth)->ah_ops->ah_refresh))(auth, msg))

#define AUTH_DESTROY(auth)		\
		((*((auth)->ah_ops->ah_destroy))(auth));
#define auth_destroy(auth)		\
		((*((auth)->ah_ops->ah_destroy))(auth));

#define AUTH_WRAP(auth, xdrs, xfunc, xwhere)            \
		((*((auth)->ah_ops->ah_wrap))(auth, xdrs, \
		xfunc, xwhere))
#define auth_wrap(auth, xdrs, xfunc, xwhere)            \
		((*((auth)->ah_ops->ah_wrap))(auth, xdrs, \
		xfunc, xwhere))

#define AUTH_UNWRAP(auth, xdrs, xfunc, xwhere)          \
		((*((auth)->ah_ops->ah_unwrap))(auth, xdrs, \
		xfunc, xwhere))
#define auth_unwrap(auth, xdrs, xfunc, xwhere)          \
		((*((auth)->ah_ops->ah_unwrap))(auth, xdrs, \
		xfunc, xwhere))


#ifdef __cplusplus
extern "C" {
#endif
extern struct opaque_auth _null_auth;
#ifdef __cplusplus
}
#endif


int authany_wrap(void), authany_unwrap(void);




#ifdef __cplusplus
extern "C" {
#endif
extern AUTH *authunix_create(char *, uid_t, uid_t, int, uid_t *);
extern AUTH *authunix_create_default(void);	
extern AUTH *authnone_create(void);		
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern AUTH *authdes_create (char *, u_int, struct sockaddr *, des_block *);
extern AUTH *authdes_pk_create (char *, netobj *, u_int,
				struct sockaddr *, des_block *);
extern AUTH *authdes_seccreate (const char *, const u_int, const  char *,
    const  des_block *);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern bool_t xdr_opaque_auth		(XDR *, struct opaque_auth *);
#ifdef __cplusplus
}
#endif

#define authsys_create(c,i1,i2,i3,ip) authunix_create((c),(i1),(i2),(i3),(ip))
#define authsys_create_default() authunix_create_default()


#ifdef __cplusplus
extern "C" {
#endif
extern int getnetname(char *);
extern int host2netname(char *, const char *, const char *);
extern int user2netname(char *, const uid_t, const char *);
extern int netname2user(char *, uid_t *, gid_t *, int *, gid_t *);
extern int netname2host(char *, char *, const int);
extern void passwd2des ( char *, char * );
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern int key_decryptsession(const char *, des_block *);
extern int key_encryptsession(const char *, des_block *);
extern int key_gendes(des_block *);
extern int key_setsecret(const char *);
extern int key_secretkey_is_set(void);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern int getpublickey (const char *, char *);
extern int getpublicandprivatekey (char *, char *);
extern int getsecretkey (char *, char *, char *);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
struct svc_req;
struct rpc_msg;
enum auth_stat _svcauth_none (struct svc_req *, struct rpc_msg *);
enum auth_stat _svcauth_short (struct svc_req *, struct rpc_msg *);
enum auth_stat _svcauth_unix (struct svc_req *, struct rpc_msg *);
enum auth_stat _svcauth_gss (struct svc_req *, struct rpc_msg *, bool_t *);
#ifdef __cplusplus
}
#endif

#define AUTH_NONE	0		
#define	AUTH_NULL	0		
#define	AUTH_SYS	1		
#define AUTH_UNIX	AUTH_SYS
#define	AUTH_SHORT	2		
#define AUTH_DH		3		
#define AUTH_DES	AUTH_DH		
#define AUTH_KERB	4		
#define RPCSEC_GSS	6		

#endif 
