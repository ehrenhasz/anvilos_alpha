







#ifndef _RPC_SVC_SOC_H
#define _RPC_SVC_SOC_H







#define svc_getcaller(x) (&(x)->xp_raddr)
 
#define svc_getcaller_netbuf(x) (&(x)->xp_rtaddr)

#ifdef __cplusplus
extern "C" {
#endif
extern bool_t	svc_register(SVCXPRT *, u_long, u_long,
		    void (*)(struct svc_req *, SVCXPRT *), int);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern void	svc_unregister(u_long, u_long);
#ifdef __cplusplus
}
#endif



#ifdef __cplusplus
extern "C" {
#endif
extern SVCXPRT *svcraw_create(void);
#ifdef __cplusplus
}
#endif



#ifdef __cplusplus
extern "C" {
#endif
extern SVCXPRT *svcudp_create(int);
extern SVCXPRT *svcudp_bufcreate(int, u_int, u_int);
extern int svcudp_enablecache(SVCXPRT *, u_long);
extern SVCXPRT *svcudp6_create(int);
extern SVCXPRT *svcudp6_bufcreate(int, u_int, u_int);
#ifdef __cplusplus
}
#endif



#ifdef __cplusplus
extern "C" {
#endif
extern SVCXPRT *svctcp_create(int, u_int, u_int);
extern SVCXPRT *svctcp6_create(int, u_int, u_int);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern SVCXPRT *svcfd_create(int, u_int, u_int);
#ifdef __cplusplus
}
#endif

#endif 
