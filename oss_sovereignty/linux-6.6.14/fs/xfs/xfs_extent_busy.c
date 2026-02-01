
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_shared.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_alloc.h"
#include "xfs_extent_busy.h"
#include "xfs_trace.h"
#include "xfs_trans.h"
#include "xfs_log.h"
#include "xfs_ag.h"

static void
xfs_extent_busy_insert_list(
	struct xfs_perag	*pag,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	unsigned int		flags,
	struct list_head	*busy_list)
{
	struct xfs_extent_busy	*new;
	struct xfs_extent_busy	*busyp;
	struct rb_node		**rbp;
	struct rb_node		*parent = NULL;

	new = kmem_zalloc(sizeof(struct xfs_extent_busy), 0);
	new->agno = pag->pag_agno;
	new->bno = bno;
	new->length = len;
	INIT_LIST_HEAD(&new->list);
	new->flags = flags;

	 
	trace_xfs_extent_busy(pag->pag_mount, pag->pag_agno, bno, len);

	spin_lock(&pag->pagb_lock);
	rbp = &pag->pagb_tree.rb_node;
	while (*rbp) {
		parent = *rbp;
		busyp = rb_entry(parent, struct xfs_extent_busy, rb_node);

		if (new->bno < busyp->bno) {
			rbp = &(*rbp)->rb_left;
			ASSERT(new->bno + new->length <= busyp->bno);
		} else if (new->bno > busyp->bno) {
			rbp = &(*rbp)->rb_right;
			ASSERT(bno >= busyp->bno + busyp->length);
		} else {
			ASSERT(0);
		}
	}

	rb_link_node(&new->rb_node, parent, rbp);
	rb_insert_color(&new->rb_node, &pag->pagb_tree);

	 
	list_add_tail(&new->list, busy_list);
	spin_unlock(&pag->pagb_lock);
}

void
xfs_extent_busy_insert(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	unsigned int		flags)
{
	xfs_extent_busy_insert_list(pag, bno, len, flags, &tp->t_busy);
}

void
xfs_extent_busy_insert_discard(
	struct xfs_perag	*pag,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	struct list_head	*busy_list)
{
	xfs_extent_busy_insert_list(pag, bno, len, XFS_EXTENT_BUSY_DISCARDED,
			busy_list);
}

 
int
xfs_extent_busy_search(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag,
	xfs_agblock_t		bno,
	xfs_extlen_t		len)
{
	struct rb_node		*rbp;
	struct xfs_extent_busy	*busyp;
	int			match = 0;

	 
	spin_lock(&pag->pagb_lock);
	rbp = pag->pagb_tree.rb_node;
	while (rbp) {
		busyp = rb_entry(rbp, struct xfs_extent_busy, rb_node);
		if (bno < busyp->bno) {
			 
			if (bno + len > busyp->bno)
				match = -1;
			rbp = rbp->rb_left;
		} else if (bno > busyp->bno) {
			 
			if (bno < busyp->bno + busyp->length)
				match = -1;
			rbp = rbp->rb_right;
		} else {
			 
			match = (busyp->length == len) ? 1 : -1;
			break;
		}
	}
	spin_unlock(&pag->pagb_lock);
	return match;
}

 
STATIC bool
xfs_extent_busy_update_extent(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag,
	struct xfs_extent_busy	*busyp,
	xfs_agblock_t		fbno,
	xfs_extlen_t		flen,
	bool			userdata) __releases(&pag->pagb_lock)
					  __acquires(&pag->pagb_lock)
{
	xfs_agblock_t		fend = fbno + flen;
	xfs_agblock_t		bbno = busyp->bno;
	xfs_agblock_t		bend = bbno + busyp->length;

	 
	if (busyp->flags & XFS_EXTENT_BUSY_DISCARDED) {
		spin_unlock(&pag->pagb_lock);
		delay(1);
		spin_lock(&pag->pagb_lock);
		return false;
	}

	 
	if (userdata)
		goto out_force_log;

	if (bbno < fbno && bend > fend) {
		 

		 
		goto out_force_log;
	} else if (bbno >= fbno && bend <= fend) {
		 

		 
		rb_erase(&busyp->rb_node, &pag->pagb_tree);
		busyp->length = 0;
		return false;
	} else if (fend < bend) {
		 
		busyp->bno = fend;
		busyp->length = bend - fend;
	} else if (bbno < fbno) {
		 
		busyp->length = fbno - busyp->bno;
	} else {
		ASSERT(0);
	}

	trace_xfs_extent_busy_reuse(mp, pag->pag_agno, fbno, flen);
	return true;

out_force_log:
	spin_unlock(&pag->pagb_lock);
	xfs_log_force(mp, XFS_LOG_SYNC);
	trace_xfs_extent_busy_force(mp, pag->pag_agno, fbno, flen);
	spin_lock(&pag->pagb_lock);
	return false;
}


 
void
xfs_extent_busy_reuse(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag,
	xfs_agblock_t		fbno,
	xfs_extlen_t		flen,
	bool			userdata)
{
	struct rb_node		*rbp;

	ASSERT(flen > 0);
	spin_lock(&pag->pagb_lock);
restart:
	rbp = pag->pagb_tree.rb_node;
	while (rbp) {
		struct xfs_extent_busy *busyp =
			rb_entry(rbp, struct xfs_extent_busy, rb_node);
		xfs_agblock_t	bbno = busyp->bno;
		xfs_agblock_t	bend = bbno + busyp->length;

		if (fbno + flen <= bbno) {
			rbp = rbp->rb_left;
			continue;
		} else if (fbno >= bend) {
			rbp = rbp->rb_right;
			continue;
		}

		if (!xfs_extent_busy_update_extent(mp, pag, busyp, fbno, flen,
						  userdata))
			goto restart;
	}
	spin_unlock(&pag->pagb_lock);
}

 
bool
xfs_extent_busy_trim(
	struct xfs_alloc_arg	*args,
	xfs_agblock_t		*bno,
	xfs_extlen_t		*len,
	unsigned		*busy_gen)
{
	xfs_agblock_t		fbno;
	xfs_extlen_t		flen;
	struct rb_node		*rbp;
	bool			ret = false;

	ASSERT(*len > 0);

	spin_lock(&args->pag->pagb_lock);
	fbno = *bno;
	flen = *len;
	rbp = args->pag->pagb_tree.rb_node;
	while (rbp && flen >= args->minlen) {
		struct xfs_extent_busy *busyp =
			rb_entry(rbp, struct xfs_extent_busy, rb_node);
		xfs_agblock_t	fend = fbno + flen;
		xfs_agblock_t	bbno = busyp->bno;
		xfs_agblock_t	bend = bbno + busyp->length;

		if (fend <= bbno) {
			rbp = rbp->rb_left;
			continue;
		} else if (fbno >= bend) {
			rbp = rbp->rb_right;
			continue;
		}

		if (bbno <= fbno) {
			 

			 
			if (fend <= bend)
				goto fail;

			 
			fbno = bend;
		} else if (bend >= fend) {
			 

			 
			fend = bbno;
		} else {
			 

			 
			if (bbno - fbno >= args->maxlen) {
				 
				fend = bbno;
			} else if (fend - bend >= args->maxlen * 4) {
				 
				fbno = bend;
			} else if (bbno - fbno >= args->minlen) {
				 
				fend = bbno;
			} else {
				goto fail;
			}
		}

		flen = fend - fbno;
	}
out:

	if (fbno != *bno || flen != *len) {
		trace_xfs_extent_busy_trim(args->mp, args->agno, *bno, *len,
					  fbno, flen);
		*bno = fbno;
		*len = flen;
		*busy_gen = args->pag->pagb_gen;
		ret = true;
	}
	spin_unlock(&args->pag->pagb_lock);
	return ret;
fail:
	 
	flen = 0;
	goto out;
}

STATIC void
xfs_extent_busy_clear_one(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag,
	struct xfs_extent_busy	*busyp)
{
	if (busyp->length) {
		trace_xfs_extent_busy_clear(mp, busyp->agno, busyp->bno,
						busyp->length);
		rb_erase(&busyp->rb_node, &pag->pagb_tree);
	}

	list_del_init(&busyp->list);
	kmem_free(busyp);
}

static void
xfs_extent_busy_put_pag(
	struct xfs_perag	*pag,
	bool			wakeup)
		__releases(pag->pagb_lock)
{
	if (wakeup) {
		pag->pagb_gen++;
		wake_up_all(&pag->pagb_wait);
	}

	spin_unlock(&pag->pagb_lock);
	xfs_perag_put(pag);
}

 
void
xfs_extent_busy_clear(
	struct xfs_mount	*mp,
	struct list_head	*list,
	bool			do_discard)
{
	struct xfs_extent_busy	*busyp, *n;
	struct xfs_perag	*pag = NULL;
	xfs_agnumber_t		agno = NULLAGNUMBER;
	bool			wakeup = false;

	list_for_each_entry_safe(busyp, n, list, list) {
		if (busyp->agno != agno) {
			if (pag)
				xfs_extent_busy_put_pag(pag, wakeup);
			agno = busyp->agno;
			pag = xfs_perag_get(mp, agno);
			spin_lock(&pag->pagb_lock);
			wakeup = false;
		}

		if (do_discard && busyp->length &&
		    !(busyp->flags & XFS_EXTENT_BUSY_SKIP_DISCARD)) {
			busyp->flags = XFS_EXTENT_BUSY_DISCARDED;
		} else {
			xfs_extent_busy_clear_one(mp, pag, busyp);
			wakeup = true;
		}
	}

	if (pag)
		xfs_extent_busy_put_pag(pag, wakeup);
}

 
int
xfs_extent_busy_flush(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	unsigned		busy_gen,
	uint32_t		alloc_flags)
{
	DEFINE_WAIT		(wait);
	int			error;

	error = xfs_log_force(tp->t_mountp, XFS_LOG_SYNC);
	if (error)
		return error;

	 
	if (!list_empty(&tp->t_busy)) {
		if (alloc_flags & XFS_ALLOC_FLAG_TRYFLUSH)
			return 0;

		if (busy_gen != READ_ONCE(pag->pagb_gen))
			return 0;

		if (alloc_flags & XFS_ALLOC_FLAG_FREEING)
			return -EAGAIN;
	}

	 
	do {
		prepare_to_wait(&pag->pagb_wait, &wait, TASK_KILLABLE);
		if  (busy_gen != READ_ONCE(pag->pagb_gen))
			break;
		schedule();
	} while (1);

	finish_wait(&pag->pagb_wait, &wait);
	return 0;
}

void
xfs_extent_busy_wait_all(
	struct xfs_mount	*mp)
{
	struct xfs_perag	*pag;
	DEFINE_WAIT		(wait);
	xfs_agnumber_t		agno;

	for_each_perag(mp, agno, pag) {
		do {
			prepare_to_wait(&pag->pagb_wait, &wait, TASK_KILLABLE);
			if  (RB_EMPTY_ROOT(&pag->pagb_tree))
				break;
			schedule();
		} while (1);
		finish_wait(&pag->pagb_wait, &wait);
	}
}

 
int
xfs_extent_busy_ag_cmp(
	void			*priv,
	const struct list_head	*l1,
	const struct list_head	*l2)
{
	struct xfs_extent_busy	*b1 =
		container_of(l1, struct xfs_extent_busy, list);
	struct xfs_extent_busy	*b2 =
		container_of(l2, struct xfs_extent_busy, list);
	s32 diff;

	diff = b1->agno - b2->agno;
	if (!diff)
		diff = b1->bno - b2->bno;
	return diff;
}
