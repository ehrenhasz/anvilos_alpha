
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_buf_item.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_quota.h"
#include "xfs_dquot_item.h"
#include "xfs_dquot.h"
#include "xfs_trace.h"
#include "xfs_log.h"
#include "xfs_log_priv.h"


struct kmem_cache	*xfs_buf_item_cache;

static inline struct xfs_buf_log_item *BUF_ITEM(struct xfs_log_item *lip)
{
	return container_of(lip, struct xfs_buf_log_item, bli_item);
}

 
bool
xfs_buf_log_check_iovec(
	struct xfs_log_iovec		*iovec)
{
	struct xfs_buf_log_format	*blfp = iovec->i_addr;
	char				*bmp_end;
	char				*item_end;

	if (offsetof(struct xfs_buf_log_format, blf_data_map) > iovec->i_len)
		return false;

	item_end = (char *)iovec->i_addr + iovec->i_len;
	bmp_end = (char *)&blfp->blf_data_map[blfp->blf_map_size];
	return bmp_end <= item_end;
}

static inline int
xfs_buf_log_format_size(
	struct xfs_buf_log_format *blfp)
{
	return offsetof(struct xfs_buf_log_format, blf_data_map) +
			(blfp->blf_map_size * sizeof(blfp->blf_data_map[0]));
}

static inline bool
xfs_buf_item_straddle(
	struct xfs_buf		*bp,
	uint			offset,
	int			first_bit,
	int			nbits)
{
	void			*first, *last;

	first = xfs_buf_offset(bp, offset + (first_bit << XFS_BLF_SHIFT));
	last = xfs_buf_offset(bp,
			offset + ((first_bit + nbits) << XFS_BLF_SHIFT));

	if (last - first != nbits * XFS_BLF_CHUNK)
		return true;
	return false;
}

 
STATIC void
xfs_buf_item_size_segment(
	struct xfs_buf_log_item		*bip,
	struct xfs_buf_log_format	*blfp,
	uint				offset,
	int				*nvecs,
	int				*nbytes)
{
	struct xfs_buf			*bp = bip->bli_buf;
	int				first_bit;
	int				nbits;
	int				next_bit;
	int				last_bit;

	first_bit = xfs_next_bit(blfp->blf_data_map, blfp->blf_map_size, 0);
	if (first_bit == -1)
		return;

	(*nvecs)++;
	*nbytes += xfs_buf_log_format_size(blfp);

	do {
		nbits = xfs_contig_bits(blfp->blf_data_map,
					blfp->blf_map_size, first_bit);
		ASSERT(nbits > 0);

		 
		if (nbits > 1 &&
		    xfs_buf_item_straddle(bp, offset, first_bit, nbits))
			goto slow_scan;

		(*nvecs)++;
		*nbytes += nbits * XFS_BLF_CHUNK;

		 
		first_bit = xfs_next_bit(blfp->blf_data_map, blfp->blf_map_size,
					(uint)first_bit + nbits + 1);
	} while (first_bit != -1);

	return;

slow_scan:
	 
	(*nvecs)++;
	*nbytes += XFS_BLF_CHUNK;
	last_bit = first_bit;
	while (last_bit != -1) {
		 
		next_bit = xfs_next_bit(blfp->blf_data_map, blfp->blf_map_size,
					last_bit + 1);
		 
		if (next_bit == -1) {
			break;
		} else if (next_bit != last_bit + 1 ||
		           xfs_buf_item_straddle(bp, offset, first_bit, nbits)) {
			last_bit = next_bit;
			first_bit = next_bit;
			(*nvecs)++;
			nbits = 1;
		} else {
			last_bit++;
			nbits++;
		}
		*nbytes += XFS_BLF_CHUNK;
	}
}

 
STATIC void
xfs_buf_item_size(
	struct xfs_log_item	*lip,
	int			*nvecs,
	int			*nbytes)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;
	int			i;
	int			bytes;
	uint			offset = 0;

	ASSERT(atomic_read(&bip->bli_refcount) > 0);
	if (bip->bli_flags & XFS_BLI_STALE) {
		 
		trace_xfs_buf_item_size_stale(bip);
		ASSERT(bip->__bli_format.blf_flags & XFS_BLF_CANCEL);
		*nvecs += bip->bli_format_count;
		for (i = 0; i < bip->bli_format_count; i++) {
			*nbytes += xfs_buf_log_format_size(&bip->bli_formats[i]);
		}
		return;
	}

	ASSERT(bip->bli_flags & XFS_BLI_LOGGED);

	if (bip->bli_flags & XFS_BLI_ORDERED) {
		 
		trace_xfs_buf_item_size_ordered(bip);
		*nvecs = XFS_LOG_VEC_ORDERED;
		return;
	}

	 
	bytes = 0;
	for (i = 0; i < bip->bli_format_count; i++) {
		xfs_buf_item_size_segment(bip, &bip->bli_formats[i], offset,
					  nvecs, &bytes);
		offset += BBTOB(bp->b_maps[i].bm_len);
	}

	 
	*nbytes = round_up(bytes, 512);
	trace_xfs_buf_item_size(bip);
}

static inline void
xfs_buf_item_copy_iovec(
	struct xfs_log_vec	*lv,
	struct xfs_log_iovec	**vecp,
	struct xfs_buf		*bp,
	uint			offset,
	int			first_bit,
	uint			nbits)
{
	offset += first_bit * XFS_BLF_CHUNK;
	xlog_copy_iovec(lv, vecp, XLOG_REG_TYPE_BCHUNK,
			xfs_buf_offset(bp, offset),
			nbits * XFS_BLF_CHUNK);
}

static void
xfs_buf_item_format_segment(
	struct xfs_buf_log_item	*bip,
	struct xfs_log_vec	*lv,
	struct xfs_log_iovec	**vecp,
	uint			offset,
	struct xfs_buf_log_format *blfp)
{
	struct xfs_buf		*bp = bip->bli_buf;
	uint			base_size;
	int			first_bit;
	int			last_bit;
	int			next_bit;
	uint			nbits;

	 
	blfp->blf_flags = bip->__bli_format.blf_flags;

	 
	base_size = xfs_buf_log_format_size(blfp);

	first_bit = xfs_next_bit(blfp->blf_data_map, blfp->blf_map_size, 0);
	if (!(bip->bli_flags & XFS_BLI_STALE) && first_bit == -1) {
		 
		return;
	}

	blfp = xlog_copy_iovec(lv, vecp, XLOG_REG_TYPE_BFORMAT, blfp, base_size);
	blfp->blf_size = 1;

	if (bip->bli_flags & XFS_BLI_STALE) {
		 
		trace_xfs_buf_item_format_stale(bip);
		ASSERT(blfp->blf_flags & XFS_BLF_CANCEL);
		return;
	}


	 
	do {
		ASSERT(first_bit >= 0);
		nbits = xfs_contig_bits(blfp->blf_data_map,
					blfp->blf_map_size, first_bit);
		ASSERT(nbits > 0);

		 
		if (nbits > 1 &&
		    xfs_buf_item_straddle(bp, offset, first_bit, nbits))
			goto slow_scan;

		xfs_buf_item_copy_iovec(lv, vecp, bp, offset,
					first_bit, nbits);
		blfp->blf_size++;

		 
		first_bit = xfs_next_bit(blfp->blf_data_map, blfp->blf_map_size,
					(uint)first_bit + nbits + 1);
	} while (first_bit != -1);

	return;

slow_scan:
	ASSERT(bp->b_addr == NULL);
	last_bit = first_bit;
	nbits = 1;
	for (;;) {
		 
		next_bit = xfs_next_bit(blfp->blf_data_map, blfp->blf_map_size,
					(uint)last_bit + 1);
		 
		if (next_bit == -1) {
			xfs_buf_item_copy_iovec(lv, vecp, bp, offset,
						first_bit, nbits);
			blfp->blf_size++;
			break;
		} else if (next_bit != last_bit + 1 ||
		           xfs_buf_item_straddle(bp, offset, first_bit, nbits)) {
			xfs_buf_item_copy_iovec(lv, vecp, bp, offset,
						first_bit, nbits);
			blfp->blf_size++;
			first_bit = next_bit;
			last_bit = next_bit;
			nbits = 1;
		} else {
			last_bit++;
			nbits++;
		}
	}
}

 
STATIC void
xfs_buf_item_format(
	struct xfs_log_item	*lip,
	struct xfs_log_vec	*lv)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;
	struct xfs_log_iovec	*vecp = NULL;
	uint			offset = 0;
	int			i;

	ASSERT(atomic_read(&bip->bli_refcount) > 0);
	ASSERT((bip->bli_flags & XFS_BLI_LOGGED) ||
	       (bip->bli_flags & XFS_BLI_STALE));
	ASSERT((bip->bli_flags & XFS_BLI_STALE) ||
	       (xfs_blft_from_flags(&bip->__bli_format) > XFS_BLFT_UNKNOWN_BUF
	        && xfs_blft_from_flags(&bip->__bli_format) < XFS_BLFT_MAX_BUF));
	ASSERT(!(bip->bli_flags & XFS_BLI_ORDERED) ||
	       (bip->bli_flags & XFS_BLI_STALE));


	 
	if (bip->bli_flags & XFS_BLI_INODE_BUF) {
		if (xfs_has_v3inodes(lip->li_log->l_mp) ||
		    !((bip->bli_flags & XFS_BLI_INODE_ALLOC_BUF) &&
		      xfs_log_item_in_current_chkpt(lip)))
			bip->__bli_format.blf_flags |= XFS_BLF_INODE_BUF;
		bip->bli_flags &= ~XFS_BLI_INODE_BUF;
	}

	for (i = 0; i < bip->bli_format_count; i++) {
		xfs_buf_item_format_segment(bip, lv, &vecp, offset,
					    &bip->bli_formats[i]);
		offset += BBTOB(bp->b_maps[i].bm_len);
	}

	 
	trace_xfs_buf_item_format(bip);
}

 
STATIC void
xfs_buf_item_pin(
	struct xfs_log_item	*lip)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);

	ASSERT(atomic_read(&bip->bli_refcount) > 0);
	ASSERT((bip->bli_flags & XFS_BLI_LOGGED) ||
	       (bip->bli_flags & XFS_BLI_ORDERED) ||
	       (bip->bli_flags & XFS_BLI_STALE));

	trace_xfs_buf_item_pin(bip);

	xfs_buf_hold(bip->bli_buf);
	atomic_inc(&bip->bli_refcount);
	atomic_inc(&bip->bli_buf->b_pin_count);
}

 
STATIC void
xfs_buf_item_unpin(
	struct xfs_log_item	*lip,
	int			remove)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;
	int			stale = bip->bli_flags & XFS_BLI_STALE;
	int			freed;

	ASSERT(bp->b_log_item == bip);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	trace_xfs_buf_item_unpin(bip);

	freed = atomic_dec_and_test(&bip->bli_refcount);
	if (atomic_dec_and_test(&bp->b_pin_count))
		wake_up_all(&bp->b_waiters);

	 
	if (!freed) {
		xfs_buf_rele(bp);
		return;
	}

	if (stale) {
		ASSERT(bip->bli_flags & XFS_BLI_STALE);
		ASSERT(xfs_buf_islocked(bp));
		ASSERT(bp->b_flags & XBF_STALE);
		ASSERT(bip->__bli_format.blf_flags & XFS_BLF_CANCEL);
		ASSERT(list_empty(&lip->li_trans));
		ASSERT(!bp->b_transp);

		trace_xfs_buf_item_unpin_stale(bip);

		 
		xfs_buf_rele(bp);

		 
		if (bip->bli_flags & XFS_BLI_STALE_INODE) {
			xfs_buf_item_done(bp);
			xfs_buf_inode_iodone(bp);
			ASSERT(list_empty(&bp->b_li_list));
		} else {
			xfs_trans_ail_delete(lip, SHUTDOWN_LOG_IO_ERROR);
			xfs_buf_item_relse(bp);
			ASSERT(bp->b_log_item == NULL);
		}
		xfs_buf_relse(bp);
		return;
	}

	if (remove) {
		 
		xfs_buf_lock(bp);
		bp->b_flags |= XBF_ASYNC;
		xfs_buf_ioend_fail(bp);
		return;
	}

	 
	xfs_buf_rele(bp);
}

STATIC uint
xfs_buf_item_push(
	struct xfs_log_item	*lip,
	struct list_head	*buffer_list)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;
	uint			rval = XFS_ITEM_SUCCESS;

	if (xfs_buf_ispinned(bp))
		return XFS_ITEM_PINNED;
	if (!xfs_buf_trylock(bp)) {
		 
		if (xfs_buf_ispinned(bp))
			return XFS_ITEM_PINNED;
		return XFS_ITEM_LOCKED;
	}

	ASSERT(!(bip->bli_flags & XFS_BLI_STALE));

	trace_xfs_buf_item_push(bip);

	 
	if (bp->b_flags & XBF_WRITE_FAIL) {
		xfs_buf_alert_ratelimited(bp, "XFS: Failing async write",
	    "Failing async write on buffer block 0x%llx. Retrying async write.",
					  (long long)xfs_buf_daddr(bp));
	}

	if (!xfs_buf_delwri_queue(bp, buffer_list))
		rval = XFS_ITEM_FLUSHING;
	xfs_buf_unlock(bp);
	return rval;
}

 
bool
xfs_buf_item_put(
	struct xfs_buf_log_item	*bip)
{
	struct xfs_log_item	*lip = &bip->bli_item;
	bool			aborted;
	bool			dirty;

	 
	if (!atomic_dec_and_test(&bip->bli_refcount))
		return false;

	 
	aborted = test_bit(XFS_LI_ABORTED, &lip->li_flags) ||
			xlog_is_shutdown(lip->li_log);
	dirty = bip->bli_flags & XFS_BLI_DIRTY;
	if (dirty && !aborted)
		return false;

	 
	if (aborted)
		xfs_trans_ail_delete(lip, 0);
	xfs_buf_item_relse(bip->bli_buf);
	return true;
}

 
STATIC void
xfs_buf_item_release(
	struct xfs_log_item	*lip)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;
	bool			released;
	bool			hold = bip->bli_flags & XFS_BLI_HOLD;
	bool			stale = bip->bli_flags & XFS_BLI_STALE;
#if defined(DEBUG) || defined(XFS_WARN)
	bool			ordered = bip->bli_flags & XFS_BLI_ORDERED;
	bool			dirty = bip->bli_flags & XFS_BLI_DIRTY;
	bool			aborted = test_bit(XFS_LI_ABORTED,
						   &lip->li_flags);
#endif

	trace_xfs_buf_item_release(bip);

	 
	ASSERT((!ordered && dirty == xfs_buf_item_dirty_format(bip)) ||
	       (ordered && dirty && !xfs_buf_item_dirty_format(bip)));
	ASSERT(!stale || (bip->__bli_format.blf_flags & XFS_BLF_CANCEL));

	 
	bp->b_transp = NULL;
	bip->bli_flags &= ~(XFS_BLI_LOGGED | XFS_BLI_HOLD | XFS_BLI_ORDERED);

	 
	released = xfs_buf_item_put(bip);
	if (hold || (stale && !released))
		return;
	ASSERT(!stale || aborted);
	xfs_buf_relse(bp);
}

STATIC void
xfs_buf_item_committing(
	struct xfs_log_item	*lip,
	xfs_csn_t		seq)
{
	return xfs_buf_item_release(lip);
}

 
STATIC xfs_lsn_t
xfs_buf_item_committed(
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);

	trace_xfs_buf_item_committed(bip);

	if ((bip->bli_flags & XFS_BLI_INODE_ALLOC_BUF) && lip->li_lsn != 0)
		return lip->li_lsn;
	return lsn;
}

static const struct xfs_item_ops xfs_buf_item_ops = {
	.iop_size	= xfs_buf_item_size,
	.iop_format	= xfs_buf_item_format,
	.iop_pin	= xfs_buf_item_pin,
	.iop_unpin	= xfs_buf_item_unpin,
	.iop_release	= xfs_buf_item_release,
	.iop_committing	= xfs_buf_item_committing,
	.iop_committed	= xfs_buf_item_committed,
	.iop_push	= xfs_buf_item_push,
};

STATIC void
xfs_buf_item_get_format(
	struct xfs_buf_log_item	*bip,
	int			count)
{
	ASSERT(bip->bli_formats == NULL);
	bip->bli_format_count = count;

	if (count == 1) {
		bip->bli_formats = &bip->__bli_format;
		return;
	}

	bip->bli_formats = kmem_zalloc(count * sizeof(struct xfs_buf_log_format),
				0);
}

STATIC void
xfs_buf_item_free_format(
	struct xfs_buf_log_item	*bip)
{
	if (bip->bli_formats != &bip->__bli_format) {
		kmem_free(bip->bli_formats);
		bip->bli_formats = NULL;
	}
}

 
int
xfs_buf_item_init(
	struct xfs_buf	*bp,
	struct xfs_mount *mp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;
	int			chunks;
	int			map_size;
	int			i;

	 
	ASSERT(bp->b_mount == mp);
	if (bip) {
		ASSERT(bip->bli_item.li_type == XFS_LI_BUF);
		ASSERT(!bp->b_transp);
		ASSERT(bip->bli_buf == bp);
		return 0;
	}

	bip = kmem_cache_zalloc(xfs_buf_item_cache, GFP_KERNEL | __GFP_NOFAIL);
	xfs_log_item_init(mp, &bip->bli_item, XFS_LI_BUF, &xfs_buf_item_ops);
	bip->bli_buf = bp;

	 
	xfs_buf_item_get_format(bip, bp->b_map_count);

	for (i = 0; i < bip->bli_format_count; i++) {
		chunks = DIV_ROUND_UP(BBTOB(bp->b_maps[i].bm_len),
				      XFS_BLF_CHUNK);
		map_size = DIV_ROUND_UP(chunks, NBWORD);

		if (map_size > XFS_BLF_DATAMAP_SIZE) {
			kmem_cache_free(xfs_buf_item_cache, bip);
			xfs_err(mp,
	"buffer item dirty bitmap (%u uints) too small to reflect %u bytes!",
					map_size,
					BBTOB(bp->b_maps[i].bm_len));
			return -EFSCORRUPTED;
		}

		bip->bli_formats[i].blf_type = XFS_LI_BUF;
		bip->bli_formats[i].blf_blkno = bp->b_maps[i].bm_bn;
		bip->bli_formats[i].blf_len = bp->b_maps[i].bm_len;
		bip->bli_formats[i].blf_map_size = map_size;
	}

	bp->b_log_item = bip;
	xfs_buf_hold(bp);
	return 0;
}


 
static void
xfs_buf_item_log_segment(
	uint			first,
	uint			last,
	uint			*map)
{
	uint		first_bit;
	uint		last_bit;
	uint		bits_to_set;
	uint		bits_set;
	uint		word_num;
	uint		*wordp;
	uint		bit;
	uint		end_bit;
	uint		mask;

	ASSERT(first < XFS_BLF_DATAMAP_SIZE * XFS_BLF_CHUNK * NBWORD);
	ASSERT(last < XFS_BLF_DATAMAP_SIZE * XFS_BLF_CHUNK * NBWORD);

	 
	first_bit = first >> XFS_BLF_SHIFT;
	last_bit = last >> XFS_BLF_SHIFT;

	 
	bits_to_set = last_bit - first_bit + 1;

	 
	word_num = first_bit >> BIT_TO_WORD_SHIFT;
	wordp = &map[word_num];

	 
	bit = first_bit & (uint)(NBWORD - 1);

	 
	if (bit) {
		end_bit = min(bit + bits_to_set, (uint)NBWORD);
		mask = ((1U << (end_bit - bit)) - 1) << bit;
		*wordp |= mask;
		wordp++;
		bits_set = end_bit - bit;
	} else {
		bits_set = 0;
	}

	 
	while ((bits_to_set - bits_set) >= NBWORD) {
		*wordp = 0xffffffff;
		bits_set += NBWORD;
		wordp++;
	}

	 
	end_bit = bits_to_set - bits_set;
	if (end_bit) {
		mask = (1U << end_bit) - 1;
		*wordp |= mask;
	}
}

 
void
xfs_buf_item_log(
	struct xfs_buf_log_item	*bip,
	uint			first,
	uint			last)
{
	int			i;
	uint			start;
	uint			end;
	struct xfs_buf		*bp = bip->bli_buf;

	 
	start = 0;
	for (i = 0; i < bip->bli_format_count; i++) {
		if (start > last)
			break;
		end = start + BBTOB(bp->b_maps[i].bm_len) - 1;

		 
		if (first > end) {
			start += BBTOB(bp->b_maps[i].bm_len);
			continue;
		}

		 
		if (first < start)
			first = start;
		if (end > last)
			end = last;
		xfs_buf_item_log_segment(first - start, end - start,
					 &bip->bli_formats[i].blf_data_map[0]);

		start += BBTOB(bp->b_maps[i].bm_len);
	}
}


 
bool
xfs_buf_item_dirty_format(
	struct xfs_buf_log_item	*bip)
{
	int			i;

	for (i = 0; i < bip->bli_format_count; i++) {
		if (!xfs_bitmap_empty(bip->bli_formats[i].blf_data_map,
			     bip->bli_formats[i].blf_map_size))
			return true;
	}

	return false;
}

STATIC void
xfs_buf_item_free(
	struct xfs_buf_log_item	*bip)
{
	xfs_buf_item_free_format(bip);
	kmem_free(bip->bli_item.li_lv_shadow);
	kmem_cache_free(xfs_buf_item_cache, bip);
}

 
void
xfs_buf_item_relse(
	struct xfs_buf	*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	trace_xfs_buf_item_relse(bp, _RET_IP_);
	ASSERT(!test_bit(XFS_LI_IN_AIL, &bip->bli_item.li_flags));

	if (atomic_read(&bip->bli_refcount))
		return;
	bp->b_log_item = NULL;
	xfs_buf_rele(bp);
	xfs_buf_item_free(bip);
}

void
xfs_buf_item_done(
	struct xfs_buf		*bp)
{
	 
	xfs_trans_ail_delete(&bp->b_log_item->bli_item,
			     (bp->b_flags & _XBF_LOGRECOVERY) ? 0 :
			     SHUTDOWN_CORRUPT_INCORE);
	xfs_buf_item_relse(bp);
}
