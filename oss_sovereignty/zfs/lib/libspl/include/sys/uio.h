#ifndef	_LIBSPL_SYS_UIO_H
#define	_LIBSPL_SYS_UIO_H
#include <sys/types.h>
#include_next <sys/uio.h>
#ifdef __APPLE__
#include <sys/_types/_iovec_t.h>
#endif
#include <stdint.h>
typedef struct iovec iovec_t;
#if defined(__linux__) || defined(__APPLE__)
typedef enum zfs_uio_rw {
	UIO_READ =	0,
	UIO_WRITE =	1,
} zfs_uio_rw_t;
typedef enum zfs_uio_seg {
	UIO_USERSPACE =	0,
	UIO_SYSSPACE =	1,
} zfs_uio_seg_t;
#elif defined(__FreeBSD__)
typedef enum uio_seg  zfs_uio_seg_t;
#endif
typedef struct zfs_uio {
	struct iovec	*uio_iov;	 
	int		uio_iovcnt;	 
	offset_t	uio_loffset;	 
	zfs_uio_seg_t	uio_segflg;	 
	uint16_t	uio_fmode;	 
	uint16_t	uio_extflg;	 
	ssize_t		uio_resid;	 
} zfs_uio_t;
#define	zfs_uio_segflg(uio)		(uio)->uio_segflg
#define	zfs_uio_offset(uio)		(uio)->uio_loffset
#define	zfs_uio_resid(uio)		(uio)->uio_resid
#define	zfs_uio_iovcnt(uio)		(uio)->uio_iovcnt
#define	zfs_uio_iovlen(uio, idx)	(uio)->uio_iov[(idx)].iov_len
#define	zfs_uio_iovbase(uio, idx)	(uio)->uio_iov[(idx)].iov_base
static inline void
zfs_uio_iov_at_index(zfs_uio_t *uio, uint_t idx, void **base, uint64_t *len)
{
	*base = zfs_uio_iovbase(uio, idx);
	*len = zfs_uio_iovlen(uio, idx);
}
static inline void
zfs_uio_advance(zfs_uio_t *uio, size_t size)
{
	uio->uio_resid -= size;
	uio->uio_loffset += size;
}
static inline offset_t
zfs_uio_index_at_offset(zfs_uio_t *uio, offset_t off, uint_t *vec_idx)
{
	*vec_idx = 0;
	while (*vec_idx < (uint_t)zfs_uio_iovcnt(uio) &&
	    off >= (offset_t)zfs_uio_iovlen(uio, *vec_idx)) {
		off -= zfs_uio_iovlen(uio, *vec_idx);
		(*vec_idx)++;
	}
	return (off);
}
#endif	 
