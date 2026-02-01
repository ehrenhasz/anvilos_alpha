 

#ifndef	_OPENSOLARIS_RPC_XDR_H_
#define	_OPENSOLARIS_RPC_XDR_H_

#include <rpc/types.h>
#include_next <rpc/xdr.h>

#if !defined(_KERNEL) && !defined(_STANDALONE)

#include <assert.h>

 
static __inline bool_t
xdrmem_control(XDR *xdrs, int request, void *info)
{
	xdr_bytesrec *xptr;

	switch (request) {
	case XDR_GET_BYTES_AVAIL:
		xptr = (xdr_bytesrec *)info;
		xptr->xc_is_last_record = TRUE;
		xptr->xc_num_avail = xdrs->x_handy;
		return (TRUE);
	default:
		assert(!"unexpected request");
	}
	return (FALSE);
}

#undef XDR_CONTROL
#define	XDR_CONTROL(xdrs, req, op)					\
	(((xdrs)->x_ops->x_control == NULL) ?				\
	    xdrmem_control((xdrs), (req), (op)) :			\
	    (*(xdrs)->x_ops->x_control)(xdrs, req, op))

#endif	 

#endif	 
