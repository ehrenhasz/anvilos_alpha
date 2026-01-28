







#ifndef _SYS_UIO_IMPL_H
#define	_SYS_UIO_IMPL_H

#include <sys/uio.h>

extern int zfs_uiomove(void *, size_t, zfs_uio_rw_t, zfs_uio_t *);
extern int zfs_uiocopy(void *, size_t, zfs_uio_rw_t, zfs_uio_t *, size_t *);
extern void zfs_uioskip(zfs_uio_t *, size_t);

static inline void
zfs_uio_iov_at_index(zfs_uio_t *uio, uint_t idx, void **base, uint64_t *len)
{
	*base = zfs_uio_iovbase(uio, idx);
	*len = zfs_uio_iovlen(uio, idx);
}

static inline offset_t
zfs_uio_index_at_offset(zfs_uio_t *uio, offset_t off, uint_t *vec_idx)
{
	*vec_idx = 0;
	while (*vec_idx < zfs_uio_iovcnt(uio) &&
	    off >= zfs_uio_iovlen(uio, *vec_idx)) {
		off -= zfs_uio_iovlen(uio, *vec_idx);
		(*vec_idx)++;
	}

	return (off);
}

#endif	
