

#ifndef _RPCB_PROT_H_RPCGEN
#define _RPCB_PROT_H_RPCGEN

#include <rpc/rpc.h>

#ifndef IXDR_GET_INT32
#define IXDR_GET_INT32(buf) IXDR_GET_LONG((buf))
#endif
#ifndef IXDR_PUT_INT32
#define IXDR_PUT_INT32(buf, v) IXDR_PUT_LONG((buf), (v))
#endif
#ifndef IXDR_GET_U_INT32
#define IXDR_GET_U_INT32(buf) IXDR_GET_U_LONG((buf))
#endif
#ifndef IXDR_PUT_U_INT32
#define IXDR_PUT_U_INT32(buf, v) IXDR_PUT_U_LONG((buf), (v))
#endif






#ifndef _KERNEL







struct rpcb {
	rpcprog_t r_prog;
	rpcvers_t r_vers;
	char *r_netid;
	char *r_addr;
	char *r_owner;
};
typedef struct rpcb rpcb;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcb(XDR *, rpcb*);
#elif __STDC__ 
extern  bool_t xdr_rpcb(XDR *, rpcb*);
#else  
bool_t xdr_rpcb();
#endif  


typedef rpcb RPCB;




struct rp__list {
	rpcb rpcb_map;
	struct rp__list *rpcb_next;
};
typedef struct rp__list rp__list;
#ifdef __cplusplus 
extern "C" bool_t xdr_rp__list(XDR *, rp__list*);
#elif __STDC__ 
extern  bool_t xdr_rp__list(XDR *, rp__list*);
#else  
bool_t xdr_rp__list();
#endif  


typedef rp__list *rpcblist_ptr;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcblist_ptr(XDR *, rpcblist_ptr*);
#elif __STDC__ 
extern  bool_t xdr_rpcblist_ptr(XDR *, rpcblist_ptr*);
#else  
bool_t xdr_rpcblist_ptr();
#endif  


typedef struct rp__list rpcblist;
typedef struct rp__list RPCBLIST;

#ifndef __cplusplus
struct rpcblist {
 RPCB rpcb_map;
 struct rpcblist *rpcb_next;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern bool_t xdr_rpcblist(XDR *, rpcblist**);
#ifdef __cplusplus
}
#endif




struct rpcb_rmtcallargs {
	rpcprog_t prog;
	rpcvers_t vers;
	rpcproc_t proc;
	struct {
		u_int args_len;
		char *args_val;
	} args;
};
typedef struct rpcb_rmtcallargs rpcb_rmtcallargs;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcb_rmtcallargs(XDR *, rpcb_rmtcallargs*);
#elif __STDC__ 
extern  bool_t xdr_rpcb_rmtcallargs(XDR *, rpcb_rmtcallargs*);
#else  
bool_t xdr_rpcb_rmtcallargs();
#endif  



struct r_rpcb_rmtcallargs {
 rpcprog_t prog;
 rpcvers_t vers;
 rpcproc_t proc;
 struct {
 u_int args_len;
 char *args_val;
 } args;
 xdrproc_t xdr_args; 
};




struct rpcb_rmtcallres {
	char *addr;
	struct {
		u_int results_len;
		char *results_val;
	} results;
};
typedef struct rpcb_rmtcallres rpcb_rmtcallres;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcb_rmtcallres(XDR *, rpcb_rmtcallres*);
#elif __STDC__ 
extern  bool_t xdr_rpcb_rmtcallres(XDR *, rpcb_rmtcallres*);
#else  
bool_t xdr_rpcb_rmtcallres();
#endif  



struct r_rpcb_rmtcallres {
 char *addr;
 struct {
 u_int32_t results_len;
 char *results_val;
 } results;
 xdrproc_t xdr_res; 
};



struct rpcb_entry {
	char *r_maddr;
	char *r_nc_netid;
	u_int r_nc_semantics;
	char *r_nc_protofmly;
	char *r_nc_proto;
};
typedef struct rpcb_entry rpcb_entry;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcb_entry(XDR *, rpcb_entry*);
#elif __STDC__ 
extern  bool_t xdr_rpcb_entry(XDR *, rpcb_entry*);
#else  
bool_t xdr_rpcb_entry();
#endif  




struct rpcb_entry_list {
	rpcb_entry rpcb_entry_map;
	struct rpcb_entry_list *rpcb_entry_next;
};
typedef struct rpcb_entry_list rpcb_entry_list;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcb_entry_list(XDR *, rpcb_entry_list*);
#elif __STDC__ 
extern  bool_t xdr_rpcb_entry_list(XDR *, rpcb_entry_list*);
#else  
bool_t xdr_rpcb_entry_list();
#endif  


typedef rpcb_entry_list *rpcb_entry_list_ptr;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcb_entry_list_ptr(XDR *, rpcb_entry_list_ptr*);
#elif __STDC__ 
extern  bool_t xdr_rpcb_entry_list_ptr(XDR *, rpcb_entry_list_ptr*);
#else  
bool_t xdr_rpcb_entry_list_ptr();
#endif  




#define rpcb_highproc_2 RPCBPROC_CALLIT
#define rpcb_highproc_3 RPCBPROC_TADDR2UADDR
#define rpcb_highproc_4 RPCBPROC_GETSTAT
#define RPCBSTAT_HIGHPROC 13
#define RPCBVERS_STAT 3
#define RPCBVERS_4_STAT 2
#define RPCBVERS_3_STAT 1
#define RPCBVERS_2_STAT 0



struct rpcbs_addrlist {
	rpcprog_t prog;
	rpcvers_t vers;
	int success;
	int failure;
	char *netid;
	struct rpcbs_addrlist *next;
};
typedef struct rpcbs_addrlist rpcbs_addrlist;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcbs_addrlist(XDR *, rpcbs_addrlist*);
#elif __STDC__ 
extern  bool_t xdr_rpcbs_addrlist(XDR *, rpcbs_addrlist*);
#else  
bool_t xdr_rpcbs_addrlist();
#endif  




struct rpcbs_rmtcalllist {
	rpcprog_t prog;
	rpcvers_t vers;
	rpcproc_t proc;
	int success;
	int failure;
	int indirect;
	char *netid;
	struct rpcbs_rmtcalllist *next;
};
typedef struct rpcbs_rmtcalllist rpcbs_rmtcalllist;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcbs_rmtcalllist(XDR *, rpcbs_rmtcalllist*);
#elif __STDC__ 
extern  bool_t xdr_rpcbs_rmtcalllist(XDR *, rpcbs_rmtcalllist*);
#else  
bool_t xdr_rpcbs_rmtcalllist();
#endif  


typedef int rpcbs_proc[RPCBSTAT_HIGHPROC];
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcbs_proc(XDR *, rpcbs_proc);
#elif __STDC__ 
extern  bool_t xdr_rpcbs_proc(XDR *, rpcbs_proc);
#else  
bool_t xdr_rpcbs_proc();
#endif  


typedef rpcbs_addrlist *rpcbs_addrlist_ptr;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcbs_addrlist_ptr(XDR *, rpcbs_addrlist_ptr*);
#elif __STDC__ 
extern  bool_t xdr_rpcbs_addrlist_ptr(XDR *, rpcbs_addrlist_ptr*);
#else  
bool_t xdr_rpcbs_addrlist_ptr();
#endif  


typedef rpcbs_rmtcalllist *rpcbs_rmtcalllist_ptr;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcbs_rmtcalllist_ptr(XDR *, rpcbs_rmtcalllist_ptr*);
#elif __STDC__ 
extern  bool_t xdr_rpcbs_rmtcalllist_ptr(XDR *, rpcbs_rmtcalllist_ptr*);
#else  
bool_t xdr_rpcbs_rmtcalllist_ptr();
#endif  


struct rpcb_stat {
	rpcbs_proc info;
	int setinfo;
	int unsetinfo;
	rpcbs_addrlist_ptr addrinfo;
	rpcbs_rmtcalllist_ptr rmtinfo;
};
typedef struct rpcb_stat rpcb_stat;
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcb_stat(XDR *, rpcb_stat*);
#elif __STDC__ 
extern  bool_t xdr_rpcb_stat(XDR *, rpcb_stat*);
#else  
bool_t xdr_rpcb_stat();
#endif  




typedef rpcb_stat rpcb_stat_byvers[RPCBVERS_STAT];
#ifdef __cplusplus 
extern "C" bool_t xdr_rpcb_stat_byvers(XDR *, rpcb_stat_byvers);
#elif __STDC__ 
extern  bool_t xdr_rpcb_stat_byvers(XDR *, rpcb_stat_byvers);
#else  
bool_t xdr_rpcb_stat_byvers();
#endif  



#ifdef __cplusplus
extern "C" bool_t xdr_netbuf(XDR *, struct netbuf *);

#else 
extern bool_t xdr_netbuf(XDR *, struct netbuf *);

#endif

#define RPCBVERS_3 RPCBVERS
#define RPCBVERS_4 RPCBVERS4

#define _PATH_RPCBINDSOCK "/var/run/rpcbind.sock"

#else 
#ifdef __cplusplus
extern "C" {
#endif


struct rpcb {
 rpcprog_t r_prog; 
 rpcvers_t r_vers; 
 char *r_netid; 
 char *r_addr; 
 char *r_owner; 
};
typedef struct rpcb RPCB;


struct rpcblist {
 RPCB rpcb_map;
 struct rpcblist *rpcb_next;
};
typedef struct rpcblist RPCBLIST;
typedef struct rpcblist *rpcblist_ptr;


struct rpcb_rmtcallargs {
 rpcprog_t prog; 
 rpcvers_t vers; 
 rpcproc_t proc; 
 u_int32_t arglen; 
 caddr_t args_ptr; 
 xdrproc_t xdr_args; 
};
typedef struct rpcb_rmtcallargs rpcb_rmtcallargs;


struct rpcb_rmtcallres {
 char *addr_ptr; 
 u_int32_t resultslen; 
 caddr_t results_ptr; 
 xdrproc_t xdr_results; 
};
typedef struct rpcb_rmtcallres rpcb_rmtcallres;

struct rpcb_entry {
 char *r_maddr;
 char *r_nc_netid;
 unsigned int r_nc_semantics;
 char *r_nc_protofmly;
 char *r_nc_proto;
};
typedef struct rpcb_entry rpcb_entry;



struct rpcb_entry_list {
 rpcb_entry rpcb_entry_map;
 struct rpcb_entry_list *rpcb_entry_next;
};
typedef struct rpcb_entry_list rpcb_entry_list;

typedef rpcb_entry_list *rpcb_entry_list_ptr;



#define rpcb_highproc_2 RPCBPROC_CALLIT
#define rpcb_highproc_3 RPCBPROC_TADDR2UADDR
#define rpcb_highproc_4 RPCBPROC_GETSTAT
#define RPCBSTAT_HIGHPROC 13
#define RPCBVERS_STAT 3
#define RPCBVERS_4_STAT 2
#define RPCBVERS_3_STAT 1
#define RPCBVERS_2_STAT 0



struct rpcbs_addrlist {
 rpcprog_t prog;
 rpcvers_t vers;
 int success;
 int failure;
 char *netid;
 struct rpcbs_addrlist *next;
};
typedef struct rpcbs_addrlist rpcbs_addrlist;



struct rpcbs_rmtcalllist {
 rpcprog_t prog;
 rpcvers_t vers;
 rpcproc_t proc;
 int success;
 int failure;
 int indirect;
 char *netid;
 struct rpcbs_rmtcalllist *next;
};
typedef struct rpcbs_rmtcalllist rpcbs_rmtcalllist;

typedef int rpcbs_proc[RPCBSTAT_HIGHPROC];

typedef rpcbs_addrlist *rpcbs_addrlist_ptr;

typedef rpcbs_rmtcalllist *rpcbs_rmtcalllist_ptr;

struct rpcb_stat {
 rpcbs_proc info;
 int setinfo;
 int unsetinfo;
 rpcbs_addrlist_ptr addrinfo;
 rpcbs_rmtcalllist_ptr rmtinfo;
};
typedef struct rpcb_stat rpcb_stat;



typedef rpcb_stat rpcb_stat_byvers[RPCBVERS_STAT];

#ifdef __cplusplus
}
#endif

#endif 

#define RPCBPROG ((u_int32_t)100000)
#define RPCBVERS ((u_int32_t)3)

#ifdef __cplusplus
#define RPCBPROC_SET ((u_int32_t)1)
extern "C" bool_t * rpcbproc_set_3(rpcb *, CLIENT *);
extern "C" bool_t * rpcbproc_set_3_svc(rpcb *, struct svc_req *);
#define RPCBPROC_UNSET ((u_int32_t)2)
extern "C" bool_t * rpcbproc_unset_3(rpcb *, CLIENT *);
extern "C" bool_t * rpcbproc_unset_3_svc(rpcb *, struct svc_req *);
#define RPCBPROC_GETADDR ((u_int32_t)3)
extern "C" char ** rpcbproc_getaddr_3(rpcb *, CLIENT *);
extern "C" char ** rpcbproc_getaddr_3_svc(rpcb *, struct svc_req *);
#define RPCBPROC_DUMP ((u_int32_t)4)
extern "C" rpcblist_ptr * rpcbproc_dump_3(void *, CLIENT *);
extern "C" rpcblist_ptr * rpcbproc_dump_3_svc(void *, struct svc_req *);
#define RPCBPROC_CALLIT ((u_int32_t)5)
extern "C" rpcb_rmtcallres * rpcbproc_callit_3(rpcb_rmtcallargs *, CLIENT *);
extern "C" rpcb_rmtcallres * rpcbproc_callit_3_svc(rpcb_rmtcallargs *, struct svc_req *);
#define RPCBPROC_GETTIME ((u_int32_t)6)
extern "C" u_int * rpcbproc_gettime_3(void *, CLIENT *);
extern "C" u_int * rpcbproc_gettime_3_svc(void *, struct svc_req *);
#define RPCBPROC_UADDR2TADDR ((u_int32_t)7)
extern "C" struct netbuf * rpcbproc_uaddr2taddr_3(char **, CLIENT *);
extern "C" struct netbuf * rpcbproc_uaddr2taddr_3_svc(char **, struct svc_req *);
#define RPCBPROC_TADDR2UADDR ((u_int32_t)8)
extern "C" char ** rpcbproc_taddr2uaddr_3(struct netbuf *, CLIENT *);
extern "C" char ** rpcbproc_taddr2uaddr_3_svc(struct netbuf *, struct svc_req *);

#elif __STDC__
#define RPCBPROC_SET ((u_int32_t)1)
extern  bool_t * rpcbproc_set_3(rpcb *, CLIENT *);
extern  bool_t * rpcbproc_set_3_svc(rpcb *, struct svc_req *);
#define RPCBPROC_UNSET ((u_int32_t)2)
extern  bool_t * rpcbproc_unset_3(rpcb *, CLIENT *);
extern  bool_t * rpcbproc_unset_3_svc(rpcb *, struct svc_req *);
#define RPCBPROC_GETADDR ((u_int32_t)3)
extern  char ** rpcbproc_getaddr_3(rpcb *, CLIENT *);
extern  char ** rpcbproc_getaddr_3_svc(rpcb *, struct svc_req *);
#define RPCBPROC_DUMP ((u_int32_t)4)
extern  rpcblist_ptr * rpcbproc_dump_3(void *, CLIENT *);
extern  rpcblist_ptr * rpcbproc_dump_3_svc(void *, struct svc_req *);
#define RPCBPROC_CALLIT ((u_int32_t)5)
extern  rpcb_rmtcallres * rpcbproc_callit_3(rpcb_rmtcallargs *, CLIENT *);
extern  rpcb_rmtcallres * rpcbproc_callit_3_svc(rpcb_rmtcallargs *, struct svc_req *);
#define RPCBPROC_GETTIME ((u_int32_t)6)
extern  u_int * rpcbproc_gettime_3(void *, CLIENT *);
extern  u_int * rpcbproc_gettime_3_svc(void *, struct svc_req *);
#define RPCBPROC_UADDR2TADDR ((u_int32_t)7)
extern  struct netbuf * rpcbproc_uaddr2taddr_3(char **, CLIENT *);
extern  struct netbuf * rpcbproc_uaddr2taddr_3_svc(char **, struct svc_req *);
#define RPCBPROC_TADDR2UADDR ((u_int32_t)8)
extern  char ** rpcbproc_taddr2uaddr_3(struct netbuf *, CLIENT *);
extern  char ** rpcbproc_taddr2uaddr_3_svc(struct netbuf *, struct svc_req *);

#else  
#define RPCBPROC_SET ((u_int32_t)1)
extern  bool_t * rpcbproc_set_3();
extern  bool_t * rpcbproc_set_3_svc();
#define RPCBPROC_UNSET ((u_int32_t)2)
extern  bool_t * rpcbproc_unset_3();
extern  bool_t * rpcbproc_unset_3_svc();
#define RPCBPROC_GETADDR ((u_int32_t)3)
extern  char ** rpcbproc_getaddr_3();
extern  char ** rpcbproc_getaddr_3_svc();
#define RPCBPROC_DUMP ((u_int32_t)4)
extern  rpcblist_ptr * rpcbproc_dump_3();
extern  rpcblist_ptr * rpcbproc_dump_3_svc();
#define RPCBPROC_CALLIT ((u_int32_t)5)
extern  rpcb_rmtcallres * rpcbproc_callit_3();
extern  rpcb_rmtcallres * rpcbproc_callit_3_svc();
#define RPCBPROC_GETTIME ((u_int32_t)6)
extern  u_int * rpcbproc_gettime_3();
extern  u_int * rpcbproc_gettime_3_svc();
#define RPCBPROC_UADDR2TADDR ((u_int32_t)7)
extern  struct netbuf * rpcbproc_uaddr2taddr_3();
extern  struct netbuf * rpcbproc_uaddr2taddr_3_svc();
#define RPCBPROC_TADDR2UADDR ((u_int32_t)8)
extern  char ** rpcbproc_taddr2uaddr_3();
extern  char ** rpcbproc_taddr2uaddr_3_svc();
#endif  
#define RPCBVERS4 ((u_int32_t)4)

#ifdef __cplusplus
extern "C" bool_t * rpcbproc_set_4(rpcb *, CLIENT *);
extern "C" bool_t * rpcbproc_set_4_svc(rpcb *, struct svc_req *);
extern "C" bool_t * rpcbproc_unset_4(rpcb *, CLIENT *);
extern "C" bool_t * rpcbproc_unset_4_svc(rpcb *, struct svc_req *);
extern "C" char ** rpcbproc_getaddr_4(rpcb *, CLIENT *);
extern "C" char ** rpcbproc_getaddr_4_svc(rpcb *, struct svc_req *);
extern "C" rpcblist_ptr * rpcbproc_dump_4(void *, CLIENT *);
extern "C" rpcblist_ptr * rpcbproc_dump_4_svc(void *, struct svc_req *);
#define RPCBPROC_BCAST ((u_int32_t)RPCBPROC_CALLIT)
extern "C" rpcb_rmtcallres * rpcbproc_bcast_4(rpcb_rmtcallargs *, CLIENT *);
extern "C" rpcb_rmtcallres * rpcbproc_bcast_4_svc(rpcb_rmtcallargs *, struct svc_req *);
extern "C" u_int * rpcbproc_gettime_4(void *, CLIENT *);
extern "C" u_int * rpcbproc_gettime_4_svc(void *, struct svc_req *);
extern "C" struct netbuf * rpcbproc_uaddr2taddr_4(char **, CLIENT *);
extern "C" struct netbuf * rpcbproc_uaddr2taddr_4_svc(char **, struct svc_req *);
extern "C" char ** rpcbproc_taddr2uaddr_4(struct netbuf *, CLIENT *);
extern "C" char ** rpcbproc_taddr2uaddr_4_svc(struct netbuf *, struct svc_req *);
#define RPCBPROC_GETVERSADDR ((u_int32_t)9)
extern "C" char ** rpcbproc_getversaddr_4(rpcb *, CLIENT *);
extern "C" char ** rpcbproc_getversaddr_4_svc(rpcb *, struct svc_req *);
#define RPCBPROC_INDIRECT ((u_int32_t)10)
extern "C" rpcb_rmtcallres * rpcbproc_indirect_4(rpcb_rmtcallargs *, CLIENT *);
extern "C" rpcb_rmtcallres * rpcbproc_indirect_4_svc(rpcb_rmtcallargs *, struct svc_req *);
#define RPCBPROC_GETADDRLIST ((u_int32_t)11)
extern "C" rpcb_entry_list_ptr * rpcbproc_getaddrlist_4(rpcb *, CLIENT *);
extern "C" rpcb_entry_list_ptr * rpcbproc_getaddrlist_4_svc(rpcb *, struct svc_req *);
#define RPCBPROC_GETSTAT ((u_int32_t)12)
extern "C" rpcb_stat * rpcbproc_getstat_4(void *, CLIENT *);
extern "C" rpcb_stat * rpcbproc_getstat_4_svc(void *, struct svc_req *);

#elif __STDC__
extern  bool_t * rpcbproc_set_4(rpcb *, CLIENT *);
extern  bool_t * rpcbproc_set_4_svc(rpcb *, struct svc_req *);
extern  bool_t * rpcbproc_unset_4(rpcb *, CLIENT *);
extern  bool_t * rpcbproc_unset_4_svc(rpcb *, struct svc_req *);
extern  char ** rpcbproc_getaddr_4(rpcb *, CLIENT *);
extern  char ** rpcbproc_getaddr_4_svc(rpcb *, struct svc_req *);
extern  rpcblist_ptr * rpcbproc_dump_4(void *, CLIENT *);
extern  rpcblist_ptr * rpcbproc_dump_4_svc(void *, struct svc_req *);
#define RPCBPROC_BCAST ((u_int32_t)RPCBPROC_CALLIT)
extern  rpcb_rmtcallres * rpcbproc_bcast_4(rpcb_rmtcallargs *, CLIENT *);
extern  rpcb_rmtcallres * rpcbproc_bcast_4_svc(rpcb_rmtcallargs *, struct svc_req *);
extern  u_int * rpcbproc_gettime_4(void *, CLIENT *);
extern  u_int * rpcbproc_gettime_4_svc(void *, struct svc_req *);
extern  struct netbuf * rpcbproc_uaddr2taddr_4(char **, CLIENT *);
extern  struct netbuf * rpcbproc_uaddr2taddr_4_svc(char **, struct svc_req *);
extern  char ** rpcbproc_taddr2uaddr_4(struct netbuf *, CLIENT *);
extern  char ** rpcbproc_taddr2uaddr_4_svc(struct netbuf *, struct svc_req *);
#define RPCBPROC_GETVERSADDR ((u_int32_t)9)
extern  char ** rpcbproc_getversaddr_4(rpcb *, CLIENT *);
extern  char ** rpcbproc_getversaddr_4_svc(rpcb *, struct svc_req *);
#define RPCBPROC_INDIRECT ((u_int32_t)10)
extern  rpcb_rmtcallres * rpcbproc_indirect_4(rpcb_rmtcallargs *, CLIENT *);
extern  rpcb_rmtcallres * rpcbproc_indirect_4_svc(rpcb_rmtcallargs *, struct svc_req *);
#define RPCBPROC_GETADDRLIST ((u_int32_t)11)
extern  rpcb_entry_list_ptr * rpcbproc_getaddrlist_4(rpcb *, CLIENT *);
extern  rpcb_entry_list_ptr * rpcbproc_getaddrlist_4_svc(rpcb *, struct svc_req *);
#define RPCBPROC_GETSTAT ((u_int32_t)12)
extern  rpcb_stat * rpcbproc_getstat_4(void *, CLIENT *);
extern  rpcb_stat * rpcbproc_getstat_4_svc(void *, struct svc_req *);

#else  
extern  bool_t * rpcbproc_set_4();
extern  bool_t * rpcbproc_set_4_svc();
extern  bool_t * rpcbproc_unset_4();
extern  bool_t * rpcbproc_unset_4_svc();
extern  char ** rpcbproc_getaddr_4();
extern  char ** rpcbproc_getaddr_4_svc();
extern  rpcblist_ptr * rpcbproc_dump_4();
extern  rpcblist_ptr * rpcbproc_dump_4_svc();
#define RPCBPROC_BCAST ((u_int32_t)RPCBPROC_CALLIT)
extern  rpcb_rmtcallres * rpcbproc_bcast_4();
extern  rpcb_rmtcallres * rpcbproc_bcast_4_svc();
extern  u_int * rpcbproc_gettime_4();
extern  u_int * rpcbproc_gettime_4_svc();
extern  struct netbuf * rpcbproc_uaddr2taddr_4();
extern  struct netbuf * rpcbproc_uaddr2taddr_4_svc();
extern  char ** rpcbproc_taddr2uaddr_4();
extern  char ** rpcbproc_taddr2uaddr_4_svc();
#define RPCBPROC_GETVERSADDR ((u_int32_t)9)
extern  char ** rpcbproc_getversaddr_4();
extern  char ** rpcbproc_getversaddr_4_svc();
#define RPCBPROC_INDIRECT ((u_int32_t)10)
extern  rpcb_rmtcallres * rpcbproc_indirect_4();
extern  rpcb_rmtcallres * rpcbproc_indirect_4_svc();
#define RPCBPROC_GETADDRLIST ((u_int32_t)11)
extern  rpcb_entry_list_ptr * rpcbproc_getaddrlist_4();
extern  rpcb_entry_list_ptr * rpcbproc_getaddrlist_4_svc();
#define RPCBPROC_GETSTAT ((u_int32_t)12)
extern  rpcb_stat * rpcbproc_getstat_4();
extern  rpcb_stat * rpcbproc_getstat_4_svc();
#endif  

#endif 
