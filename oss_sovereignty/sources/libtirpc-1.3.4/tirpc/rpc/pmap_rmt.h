





#ifndef _RPC_PMAP_RMT_H
#define _RPC_PMAP_RMT_H

struct rmtcallargs {
	u_long prog, vers, proc, arglen;
	caddr_t args_ptr;
	xdrproc_t xdr_args;
};

struct rmtcallres {
	u_long *port_ptr;
	u_long resultslen;
	caddr_t results_ptr;
	xdrproc_t xdr_results;
};

#ifdef __cplusplus
extern "C" {
#endif
extern bool_t xdr_rmtcall_args(XDR *, struct rmtcallargs *);
extern bool_t xdr_rmtcallres(XDR *, struct rmtcallres *);
#ifdef __cplusplus
}
#endif

#endif 
