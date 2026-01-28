





#ifndef _TIRPC_XDR_H
#define _TIRPC_XDR_H
#include <stdio.h>
#include <netinet/in.h>

#include <rpc/types.h>




enum xdr_op {
	XDR_ENCODE=0,
	XDR_DECODE=1,
	XDR_FREE=2
};


#define BYTES_PER_XDR_UNIT	(4)
#define RNDUP(x)  ((((x) + BYTES_PER_XDR_UNIT - 1) / BYTES_PER_XDR_UNIT) \
		    * BYTES_PER_XDR_UNIT)


typedef struct __rpc_xdr {
	enum xdr_op	x_op;		
	const struct xdr_ops {
		
		bool_t	(*x_getlong)(struct __rpc_xdr *, long *);
		
		bool_t	(*x_putlong)(struct __rpc_xdr *, const long *);
		
		bool_t	(*x_getbytes)(struct __rpc_xdr *, char *, u_int);
		
		bool_t	(*x_putbytes)(struct __rpc_xdr *, const char *, u_int);
		
		u_int	(*x_getpostn)(struct __rpc_xdr *);
		
		bool_t  (*x_setpostn)(struct __rpc_xdr *, u_int);
		
		int32_t *(*x_inline)(struct __rpc_xdr *, u_int);
		
		void	(*x_destroy)(struct __rpc_xdr *);
		bool_t	(*x_control)(struct __rpc_xdr *, int, void *);
	} *x_ops;
	char *	 	x_public;	
	void *		x_private;	
	char * 		x_base;		
	u_int		x_handy;	
} XDR;


#ifdef _KERNEL
typedef	bool_t (*xdrproc_t)(XDR *, void *, u_int);
#else

typedef	bool_t (*xdrproc_t)(XDR *, ...);
#endif


#define XDR_GETLONG(xdrs, longp)			\
	(*(xdrs)->x_ops->x_getlong)(xdrs, longp)
#define xdr_getlong(xdrs, longp)			\
	(*(xdrs)->x_ops->x_getlong)(xdrs, longp)

#define XDR_PUTLONG(xdrs, longp)			\
	(*(xdrs)->x_ops->x_putlong)(xdrs, longp)
#define xdr_putlong(xdrs, longp)			\
	(*(xdrs)->x_ops->x_putlong)(xdrs, longp)

static __inline int
xdr_getint32(XDR *xdrs, int32_t *ip)
{
	long l;

	if (!xdr_getlong(xdrs, &l))
		return (FALSE);
	*ip = (int32_t)l;
	return (TRUE);
}

static __inline int
xdr_putint32(XDR *xdrs, int32_t *ip)
{
	long l;

	l = (long)*ip;
	return xdr_putlong(xdrs, &l);
}

#define XDR_GETINT32(xdrs, int32p)	xdr_getint32(xdrs, int32p)
#define XDR_PUTINT32(xdrs, int32p)	xdr_putint32(xdrs, int32p)

#define XDR_GETBYTES(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_getbytes)(xdrs, addr, len)
#define xdr_getbytes(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_getbytes)(xdrs, addr, len)

#define XDR_PUTBYTES(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_putbytes)(xdrs, addr, len)
#define xdr_putbytes(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_putbytes)(xdrs, addr, len)

#define XDR_GETPOS(xdrs)				\
	(*(xdrs)->x_ops->x_getpostn)(xdrs)
#define xdr_getpos(xdrs)				\
	(*(xdrs)->x_ops->x_getpostn)(xdrs)

#define XDR_SETPOS(xdrs, pos)				\
	(*(xdrs)->x_ops->x_setpostn)(xdrs, pos)
#define xdr_setpos(xdrs, pos)				\
	(*(xdrs)->x_ops->x_setpostn)(xdrs, pos)

#define	XDR_INLINE(xdrs, len)				\
	(*(xdrs)->x_ops->x_inline)(xdrs, len)
#define	xdr_inline(xdrs, len)				\
	(*(xdrs)->x_ops->x_inline)(xdrs, len)

#define	XDR_DESTROY(xdrs)				\
	if ((xdrs)->x_ops->x_destroy) 			\
		(*(xdrs)->x_ops->x_destroy)(xdrs)
#define	xdr_destroy(xdrs)				\
	if ((xdrs)->x_ops->x_destroy) 			\
		(*(xdrs)->x_ops->x_destroy)(xdrs)

#define XDR_CONTROL(xdrs, req, op)			\
	if ((xdrs)->x_ops->x_control)			\
		(*(xdrs)->x_ops->x_control)(xdrs, req, op)
#define xdr_control(xdrs, req, op) XDR_CONTROL(xdrs, req, op)

#define xdr_rpcvers(xdrs, versp) xdr_u_int32_t(xdrs, versp)
#define xdr_rpcprog(xdrs, progp) xdr_u_int32_t(xdrs, progp)
#define xdr_rpcproc(xdrs, procp) xdr_u_int32_t(xdrs, procp)
#define xdr_rpcprot(xdrs, protp) xdr_u_int32_t(xdrs, protp)
#define xdr_rpcport(xdrs, portp) xdr_u_int32_t(xdrs, portp)


#define NULL_xdrproc_t ((xdrproc_t)0)
struct xdr_discrim {
	int	value;
	xdrproc_t proc;
};


#define IXDR_GET_INT32(buf)		((int32_t)ntohl((u_int32_t)*(buf)++))
#define IXDR_PUT_INT32(buf, v)		(*(buf)++ =(int32_t)htonl((u_int32_t)v))
#define IXDR_GET_U_INT32(buf)		((u_int32_t)IXDR_GET_INT32(buf))
#define IXDR_PUT_U_INT32(buf, v)	IXDR_PUT_INT32((buf), ((int32_t)(v)))

#define IXDR_GET_LONG(buf)		((long)ntohl((u_int32_t)*(buf)++))
#define IXDR_PUT_LONG(buf, v)		(*(buf)++ =(int32_t)htonl((u_int32_t)v))

#define IXDR_GET_BOOL(buf)		((bool_t)IXDR_GET_LONG(buf))
#define IXDR_GET_ENUM(buf, t)		((t)IXDR_GET_LONG(buf))
#define IXDR_GET_U_LONG(buf)		((u_long)IXDR_GET_LONG(buf))
#define IXDR_GET_SHORT(buf)		((short)IXDR_GET_LONG(buf))
#define IXDR_GET_U_SHORT(buf)		((u_short)IXDR_GET_LONG(buf))

#define IXDR_PUT_BOOL(buf, v)		IXDR_PUT_LONG((buf), (v))
#define IXDR_PUT_ENUM(buf, v)		IXDR_PUT_LONG((buf), (v))
#define IXDR_PUT_U_LONG(buf, v)		IXDR_PUT_LONG((buf), (v))
#define IXDR_PUT_SHORT(buf, v)		IXDR_PUT_LONG((buf), (v))
#define IXDR_PUT_U_SHORT(buf, v)	IXDR_PUT_LONG((buf), (v))


#ifdef __cplusplus
extern "C" {
#endif
extern bool_t	xdr_void(void);
extern bool_t	xdr_int(XDR *, int *);
extern bool_t	xdr_u_int(XDR *, u_int *);
extern bool_t	xdr_long(XDR *, long *);
extern bool_t	xdr_u_long(XDR *, u_long *);
extern bool_t	xdr_short(XDR *, short *);
extern bool_t	xdr_u_short(XDR *, u_short *);
extern bool_t	xdr_int8_t(XDR *, int8_t *);
extern bool_t	xdr_u_int8_t(XDR *, uint8_t *);
extern bool_t	xdr_uint8_t(XDR *, uint8_t *);
extern bool_t	xdr_int16_t(XDR *, int16_t *);
extern bool_t	xdr_u_int16_t(XDR *, u_int16_t *);
extern bool_t	xdr_uint16_t(XDR *, uint16_t *);
extern bool_t	xdr_int32_t(XDR *, int32_t *);
extern bool_t	xdr_u_int32_t(XDR *, u_int32_t *);
extern bool_t	xdr_uint32_t(XDR *, uint32_t *);
extern bool_t	xdr_int64_t(XDR *, int64_t *);
extern bool_t	xdr_u_int64_t(XDR *, u_int64_t *);
extern bool_t	xdr_uint64_t(XDR *, uint64_t *);
extern bool_t	xdr_quad_t(XDR *, int64_t *);
extern bool_t	xdr_u_quad_t(XDR *, u_int64_t *);
extern bool_t	xdr_bool(XDR *, bool_t *);
extern bool_t	xdr_enum(XDR *, enum_t *);
extern bool_t	xdr_array(XDR *, char **, u_int *, u_int, u_int, xdrproc_t);
extern bool_t	xdr_bytes(XDR *, char **, u_int *, u_int);
extern bool_t	xdr_opaque(XDR *, char *, u_int);
extern bool_t	xdr_string(XDR *, char **, u_int);
extern bool_t	xdr_union(XDR *, enum_t *, char *, const struct xdr_discrim *, xdrproc_t);
extern bool_t	xdr_char(XDR *, char *);
extern bool_t	xdr_u_char(XDR *, u_char *);
extern bool_t	xdr_vector(XDR *, char *, u_int, u_int, xdrproc_t);
extern bool_t	xdr_float(XDR *, float *);
extern bool_t	xdr_double(XDR *, double *);
extern bool_t	xdr_quadruple(XDR *, long double *);
extern bool_t	xdr_reference(XDR *, char **, u_int, xdrproc_t);
extern bool_t	xdr_pointer(XDR *, char **, u_int, xdrproc_t);
extern bool_t	xdr_wrapstring(XDR *, char **);
extern void	xdr_free(xdrproc_t, void *);
extern bool_t	xdr_hyper(XDR *, quad_t *);
extern bool_t	xdr_u_hyper(XDR *, u_quad_t *);
extern bool_t	xdr_longlong_t(XDR *, quad_t *);
extern bool_t	xdr_u_longlong_t(XDR *, u_quad_t *);
extern u_long	xdr_sizeof(xdrproc_t, void *);
#ifdef __cplusplus
}
#endif


#define MAX_NETOBJ_SZ 1024
struct netobj {
	u_int	n_len;
	char	*n_bytes;
};
typedef struct netobj netobj;
extern bool_t   xdr_netobj(XDR *, struct netobj *);


#ifdef __cplusplus
extern "C" {
#endif

extern void   xdrmem_create(XDR *, char *, u_int, enum xdr_op);


extern void   xdrstdio_create(XDR *, FILE *, enum xdr_op);


extern void   xdrrec_create(XDR *, u_int, u_int, void *,
			    int (*)(void *, void *, int),
			    int (*)(void *, void *, int));


extern bool_t xdrrec_endofrecord(XDR *, int);


extern bool_t xdrrec_skiprecord(XDR *);


extern bool_t xdrrec_eof(XDR *);
extern u_int xdrrec_readbytes(XDR *, caddr_t, u_int);
#ifdef __cplusplus
}
#endif

#endif 
