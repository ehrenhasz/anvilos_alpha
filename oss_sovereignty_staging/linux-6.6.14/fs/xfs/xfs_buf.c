
 
#include "xfs.h"
#include <linux/backing-dev.h>
#include <linux/dax.h>

#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_trace.h"
#include "xfs_log.h"
#include "xfs_log_recover.h"
#include "xfs_log_priv.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_ag.h"

struct kmem_cache *xfs_buf_cache;

 

static int __xfs_buf_submit(struct xfs_buf *bp, bool wait);

static inline int
xfs_buf_submit(
	struct xfs_buf		*bp)
{
	return __xfs_buf_submit(bp, !(bp->b_flags & XBF_ASYNC));
}

static inline int
xfs_buf_is_vmapped(
	struct xfs_buf	*bp)
{
	 
	return bp->b_addr && bp->b_page_count > 1;
}

static inline int
xfs_buf_vmap_len(
	struct xfs_buf	*bp)
{
	return (bp->b_page_count * PAGE_SIZE);
}

 
static inline void
xfs_buf_ioacct_inc(
	struct xfs_buf	*bp)
{
	if (bp->b_flags & XBF_NO_IOACCT)
		return;

	ASSERT(bp->b_flags & XBF_ASYNC);
	spin_lock(&bp->b_lock);
	if (!(bp->b_state & XFS_BSTATE_IN_FLIGHT)) {
		bp->b_state |= XFS_BSTATE_IN_FLIGHT;
		percpu_counter_inc(&bp->b_target->bt_io_count);
	}
	spin_unlock(&bp->b_lock);
}

 
static inline void
__xfs_buf_ioacct_dec(
	struct xfs_buf	*bp)
{
	lockdep_assert_held(&bp->b_lock);

	if (bp->b_state & XFS_BSTATE_IN_FLIGHT) {
		bp->b_state &= ~XFS_BSTATE_IN_FLIGHT;
		percpu_counter_dec(&bp->b_target->bt_io_count);
	}
}

static inline void
xfs_buf_ioacct_dec(
	struct xfs_buf	*bp)
{
	spin_lock(&bp->b_lock);
	__xfs_buf_ioacct_dec(bp);
	spin_unlock(&bp->b_lock);
}

 
void
xfs_buf_stale(
	struct xfs_buf	*bp)
{
	ASSERT(xfs_buf_islocked(bp));

	bp->b_flags |= XBF_STALE;

	 
	bp->b_flags &= ~_XBF_DELWRI_Q;

	 
	spin_lock(&bp->b_lock);
	__xfs_buf_ioacct_dec(bp);

	atomic_set(&bp->b_lru_ref, 0);
	if (!(bp->b_state & XFS_BSTATE_DISPOSE) &&
	    (list_lru_del(&bp->b_target->bt_lru, &bp->b_lru)))
		atomic_dec(&bp->b_hold);

	ASSERT(atomic_read(&bp->b_hold) >= 1);
	spin_unlock(&bp->b_lock);
}

static int
xfs_buf_get_maps(
	struct xfs_buf		*bp,
	int			map_count)
{
	ASSERT(bp->b_maps == NULL);
	bp->b_map_count = map_count;

	if (map_count == 1) {
		bp->b_maps = &bp->__b_map;
		return 0;
	}

	bp->b_maps = kmem_zalloc(map_count * sizeof(struct xfs_buf_map),
				KM_NOFS);
	if (!bp->b_maps)
		return -ENOMEM;
	return 0;
}

 
static void
xfs_buf_free_maps(
	struct xfs_buf	*bp)
{
	if (bp->b_maps != &bp->__b_map) {
		kmem_free(bp->b_maps);
		bp->b_maps = NULL;
	}
}

static int
_xfs_buf_alloc(
	struct xfs_buftarg	*target,
	struct xfs_buf_map	*map,
	int			nmaps,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp)
{
	struct xfs_buf		*bp;
	int			error;
	int			i;

	*bpp = NULL;
	bp = kmem_cache_zalloc(xfs_buf_cache, GFP_NOFS | __GFP_NOFAIL);

	 
	flags &= ~(XBF_UNMAPPED | XBF_TRYLOCK | XBF_ASYNC | XBF_READ_AHEAD);

	atomic_set(&bp->b_hold, 1);
	atomic_set(&bp->b_lru_ref, 1);
	init_completion(&bp->b_iowait);
	INIT_LIST_HEAD(&bp->b_lru);
	INIT_LIST_HEAD(&bp->b_list);
	INIT_LIST_HEAD(&bp->b_li_list);
	sema_init(&bp->b_sema, 0);  
	spin_lock_init(&bp->b_lock);
	bp->b_target = target;
	bp->b_mount = target->bt_mount;
	bp->b_flags = flags;

	 
	error = xfs_buf_get_maps(bp, nmaps);
	if (error)  {
		kmem_cache_free(xfs_buf_cache, bp);
		return error;
	}

	bp->b_rhash_key = map[0].bm_bn;
	bp->b_length = 0;
	for (i = 0; i < nmaps; i++) {
		bp->b_maps[i].bm_bn = map[i].bm_bn;
		bp->b_maps[i].bm_len = map[i].bm_len;
		bp->b_length += map[i].bm_len;
	}

	atomic_set(&bp->b_pin_count, 0);
	init_waitqueue_head(&bp->b_waiters);

	XFS_STATS_INC(bp->b_mount, xb_create);
	trace_xfs_buf_init(bp, _RET_IP_);

	*bpp = bp;
	return 0;
}

static void
xfs_buf_free_pages(
	struct xfs_buf	*bp)
{
	uint		i;

	ASSERT(bp->b_flags & _XBF_PAGES);

	if (xfs_buf_is_vmapped(bp))
		vm_unmap_ram(bp->b_addr, bp->b_page_count);

	for (i = 0; i < bp->b_page_count; i++) {
		if (bp->b_pages[i])
			__free_page(bp->b_pages[i]);
	}
	mm_account_reclaimed_pages(bp->b_page_count);

	if (bp->b_pages != bp->b_page_array)
		kmem_free(bp->b_pages);
	bp->b_pages = NULL;
	bp->b_flags &= ~_XBF_PAGES;
}

static void
xfs_buf_free_callback(
	struct callback_head	*cb)
{
	struct xfs_buf		*bp = container_of(cb, struct xfs_buf, b_rcu);

	xfs_buf_free_maps(bp);
	kmem_cache_free(xfs_buf_cache, bp);
}

static void
xfs_buf_free(
	struct xfs_buf		*bp)
{
	trace_xfs_buf_free(bp, _RET_IP_);

	ASSERT(list_empty(&bp->b_lru));

	if (bp->b_flags & _XBF_PAGES)
		xfs_buf_free_pages(bp);
	else if (bp->b_flags & _XBF_KMEM)
		kmem_free(bp->b_addr);

	call_rcu(&bp->b_rcu, xfs_buf_free_callback);
}

static int
xfs_buf_alloc_kmem(
	struct xfs_buf	*bp,
	xfs_buf_flags_t	flags)
{
	xfs_km_flags_t	kmflag_mask = KM_NOFS;
	size_t		size = BBTOB(bp->b_length);

	 
	if (!(flags & XBF_READ))
		kmflag_mask |= KM_ZERO;

	bp->b_addr = kmem_alloc(size, kmflag_mask);
	if (!bp->b_addr)
		return -ENOMEM;

	if (((unsigned long)(bp->b_addr + size - 1) & PAGE_MASK) !=
	    ((unsigned long)bp->b_addr & PAGE_MASK)) {
		 
		kmem_free(bp->b_addr);
		bp->b_addr = NULL;
		return -ENOMEM;
	}
	bp->b_offset = offset_in_page(bp->b_addr);
	bp->b_pages = bp->b_page_array;
	bp->b_pages[0] = kmem_to_page(bp->b_addr);
	bp->b_page_count = 1;
	bp->b_flags |= _XBF_KMEM;
	return 0;
}

static int
xfs_buf_alloc_pages(
	struct xfs_buf	*bp,
	xfs_buf_flags_t	flags)
{
	gfp_t		gfp_mask = __GFP_NOWARN;
	long		filled = 0;

	if (flags & XBF_READ_AHEAD)
		gfp_mask |= __GFP_NORETRY;
	else
		gfp_mask |= GFP_NOFS;

	 
	bp->b_page_count = DIV_ROUND_UP(BBTOB(bp->b_length), PAGE_SIZE);
	if (bp->b_page_count <= XB_PAGES) {
		bp->b_pages = bp->b_page_array;
	} else {
		bp->b_pages = kzalloc(sizeof(struct page *) * bp->b_page_count,
					gfp_mask);
		if (!bp->b_pages)
			return -ENOMEM;
	}
	bp->b_flags |= _XBF_PAGES;

	 
	if (!(flags & XBF_READ))
		gfp_mask |= __GFP_ZERO;

	 
	for (;;) {
		long	last = filled;

		filled = alloc_pages_bulk_array(gfp_mask, bp->b_page_count,
						bp->b_pages);
		if (filled == bp->b_page_count) {
			XFS_STATS_INC(bp->b_mount, xb_page_found);
			break;
		}

		if (filled != last)
			continue;

		if (flags & XBF_READ_AHEAD) {
			xfs_buf_free_pages(bp);
			return -ENOMEM;
		}

		XFS_STATS_INC(bp->b_mount, xb_page_retries);
		memalloc_retry_wait(gfp_mask);
	}
	return 0;
}

 
STATIC int
_xfs_buf_map_pages(
	struct xfs_buf		*bp,
	xfs_buf_flags_t		flags)
{
	ASSERT(bp->b_flags & _XBF_PAGES);
	if (bp->b_page_count == 1) {
		 
		bp->b_addr = page_address(bp->b_pages[0]);
	} else if (flags & XBF_UNMAPPED) {
		bp->b_addr = NULL;
	} else {
		int retried = 0;
		unsigned nofs_flag;

		 
		nofs_flag = memalloc_nofs_save();
		do {
			bp->b_addr = vm_map_ram(bp->b_pages, bp->b_page_count,
						-1);
			if (bp->b_addr)
				break;
			vm_unmap_aliases();
		} while (retried++ <= 1);
		memalloc_nofs_restore(nofs_flag);

		if (!bp->b_addr)
			return -ENOMEM;
	}

	return 0;
}

 
static int
_xfs_buf_obj_cmp(
	struct rhashtable_compare_arg	*arg,
	const void			*obj)
{
	const struct xfs_buf_map	*map = arg->key;
	const struct xfs_buf		*bp = obj;

	 
	BUILD_BUG_ON(offsetof(struct xfs_buf_map, bm_bn) != 0);

	if (bp->b_rhash_key != map->bm_bn)
		return 1;

	if (unlikely(bp->b_length != map->bm_len)) {
		 
		if (!(map->bm_flags & XBM_LIVESCAN))
			ASSERT(bp->b_flags & XBF_STALE);
		return 1;
	}
	return 0;
}

static const struct rhashtable_params xfs_buf_hash_params = {
	.min_size		= 32,	 
	.nelem_hint		= 16,
	.key_len		= sizeof(xfs_daddr_t),
	.key_offset		= offsetof(struct xfs_buf, b_rhash_key),
	.head_offset		= offsetof(struct xfs_buf, b_rhash_head),
	.automatic_shrinking	= true,
	.obj_cmpfn		= _xfs_buf_obj_cmp,
};

int
xfs_buf_hash_init(
	struct xfs_perag	*pag)
{
	spin_lock_init(&pag->pag_buf_lock);
	return rhashtable_init(&pag->pag_buf_hash, &xfs_buf_hash_params);
}

void
xfs_buf_hash_destroy(
	struct xfs_perag	*pag)
{
	rhashtable_destroy(&pag->pag_buf_hash);
}

static int
xfs_buf_map_verify(
	struct xfs_buftarg	*btp,
	struct xfs_buf_map	*map)
{
	xfs_daddr_t		eofs;

	 
	ASSERT(!(BBTOB(map->bm_len) < btp->bt_meta_sectorsize));
	ASSERT(!(BBTOB(map->bm_bn) & (xfs_off_t)btp->bt_meta_sectormask));

	 
	eofs = XFS_FSB_TO_BB(btp->bt_mount, btp->bt_mount->m_sb.sb_dblocks);
	if (map->bm_bn < 0 || map->bm_bn >= eofs) {
		xfs_alert(btp->bt_mount,
			  "%s: daddr 0x%llx out of range, EOFS 0x%llx",
			  __func__, map->bm_bn, eofs);
		WARN_ON(1);
		return -EFSCORRUPTED;
	}
	return 0;
}

static int
xfs_buf_find_lock(
	struct xfs_buf          *bp,
	xfs_buf_flags_t		flags)
{
	if (flags & XBF_TRYLOCK) {
		if (!xfs_buf_trylock(bp)) {
			XFS_STATS_INC(bp->b_mount, xb_busy_locked);
			return -EAGAIN;
		}
	} else {
		xfs_buf_lock(bp);
		XFS_STATS_INC(bp->b_mount, xb_get_locked_waited);
	}

	 
	if (bp->b_flags & XBF_STALE) {
		if (flags & XBF_LIVESCAN) {
			xfs_buf_unlock(bp);
			return -ENOENT;
		}
		ASSERT((bp->b_flags & _XBF_DELWRI_Q) == 0);
		bp->b_flags &= _XBF_KMEM | _XBF_PAGES;
		bp->b_ops = NULL;
	}
	return 0;
}

static inline int
xfs_buf_lookup(
	struct xfs_perag	*pag,
	struct xfs_buf_map	*map,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp)
{
	struct xfs_buf          *bp;
	int			error;

	rcu_read_lock();
	bp = rhashtable_lookup(&pag->pag_buf_hash, map, xfs_buf_hash_params);
	if (!bp || !atomic_inc_not_zero(&bp->b_hold)) {
		rcu_read_unlock();
		return -ENOENT;
	}
	rcu_read_unlock();

	error = xfs_buf_find_lock(bp, flags);
	if (error) {
		xfs_buf_rele(bp);
		return error;
	}

	trace_xfs_buf_find(bp, flags, _RET_IP_);
	*bpp = bp;
	return 0;
}

 
static int
xfs_buf_find_insert(
	struct xfs_buftarg	*btp,
	struct xfs_perag	*pag,
	struct xfs_buf_map	*cmap,
	struct xfs_buf_map	*map,
	int			nmaps,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp)
{
	struct xfs_buf		*new_bp;
	struct xfs_buf		*bp;
	int			error;

	error = _xfs_buf_alloc(btp, map, nmaps, flags, &new_bp);
	if (error)
		goto out_drop_pag;

	 
	if (BBTOB(new_bp->b_length) >= PAGE_SIZE ||
	    xfs_buf_alloc_kmem(new_bp, flags) < 0) {
		error = xfs_buf_alloc_pages(new_bp, flags);
		if (error)
			goto out_free_buf;
	}

	spin_lock(&pag->pag_buf_lock);
	bp = rhashtable_lookup_get_insert_fast(&pag->pag_buf_hash,
			&new_bp->b_rhash_head, xfs_buf_hash_params);
	if (IS_ERR(bp)) {
		error = PTR_ERR(bp);
		spin_unlock(&pag->pag_buf_lock);
		goto out_free_buf;
	}
	if (bp) {
		 
		atomic_inc(&bp->b_hold);
		spin_unlock(&pag->pag_buf_lock);
		error = xfs_buf_find_lock(bp, flags);
		if (error)
			xfs_buf_rele(bp);
		else
			*bpp = bp;
		goto out_free_buf;
	}

	 
	new_bp->b_pag = pag;
	spin_unlock(&pag->pag_buf_lock);
	*bpp = new_bp;
	return 0;

out_free_buf:
	xfs_buf_free(new_bp);
out_drop_pag:
	xfs_perag_put(pag);
	return error;
}

 
int
xfs_buf_get_map(
	struct xfs_buftarg	*btp,
	struct xfs_buf_map	*map,
	int			nmaps,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp)
{
	struct xfs_perag	*pag;
	struct xfs_buf		*bp = NULL;
	struct xfs_buf_map	cmap = { .bm_bn = map[0].bm_bn };
	int			error;
	int			i;

	if (flags & XBF_LIVESCAN)
		cmap.bm_flags |= XBM_LIVESCAN;
	for (i = 0; i < nmaps; i++)
		cmap.bm_len += map[i].bm_len;

	error = xfs_buf_map_verify(btp, &cmap);
	if (error)
		return error;

	pag = xfs_perag_get(btp->bt_mount,
			    xfs_daddr_to_agno(btp->bt_mount, cmap.bm_bn));

	error = xfs_buf_lookup(pag, &cmap, flags, &bp);
	if (error && error != -ENOENT)
		goto out_put_perag;

	 
	if (unlikely(!bp)) {
		XFS_STATS_INC(btp->bt_mount, xb_miss_locked);

		if (flags & XBF_INCORE)
			goto out_put_perag;

		 
		error = xfs_buf_find_insert(btp, pag, &cmap, map, nmaps,
				flags, &bp);
		if (error)
			return error;
	} else {
		XFS_STATS_INC(btp->bt_mount, xb_get_locked);
		xfs_perag_put(pag);
	}

	 
	if (!bp->b_addr) {
		error = _xfs_buf_map_pages(bp, flags);
		if (unlikely(error)) {
			xfs_warn_ratelimited(btp->bt_mount,
				"%s: failed to map %u pages", __func__,
				bp->b_page_count);
			xfs_buf_relse(bp);
			return error;
		}
	}

	 
	if (!(flags & XBF_READ))
		xfs_buf_ioerror(bp, 0);

	XFS_STATS_INC(btp->bt_mount, xb_get);
	trace_xfs_buf_get(bp, flags, _RET_IP_);
	*bpp = bp;
	return 0;

out_put_perag:
	xfs_perag_put(pag);
	return error;
}

int
_xfs_buf_read(
	struct xfs_buf		*bp,
	xfs_buf_flags_t		flags)
{
	ASSERT(!(flags & XBF_WRITE));
	ASSERT(bp->b_maps[0].bm_bn != XFS_BUF_DADDR_NULL);

	bp->b_flags &= ~(XBF_WRITE | XBF_ASYNC | XBF_READ_AHEAD | XBF_DONE);
	bp->b_flags |= flags & (XBF_READ | XBF_ASYNC | XBF_READ_AHEAD);

	return xfs_buf_submit(bp);
}

 
int
xfs_buf_reverify(
	struct xfs_buf		*bp,
	const struct xfs_buf_ops *ops)
{
	ASSERT(bp->b_flags & XBF_DONE);
	ASSERT(bp->b_error == 0);

	if (!ops || bp->b_ops)
		return 0;

	bp->b_ops = ops;
	bp->b_ops->verify_read(bp);
	if (bp->b_error)
		bp->b_flags &= ~XBF_DONE;
	return bp->b_error;
}

int
xfs_buf_read_map(
	struct xfs_buftarg	*target,
	struct xfs_buf_map	*map,
	int			nmaps,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp,
	const struct xfs_buf_ops *ops,
	xfs_failaddr_t		fa)
{
	struct xfs_buf		*bp;
	int			error;

	flags |= XBF_READ;
	*bpp = NULL;

	error = xfs_buf_get_map(target, map, nmaps, flags, &bp);
	if (error)
		return error;

	trace_xfs_buf_read(bp, flags, _RET_IP_);

	if (!(bp->b_flags & XBF_DONE)) {
		 
		XFS_STATS_INC(target->bt_mount, xb_get_read);
		bp->b_ops = ops;
		error = _xfs_buf_read(bp, flags);

		 
		if (flags & XBF_ASYNC)
			return 0;
	} else {
		 
		error = xfs_buf_reverify(bp, ops);

		 
		if (flags & XBF_ASYNC) {
			xfs_buf_relse(bp);
			return 0;
		}

		 
		bp->b_flags &= ~XBF_READ;
		ASSERT(bp->b_ops != NULL || ops == NULL);
	}

	 
	if (error) {
		 
		if (!xlog_is_shutdown(target->bt_mount->m_log))
			xfs_buf_ioerror_alert(bp, fa);

		bp->b_flags &= ~XBF_DONE;
		xfs_buf_stale(bp);
		xfs_buf_relse(bp);

		 
		if (error == -EFSBADCRC)
			error = -EFSCORRUPTED;
		return error;
	}

	*bpp = bp;
	return 0;
}

 
void
xfs_buf_readahead_map(
	struct xfs_buftarg	*target,
	struct xfs_buf_map	*map,
	int			nmaps,
	const struct xfs_buf_ops *ops)
{
	struct xfs_buf		*bp;

	xfs_buf_read_map(target, map, nmaps,
		     XBF_TRYLOCK | XBF_ASYNC | XBF_READ_AHEAD, &bp, ops,
		     __this_address);
}

 
int
xfs_buf_read_uncached(
	struct xfs_buftarg	*target,
	xfs_daddr_t		daddr,
	size_t			numblks,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp,
	const struct xfs_buf_ops *ops)
{
	struct xfs_buf		*bp;
	int			error;

	*bpp = NULL;

	error = xfs_buf_get_uncached(target, numblks, flags, &bp);
	if (error)
		return error;

	 
	ASSERT(bp->b_map_count == 1);
	bp->b_rhash_key = XFS_BUF_DADDR_NULL;
	bp->b_maps[0].bm_bn = daddr;
	bp->b_flags |= XBF_READ;
	bp->b_ops = ops;

	xfs_buf_submit(bp);
	if (bp->b_error) {
		error = bp->b_error;
		xfs_buf_relse(bp);
		return error;
	}

	*bpp = bp;
	return 0;
}

int
xfs_buf_get_uncached(
	struct xfs_buftarg	*target,
	size_t			numblks,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp)
{
	int			error;
	struct xfs_buf		*bp;
	DEFINE_SINGLE_BUF_MAP(map, XFS_BUF_DADDR_NULL, numblks);

	*bpp = NULL;

	 
	error = _xfs_buf_alloc(target, &map, 1, flags & XBF_NO_IOACCT, &bp);
	if (error)
		return error;

	error = xfs_buf_alloc_pages(bp, flags);
	if (error)
		goto fail_free_buf;

	error = _xfs_buf_map_pages(bp, 0);
	if (unlikely(error)) {
		xfs_warn(target->bt_mount,
			"%s: failed to map pages", __func__);
		goto fail_free_buf;
	}

	trace_xfs_buf_get_uncached(bp, _RET_IP_);
	*bpp = bp;
	return 0;

fail_free_buf:
	xfs_buf_free(bp);
	return error;
}

 
void
xfs_buf_hold(
	struct xfs_buf		*bp)
{
	trace_xfs_buf_hold(bp, _RET_IP_);
	atomic_inc(&bp->b_hold);
}

 
void
xfs_buf_rele(
	struct xfs_buf		*bp)
{
	struct xfs_perag	*pag = bp->b_pag;
	bool			release;
	bool			freebuf = false;

	trace_xfs_buf_rele(bp, _RET_IP_);

	if (!pag) {
		ASSERT(list_empty(&bp->b_lru));
		if (atomic_dec_and_test(&bp->b_hold)) {
			xfs_buf_ioacct_dec(bp);
			xfs_buf_free(bp);
		}
		return;
	}

	ASSERT(atomic_read(&bp->b_hold) > 0);

	 
	spin_lock(&bp->b_lock);
	release = atomic_dec_and_lock(&bp->b_hold, &pag->pag_buf_lock);
	if (!release) {
		 
		if ((atomic_read(&bp->b_hold) == 1) && !list_empty(&bp->b_lru))
			__xfs_buf_ioacct_dec(bp);
		goto out_unlock;
	}

	 
	__xfs_buf_ioacct_dec(bp);
	if (!(bp->b_flags & XBF_STALE) && atomic_read(&bp->b_lru_ref)) {
		 
		if (list_lru_add(&bp->b_target->bt_lru, &bp->b_lru)) {
			bp->b_state &= ~XFS_BSTATE_DISPOSE;
			atomic_inc(&bp->b_hold);
		}
		spin_unlock(&pag->pag_buf_lock);
	} else {
		 
		if (!(bp->b_state & XFS_BSTATE_DISPOSE)) {
			list_lru_del(&bp->b_target->bt_lru, &bp->b_lru);
		} else {
			ASSERT(list_empty(&bp->b_lru));
		}

		ASSERT(!(bp->b_flags & _XBF_DELWRI_Q));
		rhashtable_remove_fast(&pag->pag_buf_hash, &bp->b_rhash_head,
				       xfs_buf_hash_params);
		spin_unlock(&pag->pag_buf_lock);
		xfs_perag_put(pag);
		freebuf = true;
	}

out_unlock:
	spin_unlock(&bp->b_lock);

	if (freebuf)
		xfs_buf_free(bp);
}


 
int
xfs_buf_trylock(
	struct xfs_buf		*bp)
{
	int			locked;

	locked = down_trylock(&bp->b_sema) == 0;
	if (locked)
		trace_xfs_buf_trylock(bp, _RET_IP_);
	else
		trace_xfs_buf_trylock_fail(bp, _RET_IP_);
	return locked;
}

 
void
xfs_buf_lock(
	struct xfs_buf		*bp)
{
	trace_xfs_buf_lock(bp, _RET_IP_);

	if (atomic_read(&bp->b_pin_count) && (bp->b_flags & XBF_STALE))
		xfs_log_force(bp->b_mount, 0);
	down(&bp->b_sema);

	trace_xfs_buf_lock_done(bp, _RET_IP_);
}

void
xfs_buf_unlock(
	struct xfs_buf		*bp)
{
	ASSERT(xfs_buf_islocked(bp));

	up(&bp->b_sema);
	trace_xfs_buf_unlock(bp, _RET_IP_);
}

STATIC void
xfs_buf_wait_unpin(
	struct xfs_buf		*bp)
{
	DECLARE_WAITQUEUE	(wait, current);

	if (atomic_read(&bp->b_pin_count) == 0)
		return;

	add_wait_queue(&bp->b_waiters, &wait);
	for (;;) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (atomic_read(&bp->b_pin_count) == 0)
			break;
		io_schedule();
	}
	remove_wait_queue(&bp->b_waiters, &wait);
	set_current_state(TASK_RUNNING);
}

static void
xfs_buf_ioerror_alert_ratelimited(
	struct xfs_buf		*bp)
{
	static unsigned long	lasttime;
	static struct xfs_buftarg *lasttarg;

	if (bp->b_target != lasttarg ||
	    time_after(jiffies, (lasttime + 5*HZ))) {
		lasttime = jiffies;
		xfs_buf_ioerror_alert(bp, __this_address);
	}
	lasttarg = bp->b_target;
}

 
static bool
xfs_buf_ioerror_permanent(
	struct xfs_buf		*bp,
	struct xfs_error_cfg	*cfg)
{
	struct xfs_mount	*mp = bp->b_mount;

	if (cfg->max_retries != XFS_ERR_RETRY_FOREVER &&
	    ++bp->b_retries > cfg->max_retries)
		return true;
	if (cfg->retry_timeout != XFS_ERR_RETRY_FOREVER &&
	    time_after(jiffies, cfg->retry_timeout + bp->b_first_retry_time))
		return true;

	 
	if (xfs_is_unmounting(mp) && mp->m_fail_unmount)
		return true;

	return false;
}

 
static bool
xfs_buf_ioend_handle_error(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_error_cfg	*cfg;

	 
	if (xlog_is_shutdown(mp->m_log))
		goto out_stale;

	xfs_buf_ioerror_alert_ratelimited(bp);

	 
	if (bp->b_flags & _XBF_LOGRECOVERY) {
		xfs_force_shutdown(mp, SHUTDOWN_META_IO_ERROR);
		return false;
	}

	 
	if (!(bp->b_flags & XBF_ASYNC))
		goto out_stale;

	trace_xfs_buf_iodone_async(bp, _RET_IP_);

	cfg = xfs_error_get_cfg(mp, XFS_ERR_METADATA, bp->b_error);
	if (bp->b_last_error != bp->b_error ||
	    !(bp->b_flags & (XBF_STALE | XBF_WRITE_FAIL))) {
		bp->b_last_error = bp->b_error;
		if (cfg->retry_timeout != XFS_ERR_RETRY_FOREVER &&
		    !bp->b_first_retry_time)
			bp->b_first_retry_time = jiffies;
		goto resubmit;
	}

	 
	if (xfs_buf_ioerror_permanent(bp, cfg)) {
		xfs_force_shutdown(mp, SHUTDOWN_META_IO_ERROR);
		goto out_stale;
	}

	 
	if (bp->b_flags & _XBF_INODES)
		xfs_buf_inode_io_fail(bp);
	else if (bp->b_flags & _XBF_DQUOTS)
		xfs_buf_dquot_io_fail(bp);
	else
		ASSERT(list_empty(&bp->b_li_list));
	xfs_buf_ioerror(bp, 0);
	xfs_buf_relse(bp);
	return true;

resubmit:
	xfs_buf_ioerror(bp, 0);
	bp->b_flags |= (XBF_DONE | XBF_WRITE_FAIL);
	xfs_buf_submit(bp);
	return true;
out_stale:
	xfs_buf_stale(bp);
	bp->b_flags |= XBF_DONE;
	bp->b_flags &= ~XBF_WRITE;
	trace_xfs_buf_error_relse(bp, _RET_IP_);
	return false;
}

static void
xfs_buf_ioend(
	struct xfs_buf	*bp)
{
	trace_xfs_buf_iodone(bp, _RET_IP_);

	 
	if (!bp->b_error && bp->b_io_error)
		xfs_buf_ioerror(bp, bp->b_io_error);

	if (bp->b_flags & XBF_READ) {
		if (!bp->b_error && bp->b_ops)
			bp->b_ops->verify_read(bp);
		if (!bp->b_error)
			bp->b_flags |= XBF_DONE;
	} else {
		if (!bp->b_error) {
			bp->b_flags &= ~XBF_WRITE_FAIL;
			bp->b_flags |= XBF_DONE;
		}

		if (unlikely(bp->b_error) && xfs_buf_ioend_handle_error(bp))
			return;

		 
		bp->b_last_error = 0;
		bp->b_retries = 0;
		bp->b_first_retry_time = 0;

		 
		if (bp->b_log_item)
			xfs_buf_item_done(bp);

		if (bp->b_flags & _XBF_INODES)
			xfs_buf_inode_iodone(bp);
		else if (bp->b_flags & _XBF_DQUOTS)
			xfs_buf_dquot_iodone(bp);

	}

	bp->b_flags &= ~(XBF_READ | XBF_WRITE | XBF_READ_AHEAD |
			 _XBF_LOGRECOVERY);

	if (bp->b_flags & XBF_ASYNC)
		xfs_buf_relse(bp);
	else
		complete(&bp->b_iowait);
}

static void
xfs_buf_ioend_work(
	struct work_struct	*work)
{
	struct xfs_buf		*bp =
		container_of(work, struct xfs_buf, b_ioend_work);

	xfs_buf_ioend(bp);
}

static void
xfs_buf_ioend_async(
	struct xfs_buf	*bp)
{
	INIT_WORK(&bp->b_ioend_work, xfs_buf_ioend_work);
	queue_work(bp->b_mount->m_buf_workqueue, &bp->b_ioend_work);
}

void
__xfs_buf_ioerror(
	struct xfs_buf		*bp,
	int			error,
	xfs_failaddr_t		failaddr)
{
	ASSERT(error <= 0 && error >= -1000);
	bp->b_error = error;
	trace_xfs_buf_ioerror(bp, error, failaddr);
}

void
xfs_buf_ioerror_alert(
	struct xfs_buf		*bp,
	xfs_failaddr_t		func)
{
	xfs_buf_alert_ratelimited(bp, "XFS: metadata IO error",
		"metadata I/O error in \"%pS\" at daddr 0x%llx len %d error %d",
				  func, (uint64_t)xfs_buf_daddr(bp),
				  bp->b_length, -bp->b_error);
}

 
void
xfs_buf_ioend_fail(
	struct xfs_buf	*bp)
{
	bp->b_flags &= ~XBF_DONE;
	xfs_buf_stale(bp);
	xfs_buf_ioerror(bp, -EIO);
	xfs_buf_ioend(bp);
}

int
xfs_bwrite(
	struct xfs_buf		*bp)
{
	int			error;

	ASSERT(xfs_buf_islocked(bp));

	bp->b_flags |= XBF_WRITE;
	bp->b_flags &= ~(XBF_ASYNC | XBF_READ | _XBF_DELWRI_Q |
			 XBF_DONE);

	error = xfs_buf_submit(bp);
	if (error)
		xfs_force_shutdown(bp->b_mount, SHUTDOWN_META_IO_ERROR);
	return error;
}

static void
xfs_buf_bio_end_io(
	struct bio		*bio)
{
	struct xfs_buf		*bp = (struct xfs_buf *)bio->bi_private;

	if (!bio->bi_status &&
	    (bp->b_flags & XBF_WRITE) && (bp->b_flags & XBF_ASYNC) &&
	    XFS_TEST_ERROR(false, bp->b_mount, XFS_ERRTAG_BUF_IOERROR))
		bio->bi_status = BLK_STS_IOERR;

	 
	if (bio->bi_status) {
		int error = blk_status_to_errno(bio->bi_status);

		cmpxchg(&bp->b_io_error, 0, error);
	}

	if (!bp->b_error && xfs_buf_is_vmapped(bp) && (bp->b_flags & XBF_READ))
		invalidate_kernel_vmap_range(bp->b_addr, xfs_buf_vmap_len(bp));

	if (atomic_dec_and_test(&bp->b_io_remaining) == 1)
		xfs_buf_ioend_async(bp);
	bio_put(bio);
}

static void
xfs_buf_ioapply_map(
	struct xfs_buf	*bp,
	int		map,
	int		*buf_offset,
	int		*count,
	blk_opf_t	op)
{
	int		page_index;
	unsigned int	total_nr_pages = bp->b_page_count;
	int		nr_pages;
	struct bio	*bio;
	sector_t	sector =  bp->b_maps[map].bm_bn;
	int		size;
	int		offset;

	 
	page_index = 0;
	offset = *buf_offset;
	while (offset >= PAGE_SIZE) {
		page_index++;
		offset -= PAGE_SIZE;
	}

	 
	size = min_t(int, BBTOB(bp->b_maps[map].bm_len), *count);
	*count -= size;
	*buf_offset += size;

next_chunk:
	atomic_inc(&bp->b_io_remaining);
	nr_pages = bio_max_segs(total_nr_pages);

	bio = bio_alloc(bp->b_target->bt_bdev, nr_pages, op, GFP_NOIO);
	bio->bi_iter.bi_sector = sector;
	bio->bi_end_io = xfs_buf_bio_end_io;
	bio->bi_private = bp;

	for (; size && nr_pages; nr_pages--, page_index++) {
		int	rbytes, nbytes = PAGE_SIZE - offset;

		if (nbytes > size)
			nbytes = size;

		rbytes = bio_add_page(bio, bp->b_pages[page_index], nbytes,
				      offset);
		if (rbytes < nbytes)
			break;

		offset = 0;
		sector += BTOBB(nbytes);
		size -= nbytes;
		total_nr_pages--;
	}

	if (likely(bio->bi_iter.bi_size)) {
		if (xfs_buf_is_vmapped(bp)) {
			flush_kernel_vmap_range(bp->b_addr,
						xfs_buf_vmap_len(bp));
		}
		submit_bio(bio);
		if (size)
			goto next_chunk;
	} else {
		 
		atomic_dec(&bp->b_io_remaining);
		xfs_buf_ioerror(bp, -EIO);
		bio_put(bio);
	}

}

STATIC void
_xfs_buf_ioapply(
	struct xfs_buf	*bp)
{
	struct blk_plug	plug;
	blk_opf_t	op;
	int		offset;
	int		size;
	int		i;

	 
	bp->b_error = 0;

	if (bp->b_flags & XBF_WRITE) {
		op = REQ_OP_WRITE;

		 
		if (bp->b_ops) {
			bp->b_ops->verify_write(bp);
			if (bp->b_error) {
				xfs_force_shutdown(bp->b_mount,
						   SHUTDOWN_CORRUPT_INCORE);
				return;
			}
		} else if (bp->b_rhash_key != XFS_BUF_DADDR_NULL) {
			struct xfs_mount *mp = bp->b_mount;

			 
			if (xfs_has_crc(mp)) {
				xfs_warn(mp,
					"%s: no buf ops on daddr 0x%llx len %d",
					__func__, xfs_buf_daddr(bp),
					bp->b_length);
				xfs_hex_dump(bp->b_addr,
						XFS_CORRUPTION_DUMP_LEN);
				dump_stack();
			}
		}
	} else {
		op = REQ_OP_READ;
		if (bp->b_flags & XBF_READ_AHEAD)
			op |= REQ_RAHEAD;
	}

	 
	op |= REQ_META;

	 
	offset = bp->b_offset;
	size = BBTOB(bp->b_length);
	blk_start_plug(&plug);
	for (i = 0; i < bp->b_map_count; i++) {
		xfs_buf_ioapply_map(bp, i, &offset, &size, op);
		if (bp->b_error)
			break;
		if (size <= 0)
			break;	 
	}
	blk_finish_plug(&plug);
}

 
static int
xfs_buf_iowait(
	struct xfs_buf	*bp)
{
	ASSERT(!(bp->b_flags & XBF_ASYNC));

	trace_xfs_buf_iowait(bp, _RET_IP_);
	wait_for_completion(&bp->b_iowait);
	trace_xfs_buf_iowait_done(bp, _RET_IP_);

	return bp->b_error;
}

 
static int
__xfs_buf_submit(
	struct xfs_buf	*bp,
	bool		wait)
{
	int		error = 0;

	trace_xfs_buf_submit(bp, _RET_IP_);

	ASSERT(!(bp->b_flags & _XBF_DELWRI_Q));

	 
	if (bp->b_mount->m_log &&
	    xlog_is_shutdown(bp->b_mount->m_log)) {
		xfs_buf_ioend_fail(bp);
		return -EIO;
	}

	 
	xfs_buf_hold(bp);

	if (bp->b_flags & XBF_WRITE)
		xfs_buf_wait_unpin(bp);

	 
	bp->b_io_error = 0;

	 
	atomic_set(&bp->b_io_remaining, 1);
	if (bp->b_flags & XBF_ASYNC)
		xfs_buf_ioacct_inc(bp);
	_xfs_buf_ioapply(bp);

	 
	if (atomic_dec_and_test(&bp->b_io_remaining) == 1) {
		if (bp->b_error || !(bp->b_flags & XBF_ASYNC))
			xfs_buf_ioend(bp);
		else
			xfs_buf_ioend_async(bp);
	}

	if (wait)
		error = xfs_buf_iowait(bp);

	 
	xfs_buf_rele(bp);
	return error;
}

void *
xfs_buf_offset(
	struct xfs_buf		*bp,
	size_t			offset)
{
	struct page		*page;

	if (bp->b_addr)
		return bp->b_addr + offset;

	page = bp->b_pages[offset >> PAGE_SHIFT];
	return page_address(page) + (offset & (PAGE_SIZE-1));
}

void
xfs_buf_zero(
	struct xfs_buf		*bp,
	size_t			boff,
	size_t			bsize)
{
	size_t			bend;

	bend = boff + bsize;
	while (boff < bend) {
		struct page	*page;
		int		page_index, page_offset, csize;

		page_index = (boff + bp->b_offset) >> PAGE_SHIFT;
		page_offset = (boff + bp->b_offset) & ~PAGE_MASK;
		page = bp->b_pages[page_index];
		csize = min_t(size_t, PAGE_SIZE - page_offset,
				      BBTOB(bp->b_length) - boff);

		ASSERT((csize + page_offset) <= PAGE_SIZE);

		memset(page_address(page) + page_offset, 0, csize);

		boff += csize;
	}
}

 
void
__xfs_buf_mark_corrupt(
	struct xfs_buf		*bp,
	xfs_failaddr_t		fa)
{
	ASSERT(bp->b_flags & XBF_DONE);

	xfs_buf_corruption_error(bp, fa);
	xfs_buf_stale(bp);
}

 

 
static enum lru_status
xfs_buftarg_drain_rele(
	struct list_head	*item,
	struct list_lru_one	*lru,
	spinlock_t		*lru_lock,
	void			*arg)

{
	struct xfs_buf		*bp = container_of(item, struct xfs_buf, b_lru);
	struct list_head	*dispose = arg;

	if (atomic_read(&bp->b_hold) > 1) {
		 
		trace_xfs_buf_drain_buftarg(bp, _RET_IP_);
		return LRU_SKIP;
	}
	if (!spin_trylock(&bp->b_lock))
		return LRU_SKIP;

	 
	atomic_set(&bp->b_lru_ref, 0);
	bp->b_state |= XFS_BSTATE_DISPOSE;
	list_lru_isolate_move(lru, item, dispose);
	spin_unlock(&bp->b_lock);
	return LRU_REMOVED;
}

 
void
xfs_buftarg_wait(
	struct xfs_buftarg	*btp)
{
	 
	while (percpu_counter_sum(&btp->bt_io_count))
		delay(100);
	flush_workqueue(btp->bt_mount->m_buf_workqueue);
}

void
xfs_buftarg_drain(
	struct xfs_buftarg	*btp)
{
	LIST_HEAD(dispose);
	int			loop = 0;
	bool			write_fail = false;

	xfs_buftarg_wait(btp);

	 
	while (list_lru_count(&btp->bt_lru)) {
		list_lru_walk(&btp->bt_lru, xfs_buftarg_drain_rele,
			      &dispose, LONG_MAX);

		while (!list_empty(&dispose)) {
			struct xfs_buf *bp;
			bp = list_first_entry(&dispose, struct xfs_buf, b_lru);
			list_del_init(&bp->b_lru);
			if (bp->b_flags & XBF_WRITE_FAIL) {
				write_fail = true;
				xfs_buf_alert_ratelimited(bp,
					"XFS: Corruption Alert",
"Corruption Alert: Buffer at daddr 0x%llx had permanent write failures!",
					(long long)xfs_buf_daddr(bp));
			}
			xfs_buf_rele(bp);
		}
		if (loop++ != 0)
			delay(100);
	}

	 
	if (write_fail) {
		ASSERT(xlog_is_shutdown(btp->bt_mount->m_log));
		xfs_alert(btp->bt_mount,
	      "Please run xfs_repair to determine the extent of the problem.");
	}
}

static enum lru_status
xfs_buftarg_isolate(
	struct list_head	*item,
	struct list_lru_one	*lru,
	spinlock_t		*lru_lock,
	void			*arg)
{
	struct xfs_buf		*bp = container_of(item, struct xfs_buf, b_lru);
	struct list_head	*dispose = arg;

	 
	if (!spin_trylock(&bp->b_lock))
		return LRU_SKIP;
	 
	if (atomic_add_unless(&bp->b_lru_ref, -1, 0)) {
		spin_unlock(&bp->b_lock);
		return LRU_ROTATE;
	}

	bp->b_state |= XFS_BSTATE_DISPOSE;
	list_lru_isolate_move(lru, item, dispose);
	spin_unlock(&bp->b_lock);
	return LRU_REMOVED;
}

static unsigned long
xfs_buftarg_shrink_scan(
	struct shrinker		*shrink,
	struct shrink_control	*sc)
{
	struct xfs_buftarg	*btp = container_of(shrink,
					struct xfs_buftarg, bt_shrinker);
	LIST_HEAD(dispose);
	unsigned long		freed;

	freed = list_lru_shrink_walk(&btp->bt_lru, sc,
				     xfs_buftarg_isolate, &dispose);

	while (!list_empty(&dispose)) {
		struct xfs_buf *bp;
		bp = list_first_entry(&dispose, struct xfs_buf, b_lru);
		list_del_init(&bp->b_lru);
		xfs_buf_rele(bp);
	}

	return freed;
}

static unsigned long
xfs_buftarg_shrink_count(
	struct shrinker		*shrink,
	struct shrink_control	*sc)
{
	struct xfs_buftarg	*btp = container_of(shrink,
					struct xfs_buftarg, bt_shrinker);
	return list_lru_shrink_count(&btp->bt_lru, sc);
}

void
xfs_free_buftarg(
	struct xfs_buftarg	*btp)
{
	struct block_device	*bdev = btp->bt_bdev;

	unregister_shrinker(&btp->bt_shrinker);
	ASSERT(percpu_counter_sum(&btp->bt_io_count) == 0);
	percpu_counter_destroy(&btp->bt_io_count);
	list_lru_destroy(&btp->bt_lru);

	fs_put_dax(btp->bt_daxdev, btp->bt_mount);
	 
	if (bdev != btp->bt_mount->m_super->s_bdev)
		blkdev_put(bdev, btp->bt_mount->m_super);

	kmem_free(btp);
}

int
xfs_setsize_buftarg(
	xfs_buftarg_t		*btp,
	unsigned int		sectorsize)
{
	 
	btp->bt_meta_sectorsize = sectorsize;
	btp->bt_meta_sectormask = sectorsize - 1;

	if (set_blocksize(btp->bt_bdev, sectorsize)) {
		xfs_warn(btp->bt_mount,
			"Cannot set_blocksize to %u on device %pg",
			sectorsize, btp->bt_bdev);
		return -EINVAL;
	}

	 
	btp->bt_logical_sectorsize = bdev_logical_block_size(btp->bt_bdev);
	btp->bt_logical_sectormask = bdev_logical_block_size(btp->bt_bdev) - 1;

	return 0;
}

 
STATIC int
xfs_setsize_buftarg_early(
	xfs_buftarg_t		*btp,
	struct block_device	*bdev)
{
	return xfs_setsize_buftarg(btp, bdev_logical_block_size(bdev));
}

struct xfs_buftarg *
xfs_alloc_buftarg(
	struct xfs_mount	*mp,
	struct block_device	*bdev)
{
	xfs_buftarg_t		*btp;
	const struct dax_holder_operations *ops = NULL;

#if defined(CONFIG_FS_DAX) && defined(CONFIG_MEMORY_FAILURE)
	ops = &xfs_dax_holder_operations;
#endif
	btp = kmem_zalloc(sizeof(*btp), KM_NOFS);

	btp->bt_mount = mp;
	btp->bt_dev =  bdev->bd_dev;
	btp->bt_bdev = bdev;
	btp->bt_daxdev = fs_dax_get_by_bdev(bdev, &btp->bt_dax_part_off,
					    mp, ops);

	 
	ratelimit_state_init(&btp->bt_ioerror_rl, 30 * HZ,
			     DEFAULT_RATELIMIT_BURST);

	if (xfs_setsize_buftarg_early(btp, bdev))
		goto error_free;

	if (list_lru_init(&btp->bt_lru))
		goto error_free;

	if (percpu_counter_init(&btp->bt_io_count, 0, GFP_KERNEL))
		goto error_lru;

	btp->bt_shrinker.count_objects = xfs_buftarg_shrink_count;
	btp->bt_shrinker.scan_objects = xfs_buftarg_shrink_scan;
	btp->bt_shrinker.seeks = DEFAULT_SEEKS;
	btp->bt_shrinker.flags = SHRINKER_NUMA_AWARE;
	if (register_shrinker(&btp->bt_shrinker, "xfs-buf:%s",
			      mp->m_super->s_id))
		goto error_pcpu;
	return btp;

error_pcpu:
	percpu_counter_destroy(&btp->bt_io_count);
error_lru:
	list_lru_destroy(&btp->bt_lru);
error_free:
	kmem_free(btp);
	return NULL;
}

 
void
xfs_buf_delwri_cancel(
	struct list_head	*list)
{
	struct xfs_buf		*bp;

	while (!list_empty(list)) {
		bp = list_first_entry(list, struct xfs_buf, b_list);

		xfs_buf_lock(bp);
		bp->b_flags &= ~_XBF_DELWRI_Q;
		list_del_init(&bp->b_list);
		xfs_buf_relse(bp);
	}
}

 
bool
xfs_buf_delwri_queue(
	struct xfs_buf		*bp,
	struct list_head	*list)
{
	ASSERT(xfs_buf_islocked(bp));
	ASSERT(!(bp->b_flags & XBF_READ));

	 
	if (bp->b_flags & _XBF_DELWRI_Q) {
		trace_xfs_buf_delwri_queued(bp, _RET_IP_);
		return false;
	}

	trace_xfs_buf_delwri_queue(bp, _RET_IP_);

	 
	bp->b_flags |= _XBF_DELWRI_Q;
	if (list_empty(&bp->b_list)) {
		atomic_inc(&bp->b_hold);
		list_add_tail(&bp->b_list, list);
	}

	return true;
}

 
static int
xfs_buf_cmp(
	void			*priv,
	const struct list_head	*a,
	const struct list_head	*b)
{
	struct xfs_buf	*ap = container_of(a, struct xfs_buf, b_list);
	struct xfs_buf	*bp = container_of(b, struct xfs_buf, b_list);
	xfs_daddr_t		diff;

	diff = ap->b_maps[0].bm_bn - bp->b_maps[0].bm_bn;
	if (diff < 0)
		return -1;
	if (diff > 0)
		return 1;
	return 0;
}

 
static int
xfs_buf_delwri_submit_buffers(
	struct list_head	*buffer_list,
	struct list_head	*wait_list)
{
	struct xfs_buf		*bp, *n;
	int			pinned = 0;
	struct blk_plug		plug;

	list_sort(NULL, buffer_list, xfs_buf_cmp);

	blk_start_plug(&plug);
	list_for_each_entry_safe(bp, n, buffer_list, b_list) {
		if (!wait_list) {
			if (!xfs_buf_trylock(bp))
				continue;
			if (xfs_buf_ispinned(bp)) {
				xfs_buf_unlock(bp);
				pinned++;
				continue;
			}
		} else {
			xfs_buf_lock(bp);
		}

		 
		if (!(bp->b_flags & _XBF_DELWRI_Q)) {
			list_del_init(&bp->b_list);
			xfs_buf_relse(bp);
			continue;
		}

		trace_xfs_buf_delwri_split(bp, _RET_IP_);

		 
		bp->b_flags &= ~_XBF_DELWRI_Q;
		bp->b_flags |= XBF_WRITE;
		if (wait_list) {
			bp->b_flags &= ~XBF_ASYNC;
			list_move_tail(&bp->b_list, wait_list);
		} else {
			bp->b_flags |= XBF_ASYNC;
			list_del_init(&bp->b_list);
		}
		__xfs_buf_submit(bp, false);
	}
	blk_finish_plug(&plug);

	return pinned;
}

 
int
xfs_buf_delwri_submit_nowait(
	struct list_head	*buffer_list)
{
	return xfs_buf_delwri_submit_buffers(buffer_list, NULL);
}

 
int
xfs_buf_delwri_submit(
	struct list_head	*buffer_list)
{
	LIST_HEAD		(wait_list);
	int			error = 0, error2;
	struct xfs_buf		*bp;

	xfs_buf_delwri_submit_buffers(buffer_list, &wait_list);

	 
	while (!list_empty(&wait_list)) {
		bp = list_first_entry(&wait_list, struct xfs_buf, b_list);

		list_del_init(&bp->b_list);

		 
		error2 = xfs_buf_iowait(bp);
		xfs_buf_relse(bp);
		if (!error)
			error = error2;
	}

	return error;
}

 
int
xfs_buf_delwri_pushbuf(
	struct xfs_buf		*bp,
	struct list_head	*buffer_list)
{
	LIST_HEAD		(submit_list);
	int			error;

	ASSERT(bp->b_flags & _XBF_DELWRI_Q);

	trace_xfs_buf_delwri_pushbuf(bp, _RET_IP_);

	 
	xfs_buf_lock(bp);
	list_move(&bp->b_list, &submit_list);
	xfs_buf_unlock(bp);

	 
	xfs_buf_delwri_submit_buffers(&submit_list, buffer_list);

	 
	error = xfs_buf_iowait(bp);
	bp->b_flags |= _XBF_DELWRI_Q;
	xfs_buf_unlock(bp);

	return error;
}

void xfs_buf_set_ref(struct xfs_buf *bp, int lru_ref)
{
	 
	if (XFS_TEST_ERROR(false, bp->b_mount, XFS_ERRTAG_BUF_LRU_REF))
		lru_ref = 0;

	atomic_set(&bp->b_lru_ref, lru_ref);
}

 
bool
xfs_verify_magic(
	struct xfs_buf		*bp,
	__be32			dmagic)
{
	struct xfs_mount	*mp = bp->b_mount;
	int			idx;

	idx = xfs_has_crc(mp);
	if (WARN_ON(!bp->b_ops || !bp->b_ops->magic[idx]))
		return false;
	return dmagic == bp->b_ops->magic[idx];
}
 
bool
xfs_verify_magic16(
	struct xfs_buf		*bp,
	__be16			dmagic)
{
	struct xfs_mount	*mp = bp->b_mount;
	int			idx;

	idx = xfs_has_crc(mp);
	if (WARN_ON(!bp->b_ops || !bp->b_ops->magic16[idx]))
		return false;
	return dmagic == bp->b_ops->magic16[idx];
}
