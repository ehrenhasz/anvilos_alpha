 
 

 
 

 
 

#ifdef _KERNEL

#include <sys/types.h>
#include <sys/uio_impl.h>
#include <sys/sysmacros.h>
#include <sys/string.h>
#include <linux/kmap_compat.h>
#include <linux/uaccess.h>

 
static int
zfs_uiomove_iov(void *p, size_t n, zfs_uio_rw_t rw, zfs_uio_t *uio)
{
	const struct iovec *iov = uio->uio_iov;
	size_t skip = uio->uio_skip;
	ulong_t cnt;

	while (n && uio->uio_resid) {
		cnt = MIN(iov->iov_len - skip, n);
		switch (uio->uio_segflg) {
		case UIO_USERSPACE:
			 
			if (rw == UIO_READ) {
				if (copy_to_user(iov->iov_base+skip, p, cnt))
					return (EFAULT);
			} else {
				unsigned long b_left = 0;
				if (uio->uio_fault_disable) {
					if (!zfs_access_ok(VERIFY_READ,
					    (iov->iov_base + skip), cnt)) {
						return (EFAULT);
					}
					pagefault_disable();
					b_left =
					    __copy_from_user_inatomic(p,
					    (iov->iov_base + skip), cnt);
					pagefault_enable();
				} else {
					b_left =
					    copy_from_user(p,
					    (iov->iov_base + skip), cnt);
				}
				if (b_left > 0) {
					unsigned long c_bytes =
					    cnt - b_left;
					uio->uio_skip += c_bytes;
					ASSERT3U(uio->uio_skip, <,
					    iov->iov_len);
					uio->uio_resid -= c_bytes;
					uio->uio_loffset += c_bytes;
					return (EFAULT);
				}
			}
			break;
		case UIO_SYSSPACE:
			if (rw == UIO_READ)
				memcpy(iov->iov_base + skip, p, cnt);
			else
				memcpy(p, iov->iov_base + skip, cnt);
			break;
		default:
			ASSERT(0);
		}
		skip += cnt;
		if (skip == iov->iov_len) {
			skip = 0;
			uio->uio_iov = (++iov);
			uio->uio_iovcnt--;
		}
		uio->uio_skip = skip;
		uio->uio_resid -= cnt;
		uio->uio_loffset += cnt;
		p = (caddr_t)p + cnt;
		n -= cnt;
	}
	return (0);
}

static int
zfs_uiomove_bvec_impl(void *p, size_t n, zfs_uio_rw_t rw, zfs_uio_t *uio)
{
	const struct bio_vec *bv = uio->uio_bvec;
	size_t skip = uio->uio_skip;
	ulong_t cnt;

	while (n && uio->uio_resid) {
		void *paddr;
		cnt = MIN(bv->bv_len - skip, n);

		paddr = zfs_kmap_atomic(bv->bv_page);
		if (rw == UIO_READ) {
			 
			memcpy(paddr + bv->bv_offset + skip, p, cnt);
		} else {
			 
			memcpy(p, paddr + bv->bv_offset + skip, cnt);
		}
		zfs_kunmap_atomic(paddr);

		skip += cnt;
		if (skip == bv->bv_len) {
			skip = 0;
			uio->uio_bvec = (++bv);
			uio->uio_iovcnt--;
		}
		uio->uio_skip = skip;
		uio->uio_resid -= cnt;
		uio->uio_loffset += cnt;
		p = (caddr_t)p + cnt;
		n -= cnt;
	}
	return (0);
}

#ifdef HAVE_BLK_MQ
static void
zfs_copy_bvec(void *p, size_t skip, size_t cnt, zfs_uio_rw_t rw,
    struct bio_vec *bv)
{
	void *paddr;

	paddr = zfs_kmap_atomic(bv->bv_page);
	if (rw == UIO_READ) {
		 
		memcpy(paddr + bv->bv_offset + skip, p, cnt);
	} else {
		 
		memcpy(p, paddr + bv->bv_offset + skip, cnt);
	}
	zfs_kunmap_atomic(paddr);
}

 
static int
zfs_uiomove_bvec_rq(void *p, size_t n, zfs_uio_rw_t rw, zfs_uio_t *uio)
{
	struct request *rq = uio->rq;
	struct bio_vec bv;
	struct req_iterator iter;
	size_t this_seg_start;	 
	size_t this_seg_end;		 
	size_t skip_in_seg;
	size_t copy_from_seg;
	size_t orig_loffset;
	int copied = 0;

	 
	orig_loffset = io_offset(NULL, rq);
	this_seg_start = orig_loffset;

	rq_for_each_segment(bv, rq, iter) {
		 
		this_seg_end = this_seg_start + bv.bv_len - 1;

		 
		if (uio->uio_loffset >= this_seg_start &&
		    uio->uio_loffset <= this_seg_end) {
			 

			 
			skip_in_seg = uio->uio_loffset - this_seg_start;

			 
			copy_from_seg = MIN(bv.bv_len - skip_in_seg, n);

			 
			zfs_copy_bvec(p, skip_in_seg, copy_from_seg, rw, &bv);
			p = ((char *)p) + copy_from_seg;

			n -= copy_from_seg;
			uio->uio_resid -= copy_from_seg;
			uio->uio_loffset += copy_from_seg;
			copied = 1;	 
		}

		this_seg_start = this_seg_end + 1;
	}

	if (!copied) {
		 
		uio->uio_resid = 0;
	}
	return (0);
}
#endif

static int
zfs_uiomove_bvec(void *p, size_t n, zfs_uio_rw_t rw, zfs_uio_t *uio)
{
#ifdef HAVE_BLK_MQ
	if (uio->rq != NULL)
		return (zfs_uiomove_bvec_rq(p, n, rw, uio));
#else
	ASSERT3P(uio->rq, ==, NULL);
#endif
	return (zfs_uiomove_bvec_impl(p, n, rw, uio));
}

#if defined(HAVE_VFS_IOV_ITER)
static int
zfs_uiomove_iter(void *p, size_t n, zfs_uio_rw_t rw, zfs_uio_t *uio,
    boolean_t revert)
{
	size_t cnt = MIN(n, uio->uio_resid);

	if (uio->uio_skip)
		iov_iter_advance(uio->uio_iter, uio->uio_skip);

	if (rw == UIO_READ)
		cnt = copy_to_iter(p, cnt, uio->uio_iter);
	else
		cnt = copy_from_iter(p, cnt, uio->uio_iter);

	 
	if (cnt == 0)
		return (EFAULT);

	 
	if (revert)
		iov_iter_revert(uio->uio_iter, cnt);

	uio->uio_resid -= cnt;
	uio->uio_loffset += cnt;

	return (0);
}
#endif

int
zfs_uiomove(void *p, size_t n, zfs_uio_rw_t rw, zfs_uio_t *uio)
{
	if (uio->uio_segflg == UIO_BVEC)
		return (zfs_uiomove_bvec(p, n, rw, uio));
#if defined(HAVE_VFS_IOV_ITER)
	else if (uio->uio_segflg == UIO_ITER)
		return (zfs_uiomove_iter(p, n, rw, uio, B_FALSE));
#endif
	else
		return (zfs_uiomove_iov(p, n, rw, uio));
}
EXPORT_SYMBOL(zfs_uiomove);

 
int
zfs_uio_prefaultpages(ssize_t n, zfs_uio_t *uio)
{
	if (uio->uio_segflg == UIO_SYSSPACE || uio->uio_segflg == UIO_BVEC) {
		 
		return (0);
#if defined(HAVE_VFS_IOV_ITER)
	} else if (uio->uio_segflg == UIO_ITER) {
		 
		if (iov_iter_fault_in_readable(uio->uio_iter, n))
			return (EFAULT);
#endif
	} else {
		 
		ASSERT3S(uio->uio_segflg, ==, UIO_USERSPACE);
		const struct iovec *iov = uio->uio_iov;
		int iovcnt = uio->uio_iovcnt;
		size_t skip = uio->uio_skip;
		uint8_t tmp;
		caddr_t p;

		for (; n > 0 && iovcnt > 0; iov++, iovcnt--, skip = 0) {
			ulong_t cnt = MIN(iov->iov_len - skip, n);
			 
			if (cnt == 0)
				continue;
			n -= cnt;
			 
			p = iov->iov_base + skip;
			while (cnt) {
				if (copy_from_user(&tmp, p, 1))
					return (EFAULT);
				ulong_t incr = MIN(cnt, PAGESIZE);
				p += incr;
				cnt -= incr;
			}
			 
			p--;
			if (copy_from_user(&tmp, p, 1))
				return (EFAULT);
		}
	}

	return (0);
}
EXPORT_SYMBOL(zfs_uio_prefaultpages);

 
int
zfs_uiocopy(void *p, size_t n, zfs_uio_rw_t rw, zfs_uio_t *uio, size_t *cbytes)
{
	zfs_uio_t uio_copy;
	int ret;

	memcpy(&uio_copy, uio, sizeof (zfs_uio_t));

	if (uio->uio_segflg == UIO_BVEC)
		ret = zfs_uiomove_bvec(p, n, rw, &uio_copy);
#if defined(HAVE_VFS_IOV_ITER)
	else if (uio->uio_segflg == UIO_ITER)
		ret = zfs_uiomove_iter(p, n, rw, &uio_copy, B_TRUE);
#endif
	else
		ret = zfs_uiomove_iov(p, n, rw, &uio_copy);

	*cbytes = uio->uio_resid - uio_copy.uio_resid;

	return (ret);
}
EXPORT_SYMBOL(zfs_uiocopy);

 
void
zfs_uioskip(zfs_uio_t *uio, size_t n)
{
	if (n > uio->uio_resid)
		return;
	 
	if (uio->uio_segflg == UIO_BVEC && uio->rq == NULL) {
		uio->uio_skip += n;
		while (uio->uio_iovcnt &&
		    uio->uio_skip >= uio->uio_bvec->bv_len) {
			uio->uio_skip -= uio->uio_bvec->bv_len;
			uio->uio_bvec++;
			uio->uio_iovcnt--;
		}
#if defined(HAVE_VFS_IOV_ITER)
	} else if (uio->uio_segflg == UIO_ITER) {
		iov_iter_advance(uio->uio_iter, n);
#endif
	} else {
		uio->uio_skip += n;
		while (uio->uio_iovcnt &&
		    uio->uio_skip >= uio->uio_iov->iov_len) {
			uio->uio_skip -= uio->uio_iov->iov_len;
			uio->uio_iov++;
			uio->uio_iovcnt--;
		}
	}
	uio->uio_loffset += n;
	uio->uio_resid -= n;
}
EXPORT_SYMBOL(zfs_uioskip);

#endif  
