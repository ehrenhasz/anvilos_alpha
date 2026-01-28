#include <sys/param.h>
#include <sys/uio_impl.h>
#include <sys/vnode.h>
#include <sys/zfs_znode.h>
int
zfs_uiomove(void *cp, size_t n, zfs_uio_rw_t dir, zfs_uio_t *uio)
{
	ASSERT3U(zfs_uio_rw(uio), ==, dir);
	return (uiomove(cp, (int)n, GET_UIO_STRUCT(uio)));
}
int
zfs_uiocopy(void *p, size_t n, zfs_uio_rw_t rw, zfs_uio_t *uio, size_t *cbytes)
{
	struct iovec small_iovec[1];
	struct uio small_uio_clone;
	struct uio *uio_clone;
	int error;
	ASSERT3U(zfs_uio_rw(uio), ==, rw);
	if (zfs_uio_iovcnt(uio) == 1) {
		small_uio_clone = *(GET_UIO_STRUCT(uio));
		small_iovec[0] = *(GET_UIO_STRUCT(uio)->uio_iov);
		small_uio_clone.uio_iov = small_iovec;
		uio_clone = &small_uio_clone;
	} else {
		uio_clone = cloneuio(GET_UIO_STRUCT(uio));
	}
	error = vn_io_fault_uiomove(p, n, uio_clone);
	*cbytes = zfs_uio_resid(uio) - uio_clone->uio_resid;
	if (uio_clone != &small_uio_clone)
		free(uio_clone, M_IOV);
	return (error);
}
void
zfs_uioskip(zfs_uio_t *uio, size_t n)
{
	zfs_uio_seg_t segflg;
	if (n > zfs_uio_resid(uio))
		return;
	segflg = zfs_uio_segflg(uio);
	zfs_uio_segflg(uio) = UIO_NOCOPY;
	zfs_uiomove(NULL, n, zfs_uio_rw(uio), uio);
	zfs_uio_segflg(uio) = segflg;
}
int
zfs_uio_fault_move(void *p, size_t n, zfs_uio_rw_t dir, zfs_uio_t *uio)
{
	ASSERT3U(zfs_uio_rw(uio), ==, dir);
	return (vn_io_fault_uiomove(p, n, GET_UIO_STRUCT(uio)));
}
