
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "scrub/xfile.h"
#include "scrub/xfarray.h"
#include "scrub/scrub.h"
#include "scrub/trace.h"

 

 
static inline void *xfarray_scratch(struct xfarray *array)
{
	return (array + 1);
}

 
static xfarray_idx_t
xfarray_idx(
	struct xfarray	*array,
	loff_t		pos)
{
	if (array->obj_size_log >= 0)
		return (xfarray_idx_t)pos >> array->obj_size_log;

	return div_u64((xfarray_idx_t)pos, array->obj_size);
}

 
static inline loff_t xfarray_pos(struct xfarray *array, xfarray_idx_t idx)
{
	if (array->obj_size_log >= 0)
		return idx << array->obj_size_log;

	return idx * array->obj_size;
}

 
int
xfarray_create(
	const char		*description,
	unsigned long long	required_capacity,
	size_t			obj_size,
	struct xfarray		**arrayp)
{
	struct xfarray		*array;
	struct xfile		*xfile;
	int			error;

	ASSERT(obj_size < PAGE_SIZE);

	error = xfile_create(description, 0, &xfile);
	if (error)
		return error;

	error = -ENOMEM;
	array = kzalloc(sizeof(struct xfarray) + obj_size, XCHK_GFP_FLAGS);
	if (!array)
		goto out_xfile;

	array->xfile = xfile;
	array->obj_size = obj_size;

	if (is_power_of_2(obj_size))
		array->obj_size_log = ilog2(obj_size);
	else
		array->obj_size_log = -1;

	array->max_nr = xfarray_idx(array, MAX_LFS_FILESIZE);
	trace_xfarray_create(array, required_capacity);

	if (required_capacity > 0) {
		if (array->max_nr < required_capacity) {
			error = -ENOMEM;
			goto out_xfarray;
		}
		array->max_nr = required_capacity;
	}

	*arrayp = array;
	return 0;

out_xfarray:
	kfree(array);
out_xfile:
	xfile_destroy(xfile);
	return error;
}

 
void
xfarray_destroy(
	struct xfarray	*array)
{
	xfile_destroy(array->xfile);
	kfree(array);
}

 
int
xfarray_load(
	struct xfarray	*array,
	xfarray_idx_t	idx,
	void		*ptr)
{
	if (idx >= array->nr)
		return -ENODATA;

	return xfile_obj_load(array->xfile, ptr, array->obj_size,
			xfarray_pos(array, idx));
}

 
static inline bool
xfarray_is_unset(
	struct xfarray	*array,
	loff_t		pos)
{
	void		*temp = xfarray_scratch(array);
	int		error;

	if (array->unset_slots == 0)
		return false;

	error = xfile_obj_load(array->xfile, temp, array->obj_size, pos);
	if (!error && xfarray_element_is_null(array, temp))
		return true;

	return false;
}

 
int
xfarray_unset(
	struct xfarray	*array,
	xfarray_idx_t	idx)
{
	void		*temp = xfarray_scratch(array);
	loff_t		pos = xfarray_pos(array, idx);
	int		error;

	if (idx >= array->nr)
		return -ENODATA;

	if (idx == array->nr - 1) {
		array->nr--;
		return 0;
	}

	if (xfarray_is_unset(array, pos))
		return 0;

	memset(temp, 0, array->obj_size);
	error = xfile_obj_store(array->xfile, temp, array->obj_size, pos);
	if (error)
		return error;

	array->unset_slots++;
	return 0;
}

 
int
xfarray_store(
	struct xfarray	*array,
	xfarray_idx_t	idx,
	const void	*ptr)
{
	int		ret;

	if (idx >= array->max_nr)
		return -EFBIG;

	ASSERT(!xfarray_element_is_null(array, ptr));

	ret = xfile_obj_store(array->xfile, ptr, array->obj_size,
			xfarray_pos(array, idx));
	if (ret)
		return ret;

	array->nr = max(array->nr, idx + 1);
	return 0;
}

 
bool
xfarray_element_is_null(
	struct xfarray	*array,
	const void	*ptr)
{
	return !memchr_inv(ptr, 0, array->obj_size);
}

 
int
xfarray_store_anywhere(
	struct xfarray	*array,
	const void	*ptr)
{
	void		*temp = xfarray_scratch(array);
	loff_t		endpos = xfarray_pos(array, array->nr);
	loff_t		pos;
	int		error;

	 
	for (pos = 0;
	     pos < endpos && array->unset_slots > 0;
	     pos += array->obj_size) {
		error = xfile_obj_load(array->xfile, temp, array->obj_size,
				pos);
		if (error || !xfarray_element_is_null(array, temp))
			continue;

		error = xfile_obj_store(array->xfile, ptr, array->obj_size,
				pos);
		if (error)
			return error;

		array->unset_slots--;
		return 0;
	}

	 
	array->unset_slots = 0;
	return xfarray_append(array, ptr);
}

 
uint64_t
xfarray_length(
	struct xfarray	*array)
{
	return array->nr;
}

 
static inline int
xfarray_find_data(
	struct xfarray	*array,
	xfarray_idx_t	*cur,
	loff_t		*pos)
{
	unsigned int	pgoff = offset_in_page(*pos);
	loff_t		end_pos = *pos + array->obj_size - 1;
	loff_t		new_pos;

	 
	if (pgoff != 0 && pgoff + array->obj_size - 1 < PAGE_SIZE)
		return 0;

	 
	new_pos = xfile_seek_data(array->xfile, end_pos);
	if (new_pos == -ENXIO)
		return -ENODATA;
	if (new_pos < 0)
		return new_pos;
	if (new_pos == end_pos)
		return 0;

	 
	new_pos = roundup_64(new_pos, array->obj_size);
	*cur = xfarray_idx(array, new_pos);
	*pos = xfarray_pos(array, *cur);
	return 0;
}

 
int
xfarray_load_next(
	struct xfarray	*array,
	xfarray_idx_t	*idx,
	void		*rec)
{
	xfarray_idx_t	cur = *idx;
	loff_t		pos = xfarray_pos(array, cur);
	int		error;

	do {
		if (cur >= array->nr)
			return -ENODATA;

		 
		error = xfarray_find_data(array, &cur, &pos);
		if (error)
			return error;
		error = xfarray_load(array, cur, rec);
		if (error)
			return error;

		cur++;
		pos += array->obj_size;
	} while (xfarray_element_is_null(array, rec));

	*idx = cur;
	return 0;
}

 

#ifdef DEBUG
# define xfarray_sort_bump_loads(si)	do { (si)->loads++; } while (0)
# define xfarray_sort_bump_stores(si)	do { (si)->stores++; } while (0)
# define xfarray_sort_bump_compares(si)	do { (si)->compares++; } while (0)
# define xfarray_sort_bump_heapsorts(si) do { (si)->heapsorts++; } while (0)
#else
# define xfarray_sort_bump_loads(si)
# define xfarray_sort_bump_stores(si)
# define xfarray_sort_bump_compares(si)
# define xfarray_sort_bump_heapsorts(si)
#endif  

 
static inline int
xfarray_sort_load(
	struct xfarray_sortinfo	*si,
	xfarray_idx_t		idx,
	void			*ptr)
{
	xfarray_sort_bump_loads(si);
	return xfarray_load(si->array, idx, ptr);
}

 
static inline int
xfarray_sort_store(
	struct xfarray_sortinfo	*si,
	xfarray_idx_t		idx,
	void			*ptr)
{
	xfarray_sort_bump_stores(si);
	return xfarray_store(si->array, idx, ptr);
}

 
static inline int
xfarray_sort_cmp(
	struct xfarray_sortinfo	*si,
	const void		*a,
	const void		*b)
{
	xfarray_sort_bump_compares(si);
	return si->cmp_fn(a, b);
}

 
static inline xfarray_idx_t *xfarray_sortinfo_lo(struct xfarray_sortinfo *si)
{
	return (xfarray_idx_t *)(si + 1);
}

 
static inline xfarray_idx_t *xfarray_sortinfo_hi(struct xfarray_sortinfo *si)
{
	return xfarray_sortinfo_lo(si) + si->max_stack_depth;
}

 
static inline size_t
xfarray_pivot_rec_sz(
	struct xfarray		*array)
{
	return round_up(array->obj_size, 8) + sizeof(xfarray_idx_t);
}

 
static inline int
xfarray_sortinfo_alloc(
	struct xfarray		*array,
	xfarray_cmp_fn		cmp_fn,
	unsigned int		flags,
	struct xfarray_sortinfo	**infop)
{
	struct xfarray_sortinfo	*si;
	size_t			nr_bytes = sizeof(struct xfarray_sortinfo);
	size_t			pivot_rec_sz = xfarray_pivot_rec_sz(array);
	int			max_stack_depth;

	 
	BUILD_BUG_ON(XFARRAY_QSORT_PIVOT_NR >= XFARRAY_ISORT_NR);

	 
	max_stack_depth = ilog2(array->nr) + 1 - (XFARRAY_ISORT_SHIFT - 1);
	if (max_stack_depth < 1)
		max_stack_depth = 1;

	 
	nr_bytes += max_stack_depth * sizeof(xfarray_idx_t) * 2;

	 
	nr_bytes += max_t(size_t,
			(XFARRAY_QSORT_PIVOT_NR + 1) * pivot_rec_sz,
			XFARRAY_ISORT_NR * array->obj_size);

	si = kvzalloc(nr_bytes, XCHK_GFP_FLAGS);
	if (!si)
		return -ENOMEM;

	si->array = array;
	si->cmp_fn = cmp_fn;
	si->flags = flags;
	si->max_stack_depth = max_stack_depth;
	si->max_stack_used = 1;

	xfarray_sortinfo_lo(si)[0] = 0;
	xfarray_sortinfo_hi(si)[0] = array->nr - 1;

	trace_xfarray_sort(si, nr_bytes);
	*infop = si;
	return 0;
}

 
static inline bool
xfarray_sort_terminated(
	struct xfarray_sortinfo	*si,
	int			*error)
{
	 
	cond_resched();

	if ((si->flags & XFARRAY_SORT_KILLABLE) &&
	    fatal_signal_pending(current)) {
		if (*error == 0)
			*error = -EINTR;
		return true;
	}
	return false;
}

 
static inline bool
xfarray_want_isort(
	struct xfarray_sortinfo *si,
	xfarray_idx_t		start,
	xfarray_idx_t		end)
{
	 
	return (end - start) < XFARRAY_ISORT_NR;
}

 
static inline void *xfarray_sortinfo_isort_scratch(struct xfarray_sortinfo *si)
{
	return xfarray_sortinfo_hi(si) + si->max_stack_depth;
}

 
STATIC int
xfarray_isort(
	struct xfarray_sortinfo	*si,
	xfarray_idx_t		lo,
	xfarray_idx_t		hi)
{
	void			*scratch = xfarray_sortinfo_isort_scratch(si);
	loff_t			lo_pos = xfarray_pos(si->array, lo);
	loff_t			len = xfarray_pos(si->array, hi - lo + 1);
	int			error;

	trace_xfarray_isort(si, lo, hi);

	xfarray_sort_bump_loads(si);
	error = xfile_obj_load(si->array->xfile, scratch, len, lo_pos);
	if (error)
		return error;

	xfarray_sort_bump_heapsorts(si);
	sort(scratch, hi - lo + 1, si->array->obj_size, si->cmp_fn, NULL);

	xfarray_sort_bump_stores(si);
	return xfile_obj_store(si->array->xfile, scratch, len, lo_pos);
}

 
static inline int
xfarray_sort_get_page(
	struct xfarray_sortinfo	*si,
	loff_t			pos,
	uint64_t		len)
{
	int			error;

	error = xfile_get_page(si->array->xfile, pos, len, &si->xfpage);
	if (error)
		return error;

	 
	si->page_kaddr = kmap_local_page(si->xfpage.page);
	return 0;
}

 
static inline int
xfarray_sort_put_page(
	struct xfarray_sortinfo	*si)
{
	if (!si->page_kaddr)
		return 0;

	kunmap_local(si->page_kaddr);
	si->page_kaddr = NULL;

	return xfile_put_page(si->array->xfile, &si->xfpage);
}

 
static inline bool
xfarray_want_pagesort(
	struct xfarray_sortinfo	*si,
	xfarray_idx_t		lo,
	xfarray_idx_t		hi)
{
	pgoff_t			lo_page;
	pgoff_t			hi_page;
	loff_t			end_pos;

	 
	lo_page = xfarray_pos(si->array, lo) >> PAGE_SHIFT;
	end_pos = xfarray_pos(si->array, hi) + si->array->obj_size - 1;
	hi_page = end_pos >> PAGE_SHIFT;

	return lo_page == hi_page;
}

 
STATIC int
xfarray_pagesort(
	struct xfarray_sortinfo	*si,
	xfarray_idx_t		lo,
	xfarray_idx_t		hi)
{
	void			*startp;
	loff_t			lo_pos = xfarray_pos(si->array, lo);
	uint64_t		len = xfarray_pos(si->array, hi - lo);
	int			error = 0;

	trace_xfarray_pagesort(si, lo, hi);

	xfarray_sort_bump_loads(si);
	error = xfarray_sort_get_page(si, lo_pos, len);
	if (error)
		return error;

	xfarray_sort_bump_heapsorts(si);
	startp = si->page_kaddr + offset_in_page(lo_pos);
	sort(startp, hi - lo + 1, si->array->obj_size, si->cmp_fn, NULL);

	xfarray_sort_bump_stores(si);
	return xfarray_sort_put_page(si);
}

 
static inline void *xfarray_sortinfo_pivot(struct xfarray_sortinfo *si)
{
	return xfarray_sortinfo_hi(si) + si->max_stack_depth;
}

 
static inline void *
xfarray_sortinfo_pivot_array(
	struct xfarray_sortinfo	*si)
{
	return xfarray_sortinfo_pivot(si) + si->array->obj_size;
}

 
static inline void *
xfarray_pivot_array_rec(
	void			*pa,
	size_t			pa_recsz,
	unsigned int		pa_idx)
{
	return pa + (pa_recsz * pa_idx);
}

 
static inline xfarray_idx_t *
xfarray_pivot_array_idx(
	void			*pa,
	size_t			pa_recsz,
	unsigned int		pa_idx)
{
	return xfarray_pivot_array_rec(pa, pa_recsz, pa_idx + 1) -
			sizeof(xfarray_idx_t);
}

 
STATIC int
xfarray_qsort_pivot(
	struct xfarray_sortinfo	*si,
	xfarray_idx_t		lo,
	xfarray_idx_t		hi)
{
	void			*pivot = xfarray_sortinfo_pivot(si);
	void			*parray = xfarray_sortinfo_pivot_array(si);
	void			*recp;
	xfarray_idx_t		*idxp;
	xfarray_idx_t		step = (hi - lo) / (XFARRAY_QSORT_PIVOT_NR - 1);
	size_t			pivot_rec_sz = xfarray_pivot_rec_sz(si->array);
	int			i, j;
	int			error;

	ASSERT(step > 0);

	 
	idxp = xfarray_pivot_array_idx(parray, pivot_rec_sz, 0);
	*idxp = lo;
	for (i = 1; i < XFARRAY_QSORT_PIVOT_NR - 1; i++) {
		idxp = xfarray_pivot_array_idx(parray, pivot_rec_sz, i);
		*idxp = lo + (i * step);
	}
	idxp = xfarray_pivot_array_idx(parray, pivot_rec_sz,
			XFARRAY_QSORT_PIVOT_NR - 1);
	*idxp = hi;

	 
	for (i = 0; i < XFARRAY_QSORT_PIVOT_NR; i++) {
		xfarray_idx_t	idx;

		recp = xfarray_pivot_array_rec(parray, pivot_rec_sz, i);
		idxp = xfarray_pivot_array_idx(parray, pivot_rec_sz, i);

		 
		if (likely(si->array->unset_slots == 0)) {
			error = xfarray_sort_load(si, *idxp, recp);
			if (error)
				return error;
			continue;
		}

		 
		idx = *idxp;
		xfarray_sort_bump_loads(si);
		error = xfarray_load_next(si->array, &idx, recp);
		if (error)
			return error;
	}

	xfarray_sort_bump_heapsorts(si);
	sort(parray, XFARRAY_QSORT_PIVOT_NR, pivot_rec_sz, si->cmp_fn, NULL);

	 
	recp = xfarray_pivot_array_rec(parray, pivot_rec_sz,
			XFARRAY_QSORT_PIVOT_NR / 2);
	memcpy(pivot, recp, si->array->obj_size);

	 
	idxp = xfarray_pivot_array_idx(parray, pivot_rec_sz,
			XFARRAY_QSORT_PIVOT_NR / 2);
	if (*idxp == lo)
		return 0;

	 
	for (i = 0, j = -1; i < XFARRAY_QSORT_PIVOT_NR; i++) {
		idxp = xfarray_pivot_array_idx(parray, pivot_rec_sz, i);
		if (*idxp == lo)
			j = i;
	}
	if (j < 0) {
		ASSERT(j >= 0);
		return -EFSCORRUPTED;
	}

	 
	error = xfarray_sort_store(si, lo, pivot);
	if (error)
		return error;

	recp = xfarray_pivot_array_rec(parray, pivot_rec_sz, j);
	idxp = xfarray_pivot_array_idx(parray, pivot_rec_sz,
			XFARRAY_QSORT_PIVOT_NR / 2);
	return xfarray_sort_store(si, *idxp, recp);
}

 
static inline int
xfarray_qsort_push(
	struct xfarray_sortinfo	*si,
	xfarray_idx_t		*si_lo,
	xfarray_idx_t		*si_hi,
	xfarray_idx_t		lo,
	xfarray_idx_t		hi)
{
	 
	if (si->stack_depth >= si->max_stack_depth - 1) {
		ASSERT(si->stack_depth < si->max_stack_depth - 1);
		return -EFSCORRUPTED;
	}

	si->max_stack_used = max_t(uint8_t, si->max_stack_used,
					    si->stack_depth + 2);

	si_lo[si->stack_depth + 1] = lo + 1;
	si_hi[si->stack_depth + 1] = si_hi[si->stack_depth];
	si_hi[si->stack_depth++] = lo - 1;

	 
	if (si_hi[si->stack_depth]     - si_lo[si->stack_depth] >
	    si_hi[si->stack_depth - 1] - si_lo[si->stack_depth - 1]) {
		swap(si_lo[si->stack_depth], si_lo[si->stack_depth - 1]);
		swap(si_hi[si->stack_depth], si_hi[si->stack_depth - 1]);
	}

	return 0;
}

 
static inline int
xfarray_sort_load_cached(
	struct xfarray_sortinfo	*si,
	xfarray_idx_t		idx,
	void			*ptr)
{
	loff_t			idx_pos = xfarray_pos(si->array, idx);
	pgoff_t			startpage;
	pgoff_t			endpage;
	int			error = 0;

	 
	startpage = idx_pos >> PAGE_SHIFT;
	endpage = (idx_pos + si->array->obj_size - 1) >> PAGE_SHIFT;
	if (startpage != endpage) {
		error = xfarray_sort_put_page(si);
		if (error)
			return error;

		if (xfarray_sort_terminated(si, &error))
			return error;

		return xfile_obj_load(si->array->xfile, ptr,
				si->array->obj_size, idx_pos);
	}

	 
	if (xfile_page_cached(&si->xfpage) &&
	    xfile_page_index(&si->xfpage) != startpage) {
		error = xfarray_sort_put_page(si);
		if (error)
			return error;
	}

	 
	if (!xfile_page_cached(&si->xfpage)) {
		if (xfarray_sort_terminated(si, &error))
			return error;

		error = xfarray_sort_get_page(si, startpage << PAGE_SHIFT,
				PAGE_SIZE);
		if (error)
			return error;
	}

	memcpy(ptr, si->page_kaddr + offset_in_page(idx_pos),
			si->array->obj_size);
	return 0;
}

 

 
#define QSORT_MAX_RECS		(1ULL << 63)

int
xfarray_sort(
	struct xfarray		*array,
	xfarray_cmp_fn		cmp_fn,
	unsigned int		flags)
{
	struct xfarray_sortinfo	*si;
	xfarray_idx_t		*si_lo, *si_hi;
	void			*pivot;
	void			*scratch = xfarray_scratch(array);
	xfarray_idx_t		lo, hi;
	int			error = 0;

	if (array->nr < 2)
		return 0;
	if (array->nr >= QSORT_MAX_RECS)
		return -E2BIG;

	error = xfarray_sortinfo_alloc(array, cmp_fn, flags, &si);
	if (error)
		return error;
	si_lo = xfarray_sortinfo_lo(si);
	si_hi = xfarray_sortinfo_hi(si);
	pivot = xfarray_sortinfo_pivot(si);

	while (si->stack_depth >= 0) {
		lo = si_lo[si->stack_depth];
		hi = si_hi[si->stack_depth];

		trace_xfarray_qsort(si, lo, hi);

		 
		if (lo >= hi) {
			si->stack_depth--;
			continue;
		}

		 
		if (xfarray_want_pagesort(si, lo, hi)) {
			error = xfarray_pagesort(si, lo, hi);
			if (error)
				goto out_free;
			si->stack_depth--;
			continue;
		}

		 
		if (xfarray_want_isort(si, lo, hi)) {
			error = xfarray_isort(si, lo, hi);
			if (error)
				goto out_free;
			si->stack_depth--;
			continue;
		}

		 
		error = xfarray_qsort_pivot(si, lo, hi);
		if (error)
			goto out_free;

		 
		while (lo < hi) {
			 
			error = xfarray_sort_load_cached(si, hi, scratch);
			if (error)
				goto out_free;
			while (xfarray_sort_cmp(si, scratch, pivot) >= 0 &&
								lo < hi) {
				hi--;
				error = xfarray_sort_load_cached(si, hi,
						scratch);
				if (error)
					goto out_free;
			}
			error = xfarray_sort_put_page(si);
			if (error)
				goto out_free;

			if (xfarray_sort_terminated(si, &error))
				goto out_free;

			 
			if (lo < hi) {
				error = xfarray_sort_store(si, lo++, scratch);
				if (error)
					goto out_free;
			}

			 
			error = xfarray_sort_load_cached(si, lo, scratch);
			if (error)
				goto out_free;
			while (xfarray_sort_cmp(si, scratch, pivot) <= 0 &&
								lo < hi) {
				lo++;
				error = xfarray_sort_load_cached(si, lo,
						scratch);
				if (error)
					goto out_free;
			}
			error = xfarray_sort_put_page(si);
			if (error)
				goto out_free;

			if (xfarray_sort_terminated(si, &error))
				goto out_free;

			 
			if (lo < hi) {
				error = xfarray_sort_store(si, hi--, scratch);
				if (error)
					goto out_free;
			}

			if (xfarray_sort_terminated(si, &error))
				goto out_free;
		}

		 
		error = xfarray_sort_store(si, lo, pivot);
		if (error)
			goto out_free;

		 
		error = xfarray_qsort_push(si, si_lo, si_hi, lo, hi);
		if (error)
			goto out_free;

		if (xfarray_sort_terminated(si, &error))
			goto out_free;
	}

out_free:
	trace_xfarray_sort_stats(si, error);
	kvfree(si);
	return error;
}
