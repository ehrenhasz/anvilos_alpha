



#ifndef _TIRPC_SVC_MT_H
#define _TIRPC_SVC_MT_H

typedef struct __rpc_svcxprt_ext {
	int 		flags;
	SVCAUTH		xp_auth;
} SVCXPRT_EXT;


#define SVCEXT(xprt)					\
	((SVCXPRT_EXT *)(xprt)->xp_p3)

#define SVC_XP_AUTH(xprt)				\
	(SVCEXT(xprt)->xp_auth)

#define SVC_VERSQUIET 0x0001	

#define svc_flags(xprt)					\
	(SVCEXT(xprt)->flags)

#define version_keepquiet(xprt)				\
	(svc_flags(xprt) & SVC_VERSQUIET)

#endif 
