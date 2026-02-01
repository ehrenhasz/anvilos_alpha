
#include <crypto/hash.h>
#include <linux/export.h>
#include <linux/bvec.h>
#include <linux/fault-inject-usercopy.h>
#include <linux/uio.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/splice.h>
#include <linux/compat.h>
#include <net/checksum.h>
#include <linux/scatterlist.h>
#include <linux/instrumented.h>

 
#define iterate_buf(i, n, base, len, off, __p, STEP) {		\
	size_t __maybe_unused off = 0;				\
	len = n;						\
	base = __p + i->iov_offset;				\
	len -= (STEP);						\
	i->iov_offset += len;					\
	n = len;						\
}

 
#define iterate_iovec(i, n, base, len, off, __p, STEP) {	\
	size_t off = 0;						\
	size_t skip = i->iov_offset;				\
	do {							\
		len = min(n, __p->iov_len - skip);		\
		if (likely(len)) {				\
			base = __p->iov_base + skip;		\
			len -= (STEP);				\
			off += len;				\
			skip += len;				\
			n -= len;				\
			if (skip < __p->iov_len)		\
				break;				\
		}						\
		__p++;						\
		skip = 0;					\
	} while (n);						\
	i->iov_offset = skip;					\
	n = off;						\
}

#define iterate_bvec(i, n, base, len, off, p, STEP) {		\
	size_t off = 0;						\
	unsigned skip = i->iov_offset;				\
	while (n) {						\
		unsigned offset = p->bv_offset + skip;		\
		unsigned left;					\
		void *kaddr = kmap_local_page(p->bv_page +	\
					offset / PAGE_SIZE);	\
		base = kaddr + offset % PAGE_SIZE;		\
		len = min(min(n, (size_t)(p->bv_len - skip)),	\
		     (size_t)(PAGE_SIZE - offset % PAGE_SIZE));	\
		left = (STEP);					\
		kunmap_local(kaddr);				\
		len -= left;					\
		off += len;					\
		skip += len;					\
		if (skip == p->bv_len) {			\
			skip = 0;				\
			p++;					\
		}						\
		n -= len;					\
		if (left)					\
			break;					\
	}							\
	i->iov_offset = skip;					\
	n = off;						\
}

#define iterate_xarray(i, n, base, len, __off, STEP) {		\
	__label__ __out;					\
	size_t __off = 0;					\
	struct folio *folio;					\
	loff_t start = i->xarray_start + i->iov_offset;		\
	pgoff_t index = start / PAGE_SIZE;			\
	XA_STATE(xas, i->xarray, index);			\
								\
	len = PAGE_SIZE - offset_in_page(start);		\
	rcu_read_lock();					\
	xas_for_each(&xas, folio, ULONG_MAX) {			\
		unsigned left;					\
		size_t offset;					\
		if (xas_retry(&xas, folio))			\
			continue;				\
		if (WARN_ON(xa_is_value(folio)))		\
			break;					\
		if (WARN_ON(folio_test_hugetlb(folio)))		\
			break;					\
		offset = offset_in_folio(folio, start + __off);	\
		while (offset < folio_size(folio)) {		\
			base = kmap_local_folio(folio, offset);	\
			len = min(n, len);			\
			left = (STEP);				\
			kunmap_local(base);			\
			len -= left;				\
			__off += len;				\
			n -= len;				\
			if (left || n == 0)			\
				goto __out;			\
			offset += len;				\
			len = PAGE_SIZE;			\
		}						\
	}							\
__out:								\
	rcu_read_unlock();					\
	i->iov_offset += __off;					\
	n = __off;						\
}

#define __iterate_and_advance(i, n, base, len, off, I, K) {	\
	if (unlikely(i->count < n))				\
		n = i->count;					\
	if (likely(n)) {					\
		if (likely(iter_is_ubuf(i))) {			\
			void __user *base;			\
			size_t len;				\
			iterate_buf(i, n, base, len, off,	\
						i->ubuf, (I)) 	\
		} else if (likely(iter_is_iovec(i))) {		\
			const struct iovec *iov = iter_iov(i);	\
			void __user *base;			\
			size_t len;				\
			iterate_iovec(i, n, base, len, off,	\
						iov, (I))	\
			i->nr_segs -= iov - iter_iov(i);	\
			i->__iov = iov;				\
		} else if (iov_iter_is_bvec(i)) {		\
			const struct bio_vec *bvec = i->bvec;	\
			void *base;				\
			size_t len;				\
			iterate_bvec(i, n, base, len, off,	\
						bvec, (K))	\
			i->nr_segs -= bvec - i->bvec;		\
			i->bvec = bvec;				\
		} else if (iov_iter_is_kvec(i)) {		\
			const struct kvec *kvec = i->kvec;	\
			void *base;				\
			size_t len;				\
			iterate_iovec(i, n, base, len, off,	\
						kvec, (K))	\
			i->nr_segs -= kvec - i->kvec;		\
			i->kvec = kvec;				\
		} else if (iov_iter_is_xarray(i)) {		\
			void *base;				\
			size_t len;				\
			iterate_xarray(i, n, base, len, off,	\
							(K))	\
		}						\
		i->count -= n;					\
	}							\
}
#define iterate_and_advance(i, n, base, len, off, I, K) \
	__iterate_and_advance(i, n, base, len, off, I, ((void)(K),0))

static int copyout(void __user *to, const void *from, size_t n)
{
	if (should_fail_usercopy())
		return n;
	if (access_ok(to, n)) {
		instrument_copy_to_user(to, from, n);
		n = raw_copy_to_user(to, from, n);
	}
	return n;
}

static int copyout_nofault(void __user *to, const void *from, size_t n)
{
	long res;

	if (should_fail_usercopy())
		return n;

	res = copy_to_user_nofault(to, from, n);

	return res < 0 ? n : res;
}

static int copyin(void *to, const void __user *from, size_t n)
{
	size_t res = n;

	if (should_fail_usercopy())
		return n;
	if (access_ok(from, n)) {
		instrument_copy_from_user_before(to, from, n);
		res = raw_copy_from_user(to, from, n);
		instrument_copy_from_user_after(to, from, n, res);
	}
	return res;
}

 
size_t fault_in_iov_iter_readable(const struct iov_iter *i, size_t size)
{
	if (iter_is_ubuf(i)) {
		size_t n = min(size, iov_iter_count(i));
		n -= fault_in_readable(i->ubuf + i->iov_offset, n);
		return size - n;
	} else if (iter_is_iovec(i)) {
		size_t count = min(size, iov_iter_count(i));
		const struct iovec *p;
		size_t skip;

		size -= count;
		for (p = iter_iov(i), skip = i->iov_offset; count; p++, skip = 0) {
			size_t len = min(count, p->iov_len - skip);
			size_t ret;

			if (unlikely(!len))
				continue;
			ret = fault_in_readable(p->iov_base + skip, len);
			count -= len - ret;
			if (ret)
				break;
		}
		return count + size;
	}
	return 0;
}
EXPORT_SYMBOL(fault_in_iov_iter_readable);

 
size_t fault_in_iov_iter_writeable(const struct iov_iter *i, size_t size)
{
	if (iter_is_ubuf(i)) {
		size_t n = min(size, iov_iter_count(i));
		n -= fault_in_safe_writeable(i->ubuf + i->iov_offset, n);
		return size - n;
	} else if (iter_is_iovec(i)) {
		size_t count = min(size, iov_iter_count(i));
		const struct iovec *p;
		size_t skip;

		size -= count;
		for (p = iter_iov(i), skip = i->iov_offset; count; p++, skip = 0) {
			size_t len = min(count, p->iov_len - skip);
			size_t ret;

			if (unlikely(!len))
				continue;
			ret = fault_in_safe_writeable(p->iov_base + skip, len);
			count -= len - ret;
			if (ret)
				break;
		}
		return count + size;
	}
	return 0;
}
EXPORT_SYMBOL(fault_in_iov_iter_writeable);

void iov_iter_init(struct iov_iter *i, unsigned int direction,
			const struct iovec *iov, unsigned long nr_segs,
			size_t count)
{
	WARN_ON(direction & ~(READ | WRITE));
	*i = (struct iov_iter) {
		.iter_type = ITER_IOVEC,
		.copy_mc = false,
		.nofault = false,
		.user_backed = true,
		.data_source = direction,
		.__iov = iov,
		.nr_segs = nr_segs,
		.iov_offset = 0,
		.count = count
	};
}
EXPORT_SYMBOL(iov_iter_init);

static __wsum csum_and_memcpy(void *to, const void *from, size_t len,
			      __wsum sum, size_t off)
{
	__wsum next = csum_partial_copy_nocheck(from, to, len);
	return csum_block_add(sum, next, off);
}

size_t _copy_to_iter(const void *addr, size_t bytes, struct iov_iter *i)
{
	if (WARN_ON_ONCE(i->data_source))
		return 0;
	if (user_backed_iter(i))
		might_fault();
	iterate_and_advance(i, bytes, base, len, off,
		copyout(base, addr + off, len),
		memcpy(base, addr + off, len)
	)

	return bytes;
}
EXPORT_SYMBOL(_copy_to_iter);

#ifdef CONFIG_ARCH_HAS_COPY_MC
static int copyout_mc(void __user *to, const void *from, size_t n)
{
	if (access_ok(to, n)) {
		instrument_copy_to_user(to, from, n);
		n = copy_mc_to_user((__force void *) to, from, n);
	}
	return n;
}

 
size_t _copy_mc_to_iter(const void *addr, size_t bytes, struct iov_iter *i)
{
	if (WARN_ON_ONCE(i->data_source))
		return 0;
	if (user_backed_iter(i))
		might_fault();
	__iterate_and_advance(i, bytes, base, len, off,
		copyout_mc(base, addr + off, len),
		copy_mc_to_kernel(base, addr + off, len)
	)

	return bytes;
}
EXPORT_SYMBOL_GPL(_copy_mc_to_iter);
#endif  

static void *memcpy_from_iter(struct iov_iter *i, void *to, const void *from,
				 size_t size)
{
	if (iov_iter_is_copy_mc(i))
		return (void *)copy_mc_to_kernel(to, from, size);
	return memcpy(to, from, size);
}

size_t _copy_from_iter(void *addr, size_t bytes, struct iov_iter *i)
{
	if (WARN_ON_ONCE(!i->data_source))
		return 0;

	if (user_backed_iter(i))
		might_fault();
	iterate_and_advance(i, bytes, base, len, off,
		copyin(addr + off, base, len),
		memcpy_from_iter(i, addr + off, base, len)
	)

	return bytes;
}
EXPORT_SYMBOL(_copy_from_iter);

size_t _copy_from_iter_nocache(void *addr, size_t bytes, struct iov_iter *i)
{
	if (WARN_ON_ONCE(!i->data_source))
		return 0;

	iterate_and_advance(i, bytes, base, len, off,
		__copy_from_user_inatomic_nocache(addr + off, base, len),
		memcpy(addr + off, base, len)
	)

	return bytes;
}
EXPORT_SYMBOL(_copy_from_iter_nocache);

#ifdef CONFIG_ARCH_HAS_UACCESS_FLUSHCACHE
 
size_t _copy_from_iter_flushcache(void *addr, size_t bytes, struct iov_iter *i)
{
	if (WARN_ON_ONCE(!i->data_source))
		return 0;

	iterate_and_advance(i, bytes, base, len, off,
		__copy_from_user_flushcache(addr + off, base, len),
		memcpy_flushcache(addr + off, base, len)
	)

	return bytes;
}
EXPORT_SYMBOL_GPL(_copy_from_iter_flushcache);
#endif

static inline bool page_copy_sane(struct page *page, size_t offset, size_t n)
{
	struct page *head;
	size_t v = n + offset;

	 
	if (n <= v && v <= PAGE_SIZE)
		return true;

	head = compound_head(page);
	v += (page - head) << PAGE_SHIFT;

	if (WARN_ON(n > v || v > page_size(head)))
		return false;
	return true;
}

size_t copy_page_to_iter(struct page *page, size_t offset, size_t bytes,
			 struct iov_iter *i)
{
	size_t res = 0;
	if (!page_copy_sane(page, offset, bytes))
		return 0;
	if (WARN_ON_ONCE(i->data_source))
		return 0;
	page += offset / PAGE_SIZE;  
	offset %= PAGE_SIZE;
	while (1) {
		void *kaddr = kmap_local_page(page);
		size_t n = min(bytes, (size_t)PAGE_SIZE - offset);
		n = _copy_to_iter(kaddr + offset, n, i);
		kunmap_local(kaddr);
		res += n;
		bytes -= n;
		if (!bytes || !n)
			break;
		offset += n;
		if (offset == PAGE_SIZE) {
			page++;
			offset = 0;
		}
	}
	return res;
}
EXPORT_SYMBOL(copy_page_to_iter);

size_t copy_page_to_iter_nofault(struct page *page, unsigned offset, size_t bytes,
				 struct iov_iter *i)
{
	size_t res = 0;

	if (!page_copy_sane(page, offset, bytes))
		return 0;
	if (WARN_ON_ONCE(i->data_source))
		return 0;
	page += offset / PAGE_SIZE;  
	offset %= PAGE_SIZE;
	while (1) {
		void *kaddr = kmap_local_page(page);
		size_t n = min(bytes, (size_t)PAGE_SIZE - offset);

		iterate_and_advance(i, n, base, len, off,
			copyout_nofault(base, kaddr + offset + off, len),
			memcpy(base, kaddr + offset + off, len)
		)
		kunmap_local(kaddr);
		res += n;
		bytes -= n;
		if (!bytes || !n)
			break;
		offset += n;
		if (offset == PAGE_SIZE) {
			page++;
			offset = 0;
		}
	}
	return res;
}
EXPORT_SYMBOL(copy_page_to_iter_nofault);

size_t copy_page_from_iter(struct page *page, size_t offset, size_t bytes,
			 struct iov_iter *i)
{
	size_t res = 0;
	if (!page_copy_sane(page, offset, bytes))
		return 0;
	page += offset / PAGE_SIZE;  
	offset %= PAGE_SIZE;
	while (1) {
		void *kaddr = kmap_local_page(page);
		size_t n = min(bytes, (size_t)PAGE_SIZE - offset);
		n = _copy_from_iter(kaddr + offset, n, i);
		kunmap_local(kaddr);
		res += n;
		bytes -= n;
		if (!bytes || !n)
			break;
		offset += n;
		if (offset == PAGE_SIZE) {
			page++;
			offset = 0;
		}
	}
	return res;
}
EXPORT_SYMBOL(copy_page_from_iter);

size_t iov_iter_zero(size_t bytes, struct iov_iter *i)
{
	iterate_and_advance(i, bytes, base, len, count,
		clear_user(base, len),
		memset(base, 0, len)
	)

	return bytes;
}
EXPORT_SYMBOL(iov_iter_zero);

size_t copy_page_from_iter_atomic(struct page *page, size_t offset,
		size_t bytes, struct iov_iter *i)
{
	size_t n, copied = 0;

	if (!page_copy_sane(page, offset, bytes))
		return 0;
	if (WARN_ON_ONCE(!i->data_source))
		return 0;

	do {
		char *p;

		n = bytes - copied;
		if (PageHighMem(page)) {
			page += offset / PAGE_SIZE;
			offset %= PAGE_SIZE;
			n = min_t(size_t, n, PAGE_SIZE - offset);
		}

		p = kmap_atomic(page) + offset;
		iterate_and_advance(i, n, base, len, off,
			copyin(p + off, base, len),
			memcpy_from_iter(i, p + off, base, len)
		)
		kunmap_atomic(p);
		copied += n;
		offset += n;
	} while (PageHighMem(page) && copied != bytes && n > 0);

	return copied;
}
EXPORT_SYMBOL(copy_page_from_iter_atomic);

static void iov_iter_bvec_advance(struct iov_iter *i, size_t size)
{
	const struct bio_vec *bvec, *end;

	if (!i->count)
		return;
	i->count -= size;

	size += i->iov_offset;

	for (bvec = i->bvec, end = bvec + i->nr_segs; bvec < end; bvec++) {
		if (likely(size < bvec->bv_len))
			break;
		size -= bvec->bv_len;
	}
	i->iov_offset = size;
	i->nr_segs -= bvec - i->bvec;
	i->bvec = bvec;
}

static void iov_iter_iovec_advance(struct iov_iter *i, size_t size)
{
	const struct iovec *iov, *end;

	if (!i->count)
		return;
	i->count -= size;

	size += i->iov_offset;  
	for (iov = iter_iov(i), end = iov + i->nr_segs; iov < end; iov++) {
		if (likely(size < iov->iov_len))
			break;
		size -= iov->iov_len;
	}
	i->iov_offset = size;
	i->nr_segs -= iov - iter_iov(i);
	i->__iov = iov;
}

void iov_iter_advance(struct iov_iter *i, size_t size)
{
	if (unlikely(i->count < size))
		size = i->count;
	if (likely(iter_is_ubuf(i)) || unlikely(iov_iter_is_xarray(i))) {
		i->iov_offset += size;
		i->count -= size;
	} else if (likely(iter_is_iovec(i) || iov_iter_is_kvec(i))) {
		 
		iov_iter_iovec_advance(i, size);
	} else if (iov_iter_is_bvec(i)) {
		iov_iter_bvec_advance(i, size);
	} else if (iov_iter_is_discard(i)) {
		i->count -= size;
	}
}
EXPORT_SYMBOL(iov_iter_advance);

void iov_iter_revert(struct iov_iter *i, size_t unroll)
{
	if (!unroll)
		return;
	if (WARN_ON(unroll > MAX_RW_COUNT))
		return;
	i->count += unroll;
	if (unlikely(iov_iter_is_discard(i)))
		return;
	if (unroll <= i->iov_offset) {
		i->iov_offset -= unroll;
		return;
	}
	unroll -= i->iov_offset;
	if (iov_iter_is_xarray(i) || iter_is_ubuf(i)) {
		BUG();  
	} else if (iov_iter_is_bvec(i)) {
		const struct bio_vec *bvec = i->bvec;
		while (1) {
			size_t n = (--bvec)->bv_len;
			i->nr_segs++;
			if (unroll <= n) {
				i->bvec = bvec;
				i->iov_offset = n - unroll;
				return;
			}
			unroll -= n;
		}
	} else {  
		const struct iovec *iov = iter_iov(i);
		while (1) {
			size_t n = (--iov)->iov_len;
			i->nr_segs++;
			if (unroll <= n) {
				i->__iov = iov;
				i->iov_offset = n - unroll;
				return;
			}
			unroll -= n;
		}
	}
}
EXPORT_SYMBOL(iov_iter_revert);

 
size_t iov_iter_single_seg_count(const struct iov_iter *i)
{
	if (i->nr_segs > 1) {
		if (likely(iter_is_iovec(i) || iov_iter_is_kvec(i)))
			return min(i->count, iter_iov(i)->iov_len - i->iov_offset);
		if (iov_iter_is_bvec(i))
			return min(i->count, i->bvec->bv_len - i->iov_offset);
	}
	return i->count;
}
EXPORT_SYMBOL(iov_iter_single_seg_count);

void iov_iter_kvec(struct iov_iter *i, unsigned int direction,
			const struct kvec *kvec, unsigned long nr_segs,
			size_t count)
{
	WARN_ON(direction & ~(READ | WRITE));
	*i = (struct iov_iter){
		.iter_type = ITER_KVEC,
		.copy_mc = false,
		.data_source = direction,
		.kvec = kvec,
		.nr_segs = nr_segs,
		.iov_offset = 0,
		.count = count
	};
}
EXPORT_SYMBOL(iov_iter_kvec);

void iov_iter_bvec(struct iov_iter *i, unsigned int direction,
			const struct bio_vec *bvec, unsigned long nr_segs,
			size_t count)
{
	WARN_ON(direction & ~(READ | WRITE));
	*i = (struct iov_iter){
		.iter_type = ITER_BVEC,
		.copy_mc = false,
		.data_source = direction,
		.bvec = bvec,
		.nr_segs = nr_segs,
		.iov_offset = 0,
		.count = count
	};
}
EXPORT_SYMBOL(iov_iter_bvec);

 
void iov_iter_xarray(struct iov_iter *i, unsigned int direction,
		     struct xarray *xarray, loff_t start, size_t count)
{
	BUG_ON(direction & ~1);
	*i = (struct iov_iter) {
		.iter_type = ITER_XARRAY,
		.copy_mc = false,
		.data_source = direction,
		.xarray = xarray,
		.xarray_start = start,
		.count = count,
		.iov_offset = 0
	};
}
EXPORT_SYMBOL(iov_iter_xarray);

 
void iov_iter_discard(struct iov_iter *i, unsigned int direction, size_t count)
{
	BUG_ON(direction != READ);
	*i = (struct iov_iter){
		.iter_type = ITER_DISCARD,
		.copy_mc = false,
		.data_source = false,
		.count = count,
		.iov_offset = 0
	};
}
EXPORT_SYMBOL(iov_iter_discard);

static bool iov_iter_aligned_iovec(const struct iov_iter *i, unsigned addr_mask,
				   unsigned len_mask)
{
	size_t size = i->count;
	size_t skip = i->iov_offset;
	unsigned k;

	for (k = 0; k < i->nr_segs; k++, skip = 0) {
		const struct iovec *iov = iter_iov(i) + k;
		size_t len = iov->iov_len - skip;

		if (len > size)
			len = size;
		if (len & len_mask)
			return false;
		if ((unsigned long)(iov->iov_base + skip) & addr_mask)
			return false;

		size -= len;
		if (!size)
			break;
	}
	return true;
}

static bool iov_iter_aligned_bvec(const struct iov_iter *i, unsigned addr_mask,
				  unsigned len_mask)
{
	size_t size = i->count;
	unsigned skip = i->iov_offset;
	unsigned k;

	for (k = 0; k < i->nr_segs; k++, skip = 0) {
		size_t len = i->bvec[k].bv_len - skip;

		if (len > size)
			len = size;
		if (len & len_mask)
			return false;
		if ((unsigned long)(i->bvec[k].bv_offset + skip) & addr_mask)
			return false;

		size -= len;
		if (!size)
			break;
	}
	return true;
}

 
bool iov_iter_is_aligned(const struct iov_iter *i, unsigned addr_mask,
			 unsigned len_mask)
{
	if (likely(iter_is_ubuf(i))) {
		if (i->count & len_mask)
			return false;
		if ((unsigned long)(i->ubuf + i->iov_offset) & addr_mask)
			return false;
		return true;
	}

	if (likely(iter_is_iovec(i) || iov_iter_is_kvec(i)))
		return iov_iter_aligned_iovec(i, addr_mask, len_mask);

	if (iov_iter_is_bvec(i))
		return iov_iter_aligned_bvec(i, addr_mask, len_mask);

	if (iov_iter_is_xarray(i)) {
		if (i->count & len_mask)
			return false;
		if ((i->xarray_start + i->iov_offset) & addr_mask)
			return false;
	}

	return true;
}
EXPORT_SYMBOL_GPL(iov_iter_is_aligned);

static unsigned long iov_iter_alignment_iovec(const struct iov_iter *i)
{
	unsigned long res = 0;
	size_t size = i->count;
	size_t skip = i->iov_offset;
	unsigned k;

	for (k = 0; k < i->nr_segs; k++, skip = 0) {
		const struct iovec *iov = iter_iov(i) + k;
		size_t len = iov->iov_len - skip;
		if (len) {
			res |= (unsigned long)iov->iov_base + skip;
			if (len > size)
				len = size;
			res |= len;
			size -= len;
			if (!size)
				break;
		}
	}
	return res;
}

static unsigned long iov_iter_alignment_bvec(const struct iov_iter *i)
{
	unsigned res = 0;
	size_t size = i->count;
	unsigned skip = i->iov_offset;
	unsigned k;

	for (k = 0; k < i->nr_segs; k++, skip = 0) {
		size_t len = i->bvec[k].bv_len - skip;
		res |= (unsigned long)i->bvec[k].bv_offset + skip;
		if (len > size)
			len = size;
		res |= len;
		size -= len;
		if (!size)
			break;
	}
	return res;
}

unsigned long iov_iter_alignment(const struct iov_iter *i)
{
	if (likely(iter_is_ubuf(i))) {
		size_t size = i->count;
		if (size)
			return ((unsigned long)i->ubuf + i->iov_offset) | size;
		return 0;
	}

	 
	if (likely(iter_is_iovec(i) || iov_iter_is_kvec(i)))
		return iov_iter_alignment_iovec(i);

	if (iov_iter_is_bvec(i))
		return iov_iter_alignment_bvec(i);

	if (iov_iter_is_xarray(i))
		return (i->xarray_start + i->iov_offset) | i->count;

	return 0;
}
EXPORT_SYMBOL(iov_iter_alignment);

unsigned long iov_iter_gap_alignment(const struct iov_iter *i)
{
	unsigned long res = 0;
	unsigned long v = 0;
	size_t size = i->count;
	unsigned k;

	if (iter_is_ubuf(i))
		return 0;

	if (WARN_ON(!iter_is_iovec(i)))
		return ~0U;

	for (k = 0; k < i->nr_segs; k++) {
		const struct iovec *iov = iter_iov(i) + k;
		if (iov->iov_len) {
			unsigned long base = (unsigned long)iov->iov_base;
			if (v) 
				res |= base | v; 
			v = base + iov->iov_len;
			if (size <= iov->iov_len)
				break;
			size -= iov->iov_len;
		}
	}
	return res;
}
EXPORT_SYMBOL(iov_iter_gap_alignment);

static int want_pages_array(struct page ***res, size_t size,
			    size_t start, unsigned int maxpages)
{
	unsigned int count = DIV_ROUND_UP(size + start, PAGE_SIZE);

	if (count > maxpages)
		count = maxpages;
	WARN_ON(!count);	
	if (!*res) {
		*res = kvmalloc_array(count, sizeof(struct page *), GFP_KERNEL);
		if (!*res)
			return 0;
	}
	return count;
}

static ssize_t iter_xarray_populate_pages(struct page **pages, struct xarray *xa,
					  pgoff_t index, unsigned int nr_pages)
{
	XA_STATE(xas, xa, index);
	struct page *page;
	unsigned int ret = 0;

	rcu_read_lock();
	for (page = xas_load(&xas); page; page = xas_next(&xas)) {
		if (xas_retry(&xas, page))
			continue;

		 
		if (unlikely(page != xas_reload(&xas))) {
			xas_reset(&xas);
			continue;
		}

		pages[ret] = find_subpage(page, xas.xa_index);
		get_page(pages[ret]);
		if (++ret == nr_pages)
			break;
	}
	rcu_read_unlock();
	return ret;
}

static ssize_t iter_xarray_get_pages(struct iov_iter *i,
				     struct page ***pages, size_t maxsize,
				     unsigned maxpages, size_t *_start_offset)
{
	unsigned nr, offset, count;
	pgoff_t index;
	loff_t pos;

	pos = i->xarray_start + i->iov_offset;
	index = pos >> PAGE_SHIFT;
	offset = pos & ~PAGE_MASK;
	*_start_offset = offset;

	count = want_pages_array(pages, maxsize, offset, maxpages);
	if (!count)
		return -ENOMEM;
	nr = iter_xarray_populate_pages(*pages, i->xarray, index, count);
	if (nr == 0)
		return 0;

	maxsize = min_t(size_t, nr * PAGE_SIZE - offset, maxsize);
	i->iov_offset += maxsize;
	i->count -= maxsize;
	return maxsize;
}

 
static unsigned long first_iovec_segment(const struct iov_iter *i, size_t *size)
{
	size_t skip;
	long k;

	if (iter_is_ubuf(i))
		return (unsigned long)i->ubuf + i->iov_offset;

	for (k = 0, skip = i->iov_offset; k < i->nr_segs; k++, skip = 0) {
		const struct iovec *iov = iter_iov(i) + k;
		size_t len = iov->iov_len - skip;

		if (unlikely(!len))
			continue;
		if (*size > len)
			*size = len;
		return (unsigned long)iov->iov_base + skip;
	}
	BUG(); 
}

 
static struct page *first_bvec_segment(const struct iov_iter *i,
				       size_t *size, size_t *start)
{
	struct page *page;
	size_t skip = i->iov_offset, len;

	len = i->bvec->bv_len - skip;
	if (*size > len)
		*size = len;
	skip += i->bvec->bv_offset;
	page = i->bvec->bv_page + skip / PAGE_SIZE;
	*start = skip % PAGE_SIZE;
	return page;
}

static ssize_t __iov_iter_get_pages_alloc(struct iov_iter *i,
		   struct page ***pages, size_t maxsize,
		   unsigned int maxpages, size_t *start)
{
	unsigned int n, gup_flags = 0;

	if (maxsize > i->count)
		maxsize = i->count;
	if (!maxsize)
		return 0;
	if (maxsize > MAX_RW_COUNT)
		maxsize = MAX_RW_COUNT;

	if (likely(user_backed_iter(i))) {
		unsigned long addr;
		int res;

		if (iov_iter_rw(i) != WRITE)
			gup_flags |= FOLL_WRITE;
		if (i->nofault)
			gup_flags |= FOLL_NOFAULT;

		addr = first_iovec_segment(i, &maxsize);
		*start = addr % PAGE_SIZE;
		addr &= PAGE_MASK;
		n = want_pages_array(pages, maxsize, *start, maxpages);
		if (!n)
			return -ENOMEM;
		res = get_user_pages_fast(addr, n, gup_flags, *pages);
		if (unlikely(res <= 0))
			return res;
		maxsize = min_t(size_t, maxsize, res * PAGE_SIZE - *start);
		iov_iter_advance(i, maxsize);
		return maxsize;
	}
	if (iov_iter_is_bvec(i)) {
		struct page **p;
		struct page *page;

		page = first_bvec_segment(i, &maxsize, start);
		n = want_pages_array(pages, maxsize, *start, maxpages);
		if (!n)
			return -ENOMEM;
		p = *pages;
		for (int k = 0; k < n; k++)
			get_page(p[k] = page + k);
		maxsize = min_t(size_t, maxsize, n * PAGE_SIZE - *start);
		i->count -= maxsize;
		i->iov_offset += maxsize;
		if (i->iov_offset == i->bvec->bv_len) {
			i->iov_offset = 0;
			i->bvec++;
			i->nr_segs--;
		}
		return maxsize;
	}
	if (iov_iter_is_xarray(i))
		return iter_xarray_get_pages(i, pages, maxsize, maxpages, start);
	return -EFAULT;
}

ssize_t iov_iter_get_pages2(struct iov_iter *i, struct page **pages,
		size_t maxsize, unsigned maxpages, size_t *start)
{
	if (!maxpages)
		return 0;
	BUG_ON(!pages);

	return __iov_iter_get_pages_alloc(i, &pages, maxsize, maxpages, start);
}
EXPORT_SYMBOL(iov_iter_get_pages2);

ssize_t iov_iter_get_pages_alloc2(struct iov_iter *i,
		struct page ***pages, size_t maxsize, size_t *start)
{
	ssize_t len;

	*pages = NULL;

	len = __iov_iter_get_pages_alloc(i, pages, maxsize, ~0U, start);
	if (len <= 0) {
		kvfree(*pages);
		*pages = NULL;
	}
	return len;
}
EXPORT_SYMBOL(iov_iter_get_pages_alloc2);

size_t csum_and_copy_from_iter(void *addr, size_t bytes, __wsum *csum,
			       struct iov_iter *i)
{
	__wsum sum, next;
	sum = *csum;
	if (WARN_ON_ONCE(!i->data_source))
		return 0;

	iterate_and_advance(i, bytes, base, len, off, ({
		next = csum_and_copy_from_user(base, addr + off, len);
		sum = csum_block_add(sum, next, off);
		next ? 0 : len;
	}), ({
		sum = csum_and_memcpy(addr + off, base, len, sum, off);
	})
	)
	*csum = sum;
	return bytes;
}
EXPORT_SYMBOL(csum_and_copy_from_iter);

size_t csum_and_copy_to_iter(const void *addr, size_t bytes, void *_csstate,
			     struct iov_iter *i)
{
	struct csum_state *csstate = _csstate;
	__wsum sum, next;

	if (WARN_ON_ONCE(i->data_source))
		return 0;
	if (unlikely(iov_iter_is_discard(i))) {
		
		csstate->csum = csum_block_add(csstate->csum,
					       csum_partial(addr, bytes, 0),
					       csstate->off);
		csstate->off += bytes;
		return bytes;
	}

	sum = csum_shift(csstate->csum, csstate->off);
	iterate_and_advance(i, bytes, base, len, off, ({
		next = csum_and_copy_to_user(addr + off, base, len);
		sum = csum_block_add(sum, next, off);
		next ? 0 : len;
	}), ({
		sum = csum_and_memcpy(base, addr + off, len, sum, off);
	})
	)
	csstate->csum = csum_shift(sum, csstate->off);
	csstate->off += bytes;
	return bytes;
}
EXPORT_SYMBOL(csum_and_copy_to_iter);

size_t hash_and_copy_to_iter(const void *addr, size_t bytes, void *hashp,
		struct iov_iter *i)
{
#ifdef CONFIG_CRYPTO_HASH
	struct ahash_request *hash = hashp;
	struct scatterlist sg;
	size_t copied;

	copied = copy_to_iter(addr, bytes, i);
	sg_init_one(&sg, addr, copied);
	ahash_request_set_crypt(hash, &sg, NULL, copied);
	crypto_ahash_update(hash);
	return copied;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(hash_and_copy_to_iter);

static int iov_npages(const struct iov_iter *i, int maxpages)
{
	size_t skip = i->iov_offset, size = i->count;
	const struct iovec *p;
	int npages = 0;

	for (p = iter_iov(i); size; skip = 0, p++) {
		unsigned offs = offset_in_page(p->iov_base + skip);
		size_t len = min(p->iov_len - skip, size);

		if (len) {
			size -= len;
			npages += DIV_ROUND_UP(offs + len, PAGE_SIZE);
			if (unlikely(npages > maxpages))
				return maxpages;
		}
	}
	return npages;
}

static int bvec_npages(const struct iov_iter *i, int maxpages)
{
	size_t skip = i->iov_offset, size = i->count;
	const struct bio_vec *p;
	int npages = 0;

	for (p = i->bvec; size; skip = 0, p++) {
		unsigned offs = (p->bv_offset + skip) % PAGE_SIZE;
		size_t len = min(p->bv_len - skip, size);

		size -= len;
		npages += DIV_ROUND_UP(offs + len, PAGE_SIZE);
		if (unlikely(npages > maxpages))
			return maxpages;
	}
	return npages;
}

int iov_iter_npages(const struct iov_iter *i, int maxpages)
{
	if (unlikely(!i->count))
		return 0;
	if (likely(iter_is_ubuf(i))) {
		unsigned offs = offset_in_page(i->ubuf + i->iov_offset);
		int npages = DIV_ROUND_UP(offs + i->count, PAGE_SIZE);
		return min(npages, maxpages);
	}
	 
	if (likely(iter_is_iovec(i) || iov_iter_is_kvec(i)))
		return iov_npages(i, maxpages);
	if (iov_iter_is_bvec(i))
		return bvec_npages(i, maxpages);
	if (iov_iter_is_xarray(i)) {
		unsigned offset = (i->xarray_start + i->iov_offset) % PAGE_SIZE;
		int npages = DIV_ROUND_UP(offset + i->count, PAGE_SIZE);
		return min(npages, maxpages);
	}
	return 0;
}
EXPORT_SYMBOL(iov_iter_npages);

const void *dup_iter(struct iov_iter *new, struct iov_iter *old, gfp_t flags)
{
	*new = *old;
	if (iov_iter_is_bvec(new))
		return new->bvec = kmemdup(new->bvec,
				    new->nr_segs * sizeof(struct bio_vec),
				    flags);
	else if (iov_iter_is_kvec(new) || iter_is_iovec(new))
		 
		return new->__iov = kmemdup(new->__iov,
				   new->nr_segs * sizeof(struct iovec),
				   flags);
	return NULL;
}
EXPORT_SYMBOL(dup_iter);

static __noclone int copy_compat_iovec_from_user(struct iovec *iov,
		const struct iovec __user *uvec, unsigned long nr_segs)
{
	const struct compat_iovec __user *uiov =
		(const struct compat_iovec __user *)uvec;
	int ret = -EFAULT, i;

	if (!user_access_begin(uiov, nr_segs * sizeof(*uiov)))
		return -EFAULT;

	for (i = 0; i < nr_segs; i++) {
		compat_uptr_t buf;
		compat_ssize_t len;

		unsafe_get_user(len, &uiov[i].iov_len, uaccess_end);
		unsafe_get_user(buf, &uiov[i].iov_base, uaccess_end);

		 
		if (len < 0) {
			ret = -EINVAL;
			goto uaccess_end;
		}
		iov[i].iov_base = compat_ptr(buf);
		iov[i].iov_len = len;
	}

	ret = 0;
uaccess_end:
	user_access_end();
	return ret;
}

static __noclone int copy_iovec_from_user(struct iovec *iov,
		const struct iovec __user *uiov, unsigned long nr_segs)
{
	int ret = -EFAULT;

	if (!user_access_begin(uiov, nr_segs * sizeof(*uiov)))
		return -EFAULT;

	do {
		void __user *buf;
		ssize_t len;

		unsafe_get_user(len, &uiov->iov_len, uaccess_end);
		unsafe_get_user(buf, &uiov->iov_base, uaccess_end);

		 
		if (unlikely(len < 0)) {
			ret = -EINVAL;
			goto uaccess_end;
		}
		iov->iov_base = buf;
		iov->iov_len = len;

		uiov++; iov++;
	} while (--nr_segs);

	ret = 0;
uaccess_end:
	user_access_end();
	return ret;
}

struct iovec *iovec_from_user(const struct iovec __user *uvec,
		unsigned long nr_segs, unsigned long fast_segs,
		struct iovec *fast_iov, bool compat)
{
	struct iovec *iov = fast_iov;
	int ret;

	 
	if (nr_segs == 0)
		return iov;
	if (nr_segs > UIO_MAXIOV)
		return ERR_PTR(-EINVAL);
	if (nr_segs > fast_segs) {
		iov = kmalloc_array(nr_segs, sizeof(struct iovec), GFP_KERNEL);
		if (!iov)
			return ERR_PTR(-ENOMEM);
	}

	if (unlikely(compat))
		ret = copy_compat_iovec_from_user(iov, uvec, nr_segs);
	else
		ret = copy_iovec_from_user(iov, uvec, nr_segs);
	if (ret) {
		if (iov != fast_iov)
			kfree(iov);
		return ERR_PTR(ret);
	}

	return iov;
}

 
static ssize_t __import_iovec_ubuf(int type, const struct iovec __user *uvec,
				   struct iovec **iovp, struct iov_iter *i,
				   bool compat)
{
	struct iovec *iov = *iovp;
	ssize_t ret;

	if (compat)
		ret = copy_compat_iovec_from_user(iov, uvec, 1);
	else
		ret = copy_iovec_from_user(iov, uvec, 1);
	if (unlikely(ret))
		return ret;

	ret = import_ubuf(type, iov->iov_base, iov->iov_len, i);
	if (unlikely(ret))
		return ret;
	*iovp = NULL;
	return i->count;
}

ssize_t __import_iovec(int type, const struct iovec __user *uvec,
		 unsigned nr_segs, unsigned fast_segs, struct iovec **iovp,
		 struct iov_iter *i, bool compat)
{
	ssize_t total_len = 0;
	unsigned long seg;
	struct iovec *iov;

	if (nr_segs == 1)
		return __import_iovec_ubuf(type, uvec, iovp, i, compat);

	iov = iovec_from_user(uvec, nr_segs, fast_segs, *iovp, compat);
	if (IS_ERR(iov)) {
		*iovp = NULL;
		return PTR_ERR(iov);
	}

	 
	for (seg = 0; seg < nr_segs; seg++) {
		ssize_t len = (ssize_t)iov[seg].iov_len;

		if (!access_ok(iov[seg].iov_base, len)) {
			if (iov != *iovp)
				kfree(iov);
			*iovp = NULL;
			return -EFAULT;
		}

		if (len > MAX_RW_COUNT - total_len) {
			len = MAX_RW_COUNT - total_len;
			iov[seg].iov_len = len;
		}
		total_len += len;
	}

	iov_iter_init(i, type, iov, nr_segs, total_len);
	if (iov == *iovp)
		*iovp = NULL;
	else
		*iovp = iov;
	return total_len;
}

 
ssize_t import_iovec(int type, const struct iovec __user *uvec,
		 unsigned nr_segs, unsigned fast_segs,
		 struct iovec **iovp, struct iov_iter *i)
{
	return __import_iovec(type, uvec, nr_segs, fast_segs, iovp, i,
			      in_compat_syscall());
}
EXPORT_SYMBOL(import_iovec);

int import_single_range(int rw, void __user *buf, size_t len,
		 struct iovec *iov, struct iov_iter *i)
{
	if (len > MAX_RW_COUNT)
		len = MAX_RW_COUNT;
	if (unlikely(!access_ok(buf, len)))
		return -EFAULT;

	iov_iter_ubuf(i, rw, buf, len);
	return 0;
}
EXPORT_SYMBOL(import_single_range);

int import_ubuf(int rw, void __user *buf, size_t len, struct iov_iter *i)
{
	if (len > MAX_RW_COUNT)
		len = MAX_RW_COUNT;
	if (unlikely(!access_ok(buf, len)))
		return -EFAULT;

	iov_iter_ubuf(i, rw, buf, len);
	return 0;
}
EXPORT_SYMBOL_GPL(import_ubuf);

 
void iov_iter_restore(struct iov_iter *i, struct iov_iter_state *state)
{
	if (WARN_ON_ONCE(!iov_iter_is_bvec(i) && !iter_is_iovec(i) &&
			 !iter_is_ubuf(i)) && !iov_iter_is_kvec(i))
		return;
	i->iov_offset = state->iov_offset;
	i->count = state->count;
	if (iter_is_ubuf(i))
		return;
	 
	BUILD_BUG_ON(sizeof(struct iovec) != sizeof(struct kvec));
	if (iov_iter_is_bvec(i))
		i->bvec -= state->nr_segs - i->nr_segs;
	else
		i->__iov -= state->nr_segs - i->nr_segs;
	i->nr_segs = state->nr_segs;
}

 
static ssize_t iov_iter_extract_xarray_pages(struct iov_iter *i,
					     struct page ***pages, size_t maxsize,
					     unsigned int maxpages,
					     iov_iter_extraction_t extraction_flags,
					     size_t *offset0)
{
	struct page *page, **p;
	unsigned int nr = 0, offset;
	loff_t pos = i->xarray_start + i->iov_offset;
	pgoff_t index = pos >> PAGE_SHIFT;
	XA_STATE(xas, i->xarray, index);

	offset = pos & ~PAGE_MASK;
	*offset0 = offset;

	maxpages = want_pages_array(pages, maxsize, offset, maxpages);
	if (!maxpages)
		return -ENOMEM;
	p = *pages;

	rcu_read_lock();
	for (page = xas_load(&xas); page; page = xas_next(&xas)) {
		if (xas_retry(&xas, page))
			continue;

		 
		if (unlikely(page != xas_reload(&xas))) {
			xas_reset(&xas);
			continue;
		}

		p[nr++] = find_subpage(page, xas.xa_index);
		if (nr == maxpages)
			break;
	}
	rcu_read_unlock();

	maxsize = min_t(size_t, nr * PAGE_SIZE - offset, maxsize);
	iov_iter_advance(i, maxsize);
	return maxsize;
}

 
static ssize_t iov_iter_extract_bvec_pages(struct iov_iter *i,
					   struct page ***pages, size_t maxsize,
					   unsigned int maxpages,
					   iov_iter_extraction_t extraction_flags,
					   size_t *offset0)
{
	struct page **p, *page;
	size_t skip = i->iov_offset, offset, size;
	int k;

	for (;;) {
		if (i->nr_segs == 0)
			return 0;
		size = min(maxsize, i->bvec->bv_len - skip);
		if (size)
			break;
		i->iov_offset = 0;
		i->nr_segs--;
		i->bvec++;
		skip = 0;
	}

	skip += i->bvec->bv_offset;
	page = i->bvec->bv_page + skip / PAGE_SIZE;
	offset = skip % PAGE_SIZE;
	*offset0 = offset;

	maxpages = want_pages_array(pages, size, offset, maxpages);
	if (!maxpages)
		return -ENOMEM;
	p = *pages;
	for (k = 0; k < maxpages; k++)
		p[k] = page + k;

	size = min_t(size_t, size, maxpages * PAGE_SIZE - offset);
	iov_iter_advance(i, size);
	return size;
}

 
static ssize_t iov_iter_extract_kvec_pages(struct iov_iter *i,
					   struct page ***pages, size_t maxsize,
					   unsigned int maxpages,
					   iov_iter_extraction_t extraction_flags,
					   size_t *offset0)
{
	struct page **p, *page;
	const void *kaddr;
	size_t skip = i->iov_offset, offset, len, size;
	int k;

	for (;;) {
		if (i->nr_segs == 0)
			return 0;
		size = min(maxsize, i->kvec->iov_len - skip);
		if (size)
			break;
		i->iov_offset = 0;
		i->nr_segs--;
		i->kvec++;
		skip = 0;
	}

	kaddr = i->kvec->iov_base + skip;
	offset = (unsigned long)kaddr & ~PAGE_MASK;
	*offset0 = offset;

	maxpages = want_pages_array(pages, size, offset, maxpages);
	if (!maxpages)
		return -ENOMEM;
	p = *pages;

	kaddr -= offset;
	len = offset + size;
	for (k = 0; k < maxpages; k++) {
		size_t seg = min_t(size_t, len, PAGE_SIZE);

		if (is_vmalloc_or_module_addr(kaddr))
			page = vmalloc_to_page(kaddr);
		else
			page = virt_to_page(kaddr);

		p[k] = page;
		len -= seg;
		kaddr += PAGE_SIZE;
	}

	size = min_t(size_t, size, maxpages * PAGE_SIZE - offset);
	iov_iter_advance(i, size);
	return size;
}

 
static ssize_t iov_iter_extract_user_pages(struct iov_iter *i,
					   struct page ***pages,
					   size_t maxsize,
					   unsigned int maxpages,
					   iov_iter_extraction_t extraction_flags,
					   size_t *offset0)
{
	unsigned long addr;
	unsigned int gup_flags = 0;
	size_t offset;
	int res;

	if (i->data_source == ITER_DEST)
		gup_flags |= FOLL_WRITE;
	if (extraction_flags & ITER_ALLOW_P2PDMA)
		gup_flags |= FOLL_PCI_P2PDMA;
	if (i->nofault)
		gup_flags |= FOLL_NOFAULT;

	addr = first_iovec_segment(i, &maxsize);
	*offset0 = offset = addr % PAGE_SIZE;
	addr &= PAGE_MASK;
	maxpages = want_pages_array(pages, maxsize, offset, maxpages);
	if (!maxpages)
		return -ENOMEM;
	res = pin_user_pages_fast(addr, maxpages, gup_flags, *pages);
	if (unlikely(res <= 0))
		return res;
	maxsize = min_t(size_t, maxsize, res * PAGE_SIZE - offset);
	iov_iter_advance(i, maxsize);
	return maxsize;
}

 
ssize_t iov_iter_extract_pages(struct iov_iter *i,
			       struct page ***pages,
			       size_t maxsize,
			       unsigned int maxpages,
			       iov_iter_extraction_t extraction_flags,
			       size_t *offset0)
{
	maxsize = min_t(size_t, min_t(size_t, maxsize, i->count), MAX_RW_COUNT);
	if (!maxsize)
		return 0;

	if (likely(user_backed_iter(i)))
		return iov_iter_extract_user_pages(i, pages, maxsize,
						   maxpages, extraction_flags,
						   offset0);
	if (iov_iter_is_kvec(i))
		return iov_iter_extract_kvec_pages(i, pages, maxsize,
						   maxpages, extraction_flags,
						   offset0);
	if (iov_iter_is_bvec(i))
		return iov_iter_extract_bvec_pages(i, pages, maxsize,
						   maxpages, extraction_flags,
						   offset0);
	if (iov_iter_is_xarray(i))
		return iov_iter_extract_xarray_pages(i, pages, maxsize,
						     maxpages, extraction_flags,
						     offset0);
	return -EFAULT;
}
EXPORT_SYMBOL_GPL(iov_iter_extract_pages);
