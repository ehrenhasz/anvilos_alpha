





#ifndef _RPC_PMAP_PROT_H
#define _RPC_PMAP_PROT_H

#define PMAPPORT		((u_short)111)
#define PMAPPROG		((u_long)100000)
#define PMAPVERS		((u_long)2)
#define PMAPVERS_PROTO		((u_long)2)
#define PMAPVERS_ORIG		((u_long)1)
#define PMAPPROC_NULL		((u_long)0)
#define PMAPPROC_SET		((u_long)1)
#define PMAPPROC_UNSET		((u_long)2)
#define PMAPPROC_GETPORT	((u_long)3)
#define PMAPPROC_DUMP		((u_long)4)
#define PMAPPROC_CALLIT		((u_long)5)

#define V2FIRST		"RPCB_V2FIRST"

struct pmap {
	long unsigned pm_prog;
	long unsigned pm_vers;
	long unsigned pm_prot;
	long unsigned pm_port;
};

struct pmaplist {
	struct pmap	pml_map;
	struct pmaplist *pml_next;
};

#ifdef __cplusplus
extern "C" {
#endif
extern bool_t xdr_pmap(XDR *, struct pmap *);
extern bool_t xdr_pmaplist(XDR *, struct pmaplist **);
extern bool_t xdr_pmaplist_ptr(XDR *, struct pmaplist *);
#ifdef __cplusplus
}
#endif

#endif 
