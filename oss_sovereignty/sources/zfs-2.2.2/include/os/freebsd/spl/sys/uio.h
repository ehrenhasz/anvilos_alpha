

#ifndef _OPENSOLARIS_SYS_UIO_H_
#define	_OPENSOLARIS_SYS_UIO_H_

#ifndef _STANDALONE

#include_next <sys/uio.h>
#include <sys/_uio.h>
#include <sys/debug.h>

typedef	struct iovec	iovec_t;
typedef	enum uio_seg	zfs_uio_seg_t;
typedef	enum uio_rw	zfs_uio_rw_t;

typedef struct zfs_uio {
	struct uio	*uio;
} zfs_uio_t;

#define	GET_UIO_STRUCT(u)	(u)->uio
#define	zfs_uio_segflg(u)	GET_UIO_STRUCT(u)->uio_segflg
#define	zfs_uio_offset(u)	GET_UIO_STRUCT(u)->uio_offset
#define	zfs_uio_resid(u)	GET_UIO_STRUCT(u)->uio_resid
#define	zfs_uio_iovcnt(u)	GET_UIO_STRUCT(u)->uio_iovcnt
#define	zfs_uio_iovlen(u, idx)	GET_UIO_STRUCT(u)->uio_iov[(idx)].iov_len
#define	zfs_uio_iovbase(u, idx)	GET_UIO_STRUCT(u)->uio_iov[(idx)].iov_base
#define	zfs_uio_td(u)		GET_UIO_STRUCT(u)->uio_td
#define	zfs_uio_rw(u)		GET_UIO_STRUCT(u)->uio_rw
#define	zfs_uio_fault_disable(u, set)
#define	zfs_uio_prefaultpages(size, u)	(0)

static inline void
zfs_uio_setoffset(zfs_uio_t *uio, offset_t off)
{
	zfs_uio_offset(uio) = off;
}

static inline void
zfs_uio_advance(zfs_uio_t *uio, size_t size)
{
	zfs_uio_resid(uio) -= size;
	zfs_uio_offset(uio) += size;
}

static __inline void
zfs_uio_init(zfs_uio_t *uio, struct uio *uio_s)
{
	GET_UIO_STRUCT(uio) = uio_s;
}

int zfs_uio_fault_move(void *p, size_t n, zfs_uio_rw_t dir, zfs_uio_t *uio);

#endif 

#endif	
