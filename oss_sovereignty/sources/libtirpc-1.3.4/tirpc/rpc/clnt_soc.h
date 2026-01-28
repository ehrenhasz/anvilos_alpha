







#ifndef _RPC_CLNT_SOC_H
#define _RPC_CLNT_SOC_H






#define UDPMSGSIZE      8800      


#ifdef __cplusplus
extern "C" {
#endif
extern CLIENT *clnttcp_create(struct sockaddr_in *, u_long, u_long, int *,
			      u_int, u_int);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern CLIENT *clntraw_create(u_long, u_long);
#ifdef __cplusplus
}
#endif



#ifdef INET6
#ifdef __cplusplus
extern "C" {
#endif
extern CLIENT *clnttcp6_create(struct sockaddr_in6 *, u_long, u_long, int *,
			      u_int, u_int);
#ifdef __cplusplus
}
#endif
#endif


#ifdef __cplusplus
extern "C" {
#endif
extern CLIENT *clntudp_create(struct sockaddr_in *, u_long, u_long, 
			      struct timeval, int *);
extern CLIENT *clntudp_bufcreate(struct sockaddr_in *, u_long, u_long,
				 struct timeval, int *, u_int, u_int);
#ifdef INET6
extern CLIENT *clntudp6_create(struct sockaddr_in6 *, u_long, u_long, 
			      struct timeval, int *);
extern CLIENT *clntudp6_bufcreate(struct sockaddr_in6 *, u_long, u_long,
				 struct timeval, int *, u_int, u_int);
#endif
#ifdef __cplusplus
}
#endif


#endif 
