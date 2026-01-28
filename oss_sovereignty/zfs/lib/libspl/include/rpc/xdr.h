#ifndef LIBSPL_RPC_XDR_H
#define	LIBSPL_RPC_XDR_H
#include_next <rpc/xdr.h>
#ifdef xdr_control  
#undef xdr_control
#endif
#define	XDR_GET_BYTES_AVAIL 1
#ifndef HAVE_XDR_BYTESREC
struct xdr_bytesrec {
	bool_t xc_is_last_record;
	size_t xc_num_avail;
};
#endif
typedef struct xdr_bytesrec  xdr_bytesrec_t;
static inline bool_t
xdr_control(XDR *xdrs, int request, void *info)
{
	xdr_bytesrec_t *xptr;
	ASSERT3U(request, ==, XDR_GET_BYTES_AVAIL);
	xptr = (xdr_bytesrec_t *)info;
	xptr->xc_is_last_record = TRUE;
	xptr->xc_num_avail = xdrs->x_handy;
	return (TRUE);
}
#endif  
