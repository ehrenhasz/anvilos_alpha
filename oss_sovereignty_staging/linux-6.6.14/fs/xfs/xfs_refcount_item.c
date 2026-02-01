
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_shared.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_refcount_item.h"
#include "xfs_log.h"
#include "xfs_refcount.h"
#include "xfs_error.h"
#include "xfs_log_priv.h"
#include "xfs_log_recover.h"
#include "xfs_ag.h"

struct kmem_cache	*xfs_cui_cache;
struct kmem_cache	*xfs_cud_cache;

static const struct xfs_item_ops xfs_cui_item_ops;

static inline struct xfs_cui_log_item *CUI_ITEM(struct xfs_log_item *lip)
{
	return container_of(lip, struct xfs_cui_log_item, cui_item);
}

STATIC void
xfs_cui_item_free(
	struct xfs_cui_log_item	*cuip)
{
	kmem_free(cuip->cui_item.li_lv_shadow);
	if (cuip->cui_format.cui_nextents > XFS_CUI_MAX_FAST_EXTENTS)
		kmem_free(cuip);
	else
		kmem_cache_free(xfs_cui_cache, cuip);
}

 
STATIC void
xfs_cui_release(
	struct xfs_cui_log_item	*cuip)
{
	ASSERT(atomic_read(&cuip->cui_refcount) > 0);
	if (!atomic_dec_and_test(&cuip->cui_refcount))
		return;

	xfs_trans_ail_delete(&cuip->cui_item, 0);
	xfs_cui_item_free(cuip);
}


STATIC void
xfs_cui_item_size(
	struct xfs_log_item	*lip,
	int			*nvecs,
	int			*nbytes)
{
	struct xfs_cui_log_item	*cuip = CUI_ITEM(lip);

	*nvecs += 1;
	*nbytes += xfs_cui_log_format_sizeof(cuip->cui_format.cui_nextents);
}

 
STATIC void
xfs_cui_item_format(
	struct xfs_log_item	*lip,
	struct xfs_log_vec	*lv)
{
	struct xfs_cui_log_item	*cuip = CUI_ITEM(lip);
	struct xfs_log_iovec	*vecp = NULL;

	ASSERT(atomic_read(&cuip->cui_next_extent) ==
			cuip->cui_format.cui_nextents);

	cuip->cui_format.cui_type = XFS_LI_CUI;
	cuip->cui_format.cui_size = 1;

	xlog_copy_iovec(lv, &vecp, XLOG_REG_TYPE_CUI_FORMAT, &cuip->cui_format,
			xfs_cui_log_format_sizeof(cuip->cui_format.cui_nextents));
}

 
STATIC void
xfs_cui_item_unpin(
	struct xfs_log_item	*lip,
	int			remove)
{
	struct xfs_cui_log_item	*cuip = CUI_ITEM(lip);

	xfs_cui_release(cuip);
}

 
STATIC void
xfs_cui_item_release(
	struct xfs_log_item	*lip)
{
	xfs_cui_release(CUI_ITEM(lip));
}

 
STATIC struct xfs_cui_log_item *
xfs_cui_init(
	struct xfs_mount		*mp,
	uint				nextents)

{
	struct xfs_cui_log_item		*cuip;

	ASSERT(nextents > 0);
	if (nextents > XFS_CUI_MAX_FAST_EXTENTS)
		cuip = kmem_zalloc(xfs_cui_log_item_sizeof(nextents),
				0);
	else
		cuip = kmem_cache_zalloc(xfs_cui_cache,
					 GFP_KERNEL | __GFP_NOFAIL);

	xfs_log_item_init(mp, &cuip->cui_item, XFS_LI_CUI, &xfs_cui_item_ops);
	cuip->cui_format.cui_nextents = nextents;
	cuip->cui_format.cui_id = (uintptr_t)(void *)cuip;
	atomic_set(&cuip->cui_next_extent, 0);
	atomic_set(&cuip->cui_refcount, 2);

	return cuip;
}

static inline struct xfs_cud_log_item *CUD_ITEM(struct xfs_log_item *lip)
{
	return container_of(lip, struct xfs_cud_log_item, cud_item);
}

STATIC void
xfs_cud_item_size(
	struct xfs_log_item	*lip,
	int			*nvecs,
	int			*nbytes)
{
	*nvecs += 1;
	*nbytes += sizeof(struct xfs_cud_log_format);
}

 
STATIC void
xfs_cud_item_format(
	struct xfs_log_item	*lip,
	struct xfs_log_vec	*lv)
{
	struct xfs_cud_log_item	*cudp = CUD_ITEM(lip);
	struct xfs_log_iovec	*vecp = NULL;

	cudp->cud_format.cud_type = XFS_LI_CUD;
	cudp->cud_format.cud_size = 1;

	xlog_copy_iovec(lv, &vecp, XLOG_REG_TYPE_CUD_FORMAT, &cudp->cud_format,
			sizeof(struct xfs_cud_log_format));
}

 
STATIC void
xfs_cud_item_release(
	struct xfs_log_item	*lip)
{
	struct xfs_cud_log_item	*cudp = CUD_ITEM(lip);

	xfs_cui_release(cudp->cud_cuip);
	kmem_free(cudp->cud_item.li_lv_shadow);
	kmem_cache_free(xfs_cud_cache, cudp);
}

static struct xfs_log_item *
xfs_cud_item_intent(
	struct xfs_log_item	*lip)
{
	return &CUD_ITEM(lip)->cud_cuip->cui_item;
}

static const struct xfs_item_ops xfs_cud_item_ops = {
	.flags		= XFS_ITEM_RELEASE_WHEN_COMMITTED |
			  XFS_ITEM_INTENT_DONE,
	.iop_size	= xfs_cud_item_size,
	.iop_format	= xfs_cud_item_format,
	.iop_release	= xfs_cud_item_release,
	.iop_intent	= xfs_cud_item_intent,
};

static struct xfs_cud_log_item *
xfs_trans_get_cud(
	struct xfs_trans		*tp,
	struct xfs_cui_log_item		*cuip)
{
	struct xfs_cud_log_item		*cudp;

	cudp = kmem_cache_zalloc(xfs_cud_cache, GFP_KERNEL | __GFP_NOFAIL);
	xfs_log_item_init(tp->t_mountp, &cudp->cud_item, XFS_LI_CUD,
			  &xfs_cud_item_ops);
	cudp->cud_cuip = cuip;
	cudp->cud_format.cud_cui_id = cuip->cui_format.cui_id;

	xfs_trans_add_item(tp, &cudp->cud_item);
	return cudp;
}

 
static int
xfs_trans_log_finish_refcount_update(
	struct xfs_trans		*tp,
	struct xfs_cud_log_item		*cudp,
	struct xfs_refcount_intent	*ri,
	struct xfs_btree_cur		**pcur)
{
	int				error;

	error = xfs_refcount_finish_one(tp, ri, pcur);

	 
	tp->t_flags |= XFS_TRANS_DIRTY | XFS_TRANS_HAS_INTENT_DONE;
	set_bit(XFS_LI_DIRTY, &cudp->cud_item.li_flags);

	return error;
}

 
static int
xfs_refcount_update_diff_items(
	void				*priv,
	const struct list_head		*a,
	const struct list_head		*b)
{
	struct xfs_refcount_intent	*ra;
	struct xfs_refcount_intent	*rb;

	ra = container_of(a, struct xfs_refcount_intent, ri_list);
	rb = container_of(b, struct xfs_refcount_intent, ri_list);

	return ra->ri_pag->pag_agno - rb->ri_pag->pag_agno;
}

 
static void
xfs_trans_set_refcount_flags(
	struct xfs_phys_extent		*pmap,
	enum xfs_refcount_intent_type	type)
{
	pmap->pe_flags = 0;
	switch (type) {
	case XFS_REFCOUNT_INCREASE:
	case XFS_REFCOUNT_DECREASE:
	case XFS_REFCOUNT_ALLOC_COW:
	case XFS_REFCOUNT_FREE_COW:
		pmap->pe_flags |= type;
		break;
	default:
		ASSERT(0);
	}
}

 
STATIC void
xfs_refcount_update_log_item(
	struct xfs_trans		*tp,
	struct xfs_cui_log_item		*cuip,
	struct xfs_refcount_intent	*ri)
{
	uint				next_extent;
	struct xfs_phys_extent		*pmap;

	tp->t_flags |= XFS_TRANS_DIRTY;
	set_bit(XFS_LI_DIRTY, &cuip->cui_item.li_flags);

	 
	next_extent = atomic_inc_return(&cuip->cui_next_extent) - 1;
	ASSERT(next_extent < cuip->cui_format.cui_nextents);
	pmap = &cuip->cui_format.cui_extents[next_extent];
	pmap->pe_startblock = ri->ri_startblock;
	pmap->pe_len = ri->ri_blockcount;
	xfs_trans_set_refcount_flags(pmap, ri->ri_type);
}

static struct xfs_log_item *
xfs_refcount_update_create_intent(
	struct xfs_trans		*tp,
	struct list_head		*items,
	unsigned int			count,
	bool				sort)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_cui_log_item		*cuip = xfs_cui_init(mp, count);
	struct xfs_refcount_intent	*ri;

	ASSERT(count > 0);

	xfs_trans_add_item(tp, &cuip->cui_item);
	if (sort)
		list_sort(mp, items, xfs_refcount_update_diff_items);
	list_for_each_entry(ri, items, ri_list)
		xfs_refcount_update_log_item(tp, cuip, ri);
	return &cuip->cui_item;
}

 
static struct xfs_log_item *
xfs_refcount_update_create_done(
	struct xfs_trans		*tp,
	struct xfs_log_item		*intent,
	unsigned int			count)
{
	return &xfs_trans_get_cud(tp, CUI_ITEM(intent))->cud_item;
}

 
void
xfs_refcount_update_get_group(
	struct xfs_mount		*mp,
	struct xfs_refcount_intent	*ri)
{
	xfs_agnumber_t			agno;

	agno = XFS_FSB_TO_AGNO(mp, ri->ri_startblock);
	ri->ri_pag = xfs_perag_intent_get(mp, agno);
}

 
static inline void
xfs_refcount_update_put_group(
	struct xfs_refcount_intent	*ri)
{
	xfs_perag_intent_put(ri->ri_pag);
}

 
STATIC int
xfs_refcount_update_finish_item(
	struct xfs_trans		*tp,
	struct xfs_log_item		*done,
	struct list_head		*item,
	struct xfs_btree_cur		**state)
{
	struct xfs_refcount_intent	*ri;
	int				error;

	ri = container_of(item, struct xfs_refcount_intent, ri_list);
	error = xfs_trans_log_finish_refcount_update(tp, CUD_ITEM(done), ri,
			state);

	 
	if (!error && ri->ri_blockcount > 0) {
		ASSERT(ri->ri_type == XFS_REFCOUNT_INCREASE ||
		       ri->ri_type == XFS_REFCOUNT_DECREASE);
		return -EAGAIN;
	}

	xfs_refcount_update_put_group(ri);
	kmem_cache_free(xfs_refcount_intent_cache, ri);
	return error;
}

 
STATIC void
xfs_refcount_update_abort_intent(
	struct xfs_log_item		*intent)
{
	xfs_cui_release(CUI_ITEM(intent));
}

 
STATIC void
xfs_refcount_update_cancel_item(
	struct list_head		*item)
{
	struct xfs_refcount_intent	*ri;

	ri = container_of(item, struct xfs_refcount_intent, ri_list);

	xfs_refcount_update_put_group(ri);
	kmem_cache_free(xfs_refcount_intent_cache, ri);
}

const struct xfs_defer_op_type xfs_refcount_update_defer_type = {
	.max_items	= XFS_CUI_MAX_FAST_EXTENTS,
	.create_intent	= xfs_refcount_update_create_intent,
	.abort_intent	= xfs_refcount_update_abort_intent,
	.create_done	= xfs_refcount_update_create_done,
	.finish_item	= xfs_refcount_update_finish_item,
	.finish_cleanup = xfs_refcount_finish_one_cleanup,
	.cancel_item	= xfs_refcount_update_cancel_item,
};

 
static inline bool
xfs_cui_validate_phys(
	struct xfs_mount		*mp,
	struct xfs_phys_extent		*pmap)
{
	if (!xfs_has_reflink(mp))
		return false;

	if (pmap->pe_flags & ~XFS_REFCOUNT_EXTENT_FLAGS)
		return false;

	switch (pmap->pe_flags & XFS_REFCOUNT_EXTENT_TYPE_MASK) {
	case XFS_REFCOUNT_INCREASE:
	case XFS_REFCOUNT_DECREASE:
	case XFS_REFCOUNT_ALLOC_COW:
	case XFS_REFCOUNT_FREE_COW:
		break;
	default:
		return false;
	}

	return xfs_verify_fsbext(mp, pmap->pe_startblock, pmap->pe_len);
}

 
STATIC int
xfs_cui_item_recover(
	struct xfs_log_item		*lip,
	struct list_head		*capture_list)
{
	struct xfs_trans_res		resv;
	struct xfs_cui_log_item		*cuip = CUI_ITEM(lip);
	struct xfs_cud_log_item		*cudp;
	struct xfs_trans		*tp;
	struct xfs_btree_cur		*rcur = NULL;
	struct xfs_mount		*mp = lip->li_log->l_mp;
	unsigned int			refc_type;
	bool				requeue_only = false;
	int				i;
	int				error = 0;

	 
	for (i = 0; i < cuip->cui_format.cui_nextents; i++) {
		if (!xfs_cui_validate_phys(mp,
					&cuip->cui_format.cui_extents[i])) {
			XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp,
					&cuip->cui_format,
					sizeof(cuip->cui_format));
			return -EFSCORRUPTED;
		}
	}

	 
	resv = xlog_recover_resv(&M_RES(mp)->tr_itruncate);
	error = xfs_trans_alloc(mp, &resv, mp->m_refc_maxlevels * 2, 0,
			XFS_TRANS_RESERVE, &tp);
	if (error)
		return error;

	cudp = xfs_trans_get_cud(tp, cuip);

	for (i = 0; i < cuip->cui_format.cui_nextents; i++) {
		struct xfs_refcount_intent	fake = { };
		struct xfs_phys_extent		*pmap;

		pmap = &cuip->cui_format.cui_extents[i];
		refc_type = pmap->pe_flags & XFS_REFCOUNT_EXTENT_TYPE_MASK;
		switch (refc_type) {
		case XFS_REFCOUNT_INCREASE:
		case XFS_REFCOUNT_DECREASE:
		case XFS_REFCOUNT_ALLOC_COW:
		case XFS_REFCOUNT_FREE_COW:
			fake.ri_type = refc_type;
			break;
		default:
			XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp,
					&cuip->cui_format,
					sizeof(cuip->cui_format));
			error = -EFSCORRUPTED;
			goto abort_error;
		}

		fake.ri_startblock = pmap->pe_startblock;
		fake.ri_blockcount = pmap->pe_len;

		if (!requeue_only) {
			xfs_refcount_update_get_group(mp, &fake);
			error = xfs_trans_log_finish_refcount_update(tp, cudp,
					&fake, &rcur);
			xfs_refcount_update_put_group(&fake);
		}
		if (error == -EFSCORRUPTED)
			XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp,
					&cuip->cui_format,
					sizeof(cuip->cui_format));
		if (error)
			goto abort_error;

		 
		if (fake.ri_blockcount > 0) {
			struct xfs_bmbt_irec	irec = {
				.br_startblock	= fake.ri_startblock,
				.br_blockcount	= fake.ri_blockcount,
			};

			switch (fake.ri_type) {
			case XFS_REFCOUNT_INCREASE:
				xfs_refcount_increase_extent(tp, &irec);
				break;
			case XFS_REFCOUNT_DECREASE:
				xfs_refcount_decrease_extent(tp, &irec);
				break;
			case XFS_REFCOUNT_ALLOC_COW:
				xfs_refcount_alloc_cow_extent(tp,
						irec.br_startblock,
						irec.br_blockcount);
				break;
			case XFS_REFCOUNT_FREE_COW:
				xfs_refcount_free_cow_extent(tp,
						irec.br_startblock,
						irec.br_blockcount);
				break;
			default:
				ASSERT(0);
			}
			requeue_only = true;
		}
	}

	xfs_refcount_finish_one_cleanup(tp, rcur, error);
	return xfs_defer_ops_capture_and_commit(tp, capture_list);

abort_error:
	xfs_refcount_finish_one_cleanup(tp, rcur, error);
	xfs_trans_cancel(tp);
	return error;
}

STATIC bool
xfs_cui_item_match(
	struct xfs_log_item	*lip,
	uint64_t		intent_id)
{
	return CUI_ITEM(lip)->cui_format.cui_id == intent_id;
}

 
static struct xfs_log_item *
xfs_cui_item_relog(
	struct xfs_log_item		*intent,
	struct xfs_trans		*tp)
{
	struct xfs_cud_log_item		*cudp;
	struct xfs_cui_log_item		*cuip;
	struct xfs_phys_extent		*pmap;
	unsigned int			count;

	count = CUI_ITEM(intent)->cui_format.cui_nextents;
	pmap = CUI_ITEM(intent)->cui_format.cui_extents;

	tp->t_flags |= XFS_TRANS_DIRTY;
	cudp = xfs_trans_get_cud(tp, CUI_ITEM(intent));
	set_bit(XFS_LI_DIRTY, &cudp->cud_item.li_flags);

	cuip = xfs_cui_init(tp->t_mountp, count);
	memcpy(cuip->cui_format.cui_extents, pmap, count * sizeof(*pmap));
	atomic_set(&cuip->cui_next_extent, count);
	xfs_trans_add_item(tp, &cuip->cui_item);
	set_bit(XFS_LI_DIRTY, &cuip->cui_item.li_flags);
	return &cuip->cui_item;
}

static const struct xfs_item_ops xfs_cui_item_ops = {
	.flags		= XFS_ITEM_INTENT,
	.iop_size	= xfs_cui_item_size,
	.iop_format	= xfs_cui_item_format,
	.iop_unpin	= xfs_cui_item_unpin,
	.iop_release	= xfs_cui_item_release,
	.iop_recover	= xfs_cui_item_recover,
	.iop_match	= xfs_cui_item_match,
	.iop_relog	= xfs_cui_item_relog,
};

static inline void
xfs_cui_copy_format(
	struct xfs_cui_log_format	*dst,
	const struct xfs_cui_log_format	*src)
{
	unsigned int			i;

	memcpy(dst, src, offsetof(struct xfs_cui_log_format, cui_extents));

	for (i = 0; i < src->cui_nextents; i++)
		memcpy(&dst->cui_extents[i], &src->cui_extents[i],
				sizeof(struct xfs_phys_extent));
}

 
STATIC int
xlog_recover_cui_commit_pass2(
	struct xlog			*log,
	struct list_head		*buffer_list,
	struct xlog_recover_item	*item,
	xfs_lsn_t			lsn)
{
	struct xfs_mount		*mp = log->l_mp;
	struct xfs_cui_log_item		*cuip;
	struct xfs_cui_log_format	*cui_formatp;
	size_t				len;

	cui_formatp = item->ri_buf[0].i_addr;

	if (item->ri_buf[0].i_len < xfs_cui_log_format_sizeof(0)) {
		XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp,
				item->ri_buf[0].i_addr, item->ri_buf[0].i_len);
		return -EFSCORRUPTED;
	}

	len = xfs_cui_log_format_sizeof(cui_formatp->cui_nextents);
	if (item->ri_buf[0].i_len != len) {
		XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp,
				item->ri_buf[0].i_addr, item->ri_buf[0].i_len);
		return -EFSCORRUPTED;
	}

	cuip = xfs_cui_init(mp, cui_formatp->cui_nextents);
	xfs_cui_copy_format(&cuip->cui_format, cui_formatp);
	atomic_set(&cuip->cui_next_extent, cui_formatp->cui_nextents);
	 
	xfs_trans_ail_insert(log->l_ailp, &cuip->cui_item, lsn);
	xfs_cui_release(cuip);
	return 0;
}

const struct xlog_recover_item_ops xlog_cui_item_ops = {
	.item_type		= XFS_LI_CUI,
	.commit_pass2		= xlog_recover_cui_commit_pass2,
};

 
STATIC int
xlog_recover_cud_commit_pass2(
	struct xlog			*log,
	struct list_head		*buffer_list,
	struct xlog_recover_item	*item,
	xfs_lsn_t			lsn)
{
	struct xfs_cud_log_format	*cud_formatp;

	cud_formatp = item->ri_buf[0].i_addr;
	if (item->ri_buf[0].i_len != sizeof(struct xfs_cud_log_format)) {
		XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, log->l_mp,
				item->ri_buf[0].i_addr, item->ri_buf[0].i_len);
		return -EFSCORRUPTED;
	}

	xlog_recover_release_intent(log, XFS_LI_CUI, cud_formatp->cud_cui_id);
	return 0;
}

const struct xlog_recover_item_ops xlog_cud_item_ops = {
	.item_type		= XFS_LI_CUD,
	.commit_pass2		= xlog_recover_cud_commit_pass2,
};
