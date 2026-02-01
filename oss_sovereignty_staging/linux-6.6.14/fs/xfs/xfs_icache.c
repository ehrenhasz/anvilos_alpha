
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_inode_item.h"
#include "xfs_quota.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_bmap_util.h"
#include "xfs_dquot_item.h"
#include "xfs_dquot.h"
#include "xfs_reflink.h"
#include "xfs_ialloc.h"
#include "xfs_ag.h"
#include "xfs_log_priv.h"

#include <linux/iversion.h>

 

 
#define XFS_ICI_RECLAIM_TAG	0
 
#define XFS_ICI_BLOCKGC_TAG	1

 
enum xfs_icwalk_goal {
	 
	XFS_ICWALK_BLOCKGC	= XFS_ICI_BLOCKGC_TAG,
	XFS_ICWALK_RECLAIM	= XFS_ICI_RECLAIM_TAG,
};

static int xfs_icwalk(struct xfs_mount *mp,
		enum xfs_icwalk_goal goal, struct xfs_icwalk *icw);
static int xfs_icwalk_ag(struct xfs_perag *pag,
		enum xfs_icwalk_goal goal, struct xfs_icwalk *icw);

 

 
#define XFS_ICWALK_FLAG_SCAN_LIMIT	(1U << 28)

#define XFS_ICWALK_FLAG_RECLAIM_SICK	(1U << 27)
#define XFS_ICWALK_FLAG_UNION		(1U << 26)  

#define XFS_ICWALK_PRIVATE_FLAGS	(XFS_ICWALK_FLAG_SCAN_LIMIT | \
					 XFS_ICWALK_FLAG_RECLAIM_SICK | \
					 XFS_ICWALK_FLAG_UNION)

 
struct xfs_inode *
xfs_inode_alloc(
	struct xfs_mount	*mp,
	xfs_ino_t		ino)
{
	struct xfs_inode	*ip;

	 
	ip = alloc_inode_sb(mp->m_super, xfs_inode_cache, GFP_KERNEL | __GFP_NOFAIL);

	if (inode_init_always(mp->m_super, VFS_I(ip))) {
		kmem_cache_free(xfs_inode_cache, ip);
		return NULL;
	}

	 
	VFS_I(ip)->i_mode = 0;
	VFS_I(ip)->i_state = 0;
	mapping_set_large_folios(VFS_I(ip)->i_mapping);

	XFS_STATS_INC(mp, vn_active);
	ASSERT(atomic_read(&ip->i_pincount) == 0);
	ASSERT(ip->i_ino == 0);

	 
	ip->i_ino = ino;
	ip->i_mount = mp;
	memset(&ip->i_imap, 0, sizeof(struct xfs_imap));
	ip->i_cowfp = NULL;
	memset(&ip->i_af, 0, sizeof(ip->i_af));
	ip->i_af.if_format = XFS_DINODE_FMT_EXTENTS;
	memset(&ip->i_df, 0, sizeof(ip->i_df));
	ip->i_flags = 0;
	ip->i_delayed_blks = 0;
	ip->i_diflags2 = mp->m_ino_geo.new_diflags2;
	ip->i_nblocks = 0;
	ip->i_forkoff = 0;
	ip->i_sick = 0;
	ip->i_checked = 0;
	INIT_WORK(&ip->i_ioend_work, xfs_end_io);
	INIT_LIST_HEAD(&ip->i_ioend_list);
	spin_lock_init(&ip->i_ioend_lock);
	ip->i_next_unlinked = NULLAGINO;
	ip->i_prev_unlinked = 0;

	return ip;
}

STATIC void
xfs_inode_free_callback(
	struct rcu_head		*head)
{
	struct inode		*inode = container_of(head, struct inode, i_rcu);
	struct xfs_inode	*ip = XFS_I(inode);

	switch (VFS_I(ip)->i_mode & S_IFMT) {
	case S_IFREG:
	case S_IFDIR:
	case S_IFLNK:
		xfs_idestroy_fork(&ip->i_df);
		break;
	}

	xfs_ifork_zap_attr(ip);

	if (ip->i_cowfp) {
		xfs_idestroy_fork(ip->i_cowfp);
		kmem_cache_free(xfs_ifork_cache, ip->i_cowfp);
	}
	if (ip->i_itemp) {
		ASSERT(!test_bit(XFS_LI_IN_AIL,
				 &ip->i_itemp->ili_item.li_flags));
		xfs_inode_item_destroy(ip);
		ip->i_itemp = NULL;
	}

	kmem_cache_free(xfs_inode_cache, ip);
}

static void
__xfs_inode_free(
	struct xfs_inode	*ip)
{
	 
	ASSERT(atomic_read(&ip->i_pincount) == 0);
	ASSERT(!ip->i_itemp || list_empty(&ip->i_itemp->ili_item.li_bio_list));
	XFS_STATS_DEC(ip->i_mount, vn_active);

	call_rcu(&VFS_I(ip)->i_rcu, xfs_inode_free_callback);
}

void
xfs_inode_free(
	struct xfs_inode	*ip)
{
	ASSERT(!xfs_iflags_test(ip, XFS_IFLUSHING));

	 
	spin_lock(&ip->i_flags_lock);
	ip->i_flags = XFS_IRECLAIM;
	ip->i_ino = 0;
	spin_unlock(&ip->i_flags_lock);

	__xfs_inode_free(ip);
}

 
static void
xfs_reclaim_work_queue(
	struct xfs_mount        *mp)
{

	rcu_read_lock();
	if (radix_tree_tagged(&mp->m_perag_tree, XFS_ICI_RECLAIM_TAG)) {
		queue_delayed_work(mp->m_reclaim_workqueue, &mp->m_reclaim_work,
			msecs_to_jiffies(xfs_syncd_centisecs / 6 * 10));
	}
	rcu_read_unlock();
}

 
static inline void
xfs_blockgc_queue(
	struct xfs_perag	*pag)
{
	struct xfs_mount	*mp = pag->pag_mount;

	if (!xfs_is_blockgc_enabled(mp))
		return;

	rcu_read_lock();
	if (radix_tree_tagged(&pag->pag_ici_root, XFS_ICI_BLOCKGC_TAG))
		queue_delayed_work(pag->pag_mount->m_blockgc_wq,
				   &pag->pag_blockgc_work,
				   msecs_to_jiffies(xfs_blockgc_secs * 1000));
	rcu_read_unlock();
}

 
static void
xfs_perag_set_inode_tag(
	struct xfs_perag	*pag,
	xfs_agino_t		agino,
	unsigned int		tag)
{
	struct xfs_mount	*mp = pag->pag_mount;
	bool			was_tagged;

	lockdep_assert_held(&pag->pag_ici_lock);

	was_tagged = radix_tree_tagged(&pag->pag_ici_root, tag);
	radix_tree_tag_set(&pag->pag_ici_root, agino, tag);

	if (tag == XFS_ICI_RECLAIM_TAG)
		pag->pag_ici_reclaimable++;

	if (was_tagged)
		return;

	 
	spin_lock(&mp->m_perag_lock);
	radix_tree_tag_set(&mp->m_perag_tree, pag->pag_agno, tag);
	spin_unlock(&mp->m_perag_lock);

	 
	switch (tag) {
	case XFS_ICI_RECLAIM_TAG:
		xfs_reclaim_work_queue(mp);
		break;
	case XFS_ICI_BLOCKGC_TAG:
		xfs_blockgc_queue(pag);
		break;
	}

	trace_xfs_perag_set_inode_tag(pag, _RET_IP_);
}

 
static void
xfs_perag_clear_inode_tag(
	struct xfs_perag	*pag,
	xfs_agino_t		agino,
	unsigned int		tag)
{
	struct xfs_mount	*mp = pag->pag_mount;

	lockdep_assert_held(&pag->pag_ici_lock);

	 
	if (agino != NULLAGINO)
		radix_tree_tag_clear(&pag->pag_ici_root, agino, tag);
	else
		ASSERT(tag == XFS_ICI_RECLAIM_TAG);

	if (tag == XFS_ICI_RECLAIM_TAG)
		pag->pag_ici_reclaimable--;

	if (radix_tree_tagged(&pag->pag_ici_root, tag))
		return;

	 
	spin_lock(&mp->m_perag_lock);
	radix_tree_tag_clear(&mp->m_perag_tree, pag->pag_agno, tag);
	spin_unlock(&mp->m_perag_lock);

	trace_xfs_perag_clear_inode_tag(pag, _RET_IP_);
}

 
static int
xfs_reinit_inode(
	struct xfs_mount	*mp,
	struct inode		*inode)
{
	int			error;
	uint32_t		nlink = inode->i_nlink;
	uint32_t		generation = inode->i_generation;
	uint64_t		version = inode_peek_iversion(inode);
	umode_t			mode = inode->i_mode;
	dev_t			dev = inode->i_rdev;
	kuid_t			uid = inode->i_uid;
	kgid_t			gid = inode->i_gid;

	error = inode_init_always(mp->m_super, inode);

	set_nlink(inode, nlink);
	inode->i_generation = generation;
	inode_set_iversion_queried(inode, version);
	inode->i_mode = mode;
	inode->i_rdev = dev;
	inode->i_uid = uid;
	inode->i_gid = gid;
	mapping_set_large_folios(inode->i_mapping);
	return error;
}

 
static int
xfs_iget_recycle(
	struct xfs_perag	*pag,
	struct xfs_inode	*ip) __releases(&ip->i_flags_lock)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct inode		*inode = VFS_I(ip);
	int			error;

	trace_xfs_iget_recycle(ip);

	if (!xfs_ilock_nowait(ip, XFS_ILOCK_EXCL))
		return -EAGAIN;

	 
	ip->i_flags |= XFS_IRECLAIM;

	spin_unlock(&ip->i_flags_lock);
	rcu_read_unlock();

	ASSERT(!rwsem_is_locked(&inode->i_rwsem));
	error = xfs_reinit_inode(mp, inode);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	if (error) {
		 
		rcu_read_lock();
		spin_lock(&ip->i_flags_lock);
		ip->i_flags &= ~(XFS_INEW | XFS_IRECLAIM);
		ASSERT(ip->i_flags & XFS_IRECLAIMABLE);
		spin_unlock(&ip->i_flags_lock);
		rcu_read_unlock();

		trace_xfs_iget_recycle_fail(ip);
		return error;
	}

	spin_lock(&pag->pag_ici_lock);
	spin_lock(&ip->i_flags_lock);

	 
	ip->i_flags &= ~XFS_IRECLAIM_RESET_FLAGS;
	ip->i_flags |= XFS_INEW;
	xfs_perag_clear_inode_tag(pag, XFS_INO_TO_AGINO(mp, ip->i_ino),
			XFS_ICI_RECLAIM_TAG);
	inode->i_state = I_NEW;
	spin_unlock(&ip->i_flags_lock);
	spin_unlock(&pag->pag_ici_lock);

	return 0;
}

 
static int
xfs_iget_check_free_state(
	struct xfs_inode	*ip,
	int			flags)
{
	if (flags & XFS_IGET_CREATE) {
		 
		if (VFS_I(ip)->i_mode != 0) {
			xfs_warn(ip->i_mount,
"Corruption detected! Free inode 0x%llx not marked free! (mode 0x%x)",
				ip->i_ino, VFS_I(ip)->i_mode);
			return -EFSCORRUPTED;
		}

		if (ip->i_nblocks != 0) {
			xfs_warn(ip->i_mount,
"Corruption detected! Free inode 0x%llx has blocks allocated!",
				ip->i_ino);
			return -EFSCORRUPTED;
		}
		return 0;
	}

	 
	if (VFS_I(ip)->i_mode == 0)
		return -ENOENT;

	return 0;
}

 
static bool
xfs_inodegc_queue_all(
	struct xfs_mount	*mp)
{
	struct xfs_inodegc	*gc;
	int			cpu;
	bool			ret = false;

	for_each_cpu(cpu, &mp->m_inodegc_cpumask) {
		gc = per_cpu_ptr(mp->m_inodegc, cpu);
		if (!llist_empty(&gc->list)) {
			mod_delayed_work_on(cpu, mp->m_inodegc_wq, &gc->work, 0);
			ret = true;
		}
	}

	return ret;
}

 
static int
xfs_inodegc_wait_all(
	struct xfs_mount	*mp)
{
	int			cpu;
	int			error = 0;

	flush_workqueue(mp->m_inodegc_wq);
	for_each_cpu(cpu, &mp->m_inodegc_cpumask) {
		struct xfs_inodegc	*gc;

		gc = per_cpu_ptr(mp->m_inodegc, cpu);
		if (gc->error && !error)
			error = gc->error;
		gc->error = 0;
	}

	return error;
}

 
static int
xfs_iget_cache_hit(
	struct xfs_perag	*pag,
	struct xfs_inode	*ip,
	xfs_ino_t		ino,
	int			flags,
	int			lock_flags) __releases(RCU)
{
	struct inode		*inode = VFS_I(ip);
	struct xfs_mount	*mp = ip->i_mount;
	int			error;

	 
	spin_lock(&ip->i_flags_lock);
	if (ip->i_ino != ino)
		goto out_skip;

	 
	if (ip->i_flags & (XFS_INEW | XFS_IRECLAIM | XFS_INACTIVATING))
		goto out_skip;

	if (ip->i_flags & XFS_NEED_INACTIVE) {
		 
		if (VFS_I(ip)->i_nlink == 0) {
			error = -ENOENT;
			goto out_error;
		}
		goto out_inodegc_flush;
	}

	 
	error = xfs_iget_check_free_state(ip, flags);
	if (error)
		goto out_error;

	 
	if ((flags & XFS_IGET_INCORE) &&
	    (ip->i_flags & XFS_IRECLAIMABLE))
		goto out_skip;

	 
	if (ip->i_flags & XFS_IRECLAIMABLE) {
		 
		error = xfs_iget_recycle(pag, ip);
		if (error == -EAGAIN)
			goto out_skip;
		if (error)
			return error;
	} else {
		 
		if (!igrab(inode))
			goto out_skip;

		 
		spin_unlock(&ip->i_flags_lock);
		rcu_read_unlock();
		trace_xfs_iget_hit(ip);
	}

	if (lock_flags != 0)
		xfs_ilock(ip, lock_flags);

	if (!(flags & XFS_IGET_INCORE))
		xfs_iflags_clear(ip, XFS_ISTALE);
	XFS_STATS_INC(mp, xs_ig_found);

	return 0;

out_skip:
	trace_xfs_iget_skip(ip);
	XFS_STATS_INC(mp, xs_ig_frecycle);
	error = -EAGAIN;
out_error:
	spin_unlock(&ip->i_flags_lock);
	rcu_read_unlock();
	return error;

out_inodegc_flush:
	spin_unlock(&ip->i_flags_lock);
	rcu_read_unlock();
	 
	if (xfs_is_inodegc_enabled(mp))
		xfs_inodegc_queue_all(mp);
	return -EAGAIN;
}

static int
xfs_iget_cache_miss(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag,
	xfs_trans_t		*tp,
	xfs_ino_t		ino,
	struct xfs_inode	**ipp,
	int			flags,
	int			lock_flags)
{
	struct xfs_inode	*ip;
	int			error;
	xfs_agino_t		agino = XFS_INO_TO_AGINO(mp, ino);
	int			iflags;

	ip = xfs_inode_alloc(mp, ino);
	if (!ip)
		return -ENOMEM;

	error = xfs_imap(pag, tp, ip->i_ino, &ip->i_imap, flags);
	if (error)
		goto out_destroy;

	 
	if (xfs_has_v3inodes(mp) &&
	    (flags & XFS_IGET_CREATE) && !xfs_has_ikeep(mp)) {
		VFS_I(ip)->i_generation = get_random_u32();
	} else {
		struct xfs_buf		*bp;

		error = xfs_imap_to_bp(mp, tp, &ip->i_imap, &bp);
		if (error)
			goto out_destroy;

		error = xfs_inode_from_disk(ip,
				xfs_buf_offset(bp, ip->i_imap.im_boffset));
		if (!error)
			xfs_buf_set_ref(bp, XFS_INO_REF);
		xfs_trans_brelse(tp, bp);

		if (error)
			goto out_destroy;
	}

	trace_xfs_iget_miss(ip);

	 
	error = xfs_iget_check_free_state(ip, flags);
	if (error)
		goto out_destroy;

	 
	if (radix_tree_preload(GFP_NOFS)) {
		error = -EAGAIN;
		goto out_destroy;
	}

	 
	if (lock_flags) {
		if (!xfs_ilock_nowait(ip, lock_flags))
			BUG();
	}

	 
	iflags = XFS_INEW;
	if (flags & XFS_IGET_DONTCACHE)
		d_mark_dontcache(VFS_I(ip));
	ip->i_udquot = NULL;
	ip->i_gdquot = NULL;
	ip->i_pdquot = NULL;
	xfs_iflags_set(ip, iflags);

	 
	spin_lock(&pag->pag_ici_lock);
	error = radix_tree_insert(&pag->pag_ici_root, agino, ip);
	if (unlikely(error)) {
		WARN_ON(error != -EEXIST);
		XFS_STATS_INC(mp, xs_ig_dup);
		error = -EAGAIN;
		goto out_preload_end;
	}
	spin_unlock(&pag->pag_ici_lock);
	radix_tree_preload_end();

	*ipp = ip;
	return 0;

out_preload_end:
	spin_unlock(&pag->pag_ici_lock);
	radix_tree_preload_end();
	if (lock_flags)
		xfs_iunlock(ip, lock_flags);
out_destroy:
	__destroy_inode(VFS_I(ip));
	xfs_inode_free(ip);
	return error;
}

 
int
xfs_iget(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_ino_t		ino,
	uint			flags,
	uint			lock_flags,
	struct xfs_inode	**ipp)
{
	struct xfs_inode	*ip;
	struct xfs_perag	*pag;
	xfs_agino_t		agino;
	int			error;

	ASSERT((lock_flags & (XFS_IOLOCK_EXCL | XFS_IOLOCK_SHARED)) == 0);

	 
	if (!ino || XFS_INO_TO_AGNO(mp, ino) >= mp->m_sb.sb_agcount)
		return -EINVAL;

	XFS_STATS_INC(mp, xs_ig_attempts);

	 
	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, ino));
	agino = XFS_INO_TO_AGINO(mp, ino);

again:
	error = 0;
	rcu_read_lock();
	ip = radix_tree_lookup(&pag->pag_ici_root, agino);

	if (ip) {
		error = xfs_iget_cache_hit(pag, ip, ino, flags, lock_flags);
		if (error)
			goto out_error_or_again;
	} else {
		rcu_read_unlock();
		if (flags & XFS_IGET_INCORE) {
			error = -ENODATA;
			goto out_error_or_again;
		}
		XFS_STATS_INC(mp, xs_ig_missed);

		error = xfs_iget_cache_miss(mp, pag, tp, ino, &ip,
							flags, lock_flags);
		if (error)
			goto out_error_or_again;
	}
	xfs_perag_put(pag);

	*ipp = ip;

	 
	if (xfs_iflags_test(ip, XFS_INEW) && VFS_I(ip)->i_mode != 0)
		xfs_setup_existing_inode(ip);
	return 0;

out_error_or_again:
	if (!(flags & (XFS_IGET_INCORE | XFS_IGET_NORETRY)) &&
	    error == -EAGAIN) {
		delay(1);
		goto again;
	}
	xfs_perag_put(pag);
	return error;
}

 
static bool
xfs_reclaim_igrab(
	struct xfs_inode	*ip,
	struct xfs_icwalk	*icw)
{
	ASSERT(rcu_read_lock_held());

	spin_lock(&ip->i_flags_lock);
	if (!__xfs_iflags_test(ip, XFS_IRECLAIMABLE) ||
	    __xfs_iflags_test(ip, XFS_IRECLAIM)) {
		 
		spin_unlock(&ip->i_flags_lock);
		return false;
	}

	 
	if (ip->i_sick &&
	    (!icw || !(icw->icw_flags & XFS_ICWALK_FLAG_RECLAIM_SICK))) {
		spin_unlock(&ip->i_flags_lock);
		return false;
	}

	__xfs_iflags_set(ip, XFS_IRECLAIM);
	spin_unlock(&ip->i_flags_lock);
	return true;
}

 
static void
xfs_reclaim_inode(
	struct xfs_inode	*ip,
	struct xfs_perag	*pag)
{
	xfs_ino_t		ino = ip->i_ino;  

	if (!xfs_ilock_nowait(ip, XFS_ILOCK_EXCL))
		goto out;
	if (xfs_iflags_test_and_set(ip, XFS_IFLUSHING))
		goto out_iunlock;

	 
	if (xlog_is_shutdown(ip->i_mount->m_log)) {
		xfs_iunpin_wait(ip);
		xfs_iflush_shutdown_abort(ip);
		goto reclaim;
	}
	if (xfs_ipincount(ip))
		goto out_clear_flush;
	if (!xfs_inode_clean(ip))
		goto out_clear_flush;

	xfs_iflags_clear(ip, XFS_IFLUSHING);
reclaim:
	trace_xfs_inode_reclaiming(ip);

	 
	spin_lock(&ip->i_flags_lock);
	ip->i_flags = XFS_IRECLAIM;
	ip->i_ino = 0;
	ip->i_sick = 0;
	ip->i_checked = 0;
	spin_unlock(&ip->i_flags_lock);

	ASSERT(!ip->i_itemp || ip->i_itemp->ili_item.li_buf == NULL);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);

	XFS_STATS_INC(ip->i_mount, xs_ig_reclaims);
	 
	spin_lock(&pag->pag_ici_lock);
	if (!radix_tree_delete(&pag->pag_ici_root,
				XFS_INO_TO_AGINO(ip->i_mount, ino)))
		ASSERT(0);
	xfs_perag_clear_inode_tag(pag, NULLAGINO, XFS_ICI_RECLAIM_TAG);
	spin_unlock(&pag->pag_ici_lock);

	 
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	ASSERT(!ip->i_udquot && !ip->i_gdquot && !ip->i_pdquot);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	ASSERT(xfs_inode_clean(ip));

	__xfs_inode_free(ip);
	return;

out_clear_flush:
	xfs_iflags_clear(ip, XFS_IFLUSHING);
out_iunlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
out:
	xfs_iflags_clear(ip, XFS_IRECLAIM);
}

 
static inline bool
xfs_want_reclaim_sick(
	struct xfs_mount	*mp)
{
	return xfs_is_unmounting(mp) || xfs_has_norecovery(mp) ||
	       xfs_is_shutdown(mp);
}

void
xfs_reclaim_inodes(
	struct xfs_mount	*mp)
{
	struct xfs_icwalk	icw = {
		.icw_flags	= 0,
	};

	if (xfs_want_reclaim_sick(mp))
		icw.icw_flags |= XFS_ICWALK_FLAG_RECLAIM_SICK;

	while (radix_tree_tagged(&mp->m_perag_tree, XFS_ICI_RECLAIM_TAG)) {
		xfs_ail_push_all_sync(mp->m_ail);
		xfs_icwalk(mp, XFS_ICWALK_RECLAIM, &icw);
	}
}

 
long
xfs_reclaim_inodes_nr(
	struct xfs_mount	*mp,
	unsigned long		nr_to_scan)
{
	struct xfs_icwalk	icw = {
		.icw_flags	= XFS_ICWALK_FLAG_SCAN_LIMIT,
		.icw_scan_limit	= min_t(unsigned long, LONG_MAX, nr_to_scan),
	};

	if (xfs_want_reclaim_sick(mp))
		icw.icw_flags |= XFS_ICWALK_FLAG_RECLAIM_SICK;

	 
	xfs_reclaim_work_queue(mp);
	xfs_ail_push_all(mp->m_ail);

	xfs_icwalk(mp, XFS_ICWALK_RECLAIM, &icw);
	return 0;
}

 
long
xfs_reclaim_inodes_count(
	struct xfs_mount	*mp)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		ag = 0;
	long			reclaimable = 0;

	while ((pag = xfs_perag_get_tag(mp, ag, XFS_ICI_RECLAIM_TAG))) {
		ag = pag->pag_agno + 1;
		reclaimable += pag->pag_ici_reclaimable;
		xfs_perag_put(pag);
	}
	return reclaimable;
}

STATIC bool
xfs_icwalk_match_id(
	struct xfs_inode	*ip,
	struct xfs_icwalk	*icw)
{
	if ((icw->icw_flags & XFS_ICWALK_FLAG_UID) &&
	    !uid_eq(VFS_I(ip)->i_uid, icw->icw_uid))
		return false;

	if ((icw->icw_flags & XFS_ICWALK_FLAG_GID) &&
	    !gid_eq(VFS_I(ip)->i_gid, icw->icw_gid))
		return false;

	if ((icw->icw_flags & XFS_ICWALK_FLAG_PRID) &&
	    ip->i_projid != icw->icw_prid)
		return false;

	return true;
}

 
STATIC bool
xfs_icwalk_match_id_union(
	struct xfs_inode	*ip,
	struct xfs_icwalk	*icw)
{
	if ((icw->icw_flags & XFS_ICWALK_FLAG_UID) &&
	    uid_eq(VFS_I(ip)->i_uid, icw->icw_uid))
		return true;

	if ((icw->icw_flags & XFS_ICWALK_FLAG_GID) &&
	    gid_eq(VFS_I(ip)->i_gid, icw->icw_gid))
		return true;

	if ((icw->icw_flags & XFS_ICWALK_FLAG_PRID) &&
	    ip->i_projid == icw->icw_prid)
		return true;

	return false;
}

 
static bool
xfs_icwalk_match(
	struct xfs_inode	*ip,
	struct xfs_icwalk	*icw)
{
	bool			match;

	if (!icw)
		return true;

	if (icw->icw_flags & XFS_ICWALK_FLAG_UNION)
		match = xfs_icwalk_match_id_union(ip, icw);
	else
		match = xfs_icwalk_match_id(ip, icw);
	if (!match)
		return false;

	 
	if ((icw->icw_flags & XFS_ICWALK_FLAG_MINFILESIZE) &&
	    XFS_ISIZE(ip) < icw->icw_min_file_size)
		return false;

	return true;
}

 
void
xfs_reclaim_worker(
	struct work_struct *work)
{
	struct xfs_mount *mp = container_of(to_delayed_work(work),
					struct xfs_mount, m_reclaim_work);

	xfs_icwalk(mp, XFS_ICWALK_RECLAIM, NULL);
	xfs_reclaim_work_queue(mp);
}

STATIC int
xfs_inode_free_eofblocks(
	struct xfs_inode	*ip,
	struct xfs_icwalk	*icw,
	unsigned int		*lockflags)
{
	bool			wait;

	wait = icw && (icw->icw_flags & XFS_ICWALK_FLAG_SYNC);

	if (!xfs_iflags_test(ip, XFS_IEOFBLOCKS))
		return 0;

	 
	if (!wait && mapping_tagged(VFS_I(ip)->i_mapping, PAGECACHE_TAG_DIRTY))
		return 0;

	if (!xfs_icwalk_match(ip, icw))
		return 0;

	 
	if (!xfs_ilock_nowait(ip, XFS_IOLOCK_EXCL)) {
		if (wait)
			return -EAGAIN;
		return 0;
	}
	*lockflags |= XFS_IOLOCK_EXCL;

	if (xfs_can_free_eofblocks(ip, false))
		return xfs_free_eofblocks(ip);

	 
	trace_xfs_inode_free_eofblocks_invalid(ip);
	xfs_inode_clear_eofblocks_tag(ip);
	return 0;
}

static void
xfs_blockgc_set_iflag(
	struct xfs_inode	*ip,
	unsigned long		iflag)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_perag	*pag;

	ASSERT((iflag & ~(XFS_IEOFBLOCKS | XFS_ICOWBLOCKS)) == 0);

	 
	if (ip->i_flags & iflag)
		return;
	spin_lock(&ip->i_flags_lock);
	ip->i_flags |= iflag;
	spin_unlock(&ip->i_flags_lock);

	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, ip->i_ino));
	spin_lock(&pag->pag_ici_lock);

	xfs_perag_set_inode_tag(pag, XFS_INO_TO_AGINO(mp, ip->i_ino),
			XFS_ICI_BLOCKGC_TAG);

	spin_unlock(&pag->pag_ici_lock);
	xfs_perag_put(pag);
}

void
xfs_inode_set_eofblocks_tag(
	xfs_inode_t	*ip)
{
	trace_xfs_inode_set_eofblocks_tag(ip);
	return xfs_blockgc_set_iflag(ip, XFS_IEOFBLOCKS);
}

static void
xfs_blockgc_clear_iflag(
	struct xfs_inode	*ip,
	unsigned long		iflag)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_perag	*pag;
	bool			clear_tag;

	ASSERT((iflag & ~(XFS_IEOFBLOCKS | XFS_ICOWBLOCKS)) == 0);

	spin_lock(&ip->i_flags_lock);
	ip->i_flags &= ~iflag;
	clear_tag = (ip->i_flags & (XFS_IEOFBLOCKS | XFS_ICOWBLOCKS)) == 0;
	spin_unlock(&ip->i_flags_lock);

	if (!clear_tag)
		return;

	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, ip->i_ino));
	spin_lock(&pag->pag_ici_lock);

	xfs_perag_clear_inode_tag(pag, XFS_INO_TO_AGINO(mp, ip->i_ino),
			XFS_ICI_BLOCKGC_TAG);

	spin_unlock(&pag->pag_ici_lock);
	xfs_perag_put(pag);
}

void
xfs_inode_clear_eofblocks_tag(
	xfs_inode_t	*ip)
{
	trace_xfs_inode_clear_eofblocks_tag(ip);
	return xfs_blockgc_clear_iflag(ip, XFS_IEOFBLOCKS);
}

 
static bool
xfs_prep_free_cowblocks(
	struct xfs_inode	*ip)
{
	 
	if (!xfs_inode_has_cow_data(ip)) {
		trace_xfs_inode_free_cowblocks_invalid(ip);
		xfs_inode_clear_cowblocks_tag(ip);
		return false;
	}

	 
	if ((VFS_I(ip)->i_state & I_DIRTY_PAGES) ||
	    mapping_tagged(VFS_I(ip)->i_mapping, PAGECACHE_TAG_DIRTY) ||
	    mapping_tagged(VFS_I(ip)->i_mapping, PAGECACHE_TAG_WRITEBACK) ||
	    atomic_read(&VFS_I(ip)->i_dio_count))
		return false;

	return true;
}

 
STATIC int
xfs_inode_free_cowblocks(
	struct xfs_inode	*ip,
	struct xfs_icwalk	*icw,
	unsigned int		*lockflags)
{
	bool			wait;
	int			ret = 0;

	wait = icw && (icw->icw_flags & XFS_ICWALK_FLAG_SYNC);

	if (!xfs_iflags_test(ip, XFS_ICOWBLOCKS))
		return 0;

	if (!xfs_prep_free_cowblocks(ip))
		return 0;

	if (!xfs_icwalk_match(ip, icw))
		return 0;

	 
	if (!(*lockflags & XFS_IOLOCK_EXCL) &&
	    !xfs_ilock_nowait(ip, XFS_IOLOCK_EXCL)) {
		if (wait)
			return -EAGAIN;
		return 0;
	}
	*lockflags |= XFS_IOLOCK_EXCL;

	if (!xfs_ilock_nowait(ip, XFS_MMAPLOCK_EXCL)) {
		if (wait)
			return -EAGAIN;
		return 0;
	}
	*lockflags |= XFS_MMAPLOCK_EXCL;

	 
	if (xfs_prep_free_cowblocks(ip))
		ret = xfs_reflink_cancel_cow_range(ip, 0, NULLFILEOFF, false);
	return ret;
}

void
xfs_inode_set_cowblocks_tag(
	xfs_inode_t	*ip)
{
	trace_xfs_inode_set_cowblocks_tag(ip);
	return xfs_blockgc_set_iflag(ip, XFS_ICOWBLOCKS);
}

void
xfs_inode_clear_cowblocks_tag(
	xfs_inode_t	*ip)
{
	trace_xfs_inode_clear_cowblocks_tag(ip);
	return xfs_blockgc_clear_iflag(ip, XFS_ICOWBLOCKS);
}

 
void
xfs_blockgc_stop(
	struct xfs_mount	*mp)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno;

	if (!xfs_clear_blockgc_enabled(mp))
		return;

	for_each_perag(mp, agno, pag)
		cancel_delayed_work_sync(&pag->pag_blockgc_work);
	trace_xfs_blockgc_stop(mp, __return_address);
}

 
void
xfs_blockgc_start(
	struct xfs_mount	*mp)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno;

	if (xfs_set_blockgc_enabled(mp))
		return;

	trace_xfs_blockgc_start(mp, __return_address);
	for_each_perag_tag(mp, agno, pag, XFS_ICI_BLOCKGC_TAG)
		xfs_blockgc_queue(pag);
}

 
#define XFS_BLOCKGC_NOGRAB_IFLAGS	(XFS_INEW | \
					 XFS_NEED_INACTIVE | \
					 XFS_INACTIVATING | \
					 XFS_IRECLAIMABLE | \
					 XFS_IRECLAIM)
 
static bool
xfs_blockgc_igrab(
	struct xfs_inode	*ip)
{
	struct inode		*inode = VFS_I(ip);

	ASSERT(rcu_read_lock_held());

	 
	spin_lock(&ip->i_flags_lock);
	if (!ip->i_ino)
		goto out_unlock_noent;

	if (ip->i_flags & XFS_BLOCKGC_NOGRAB_IFLAGS)
		goto out_unlock_noent;
	spin_unlock(&ip->i_flags_lock);

	 
	if (xfs_is_shutdown(ip->i_mount))
		return false;

	 
	if (!igrab(inode))
		return false;

	 
	return true;

out_unlock_noent:
	spin_unlock(&ip->i_flags_lock);
	return false;
}

 
static int
xfs_blockgc_scan_inode(
	struct xfs_inode	*ip,
	struct xfs_icwalk	*icw)
{
	unsigned int		lockflags = 0;
	int			error;

	error = xfs_inode_free_eofblocks(ip, icw, &lockflags);
	if (error)
		goto unlock;

	error = xfs_inode_free_cowblocks(ip, icw, &lockflags);
unlock:
	if (lockflags)
		xfs_iunlock(ip, lockflags);
	xfs_irele(ip);
	return error;
}

 
void
xfs_blockgc_worker(
	struct work_struct	*work)
{
	struct xfs_perag	*pag = container_of(to_delayed_work(work),
					struct xfs_perag, pag_blockgc_work);
	struct xfs_mount	*mp = pag->pag_mount;
	int			error;

	trace_xfs_blockgc_worker(mp, __return_address);

	error = xfs_icwalk_ag(pag, XFS_ICWALK_BLOCKGC, NULL);
	if (error)
		xfs_info(mp, "AG %u preallocation gc worker failed, err=%d",
				pag->pag_agno, error);
	xfs_blockgc_queue(pag);
}

 
int
xfs_blockgc_free_space(
	struct xfs_mount	*mp,
	struct xfs_icwalk	*icw)
{
	int			error;

	trace_xfs_blockgc_free_space(mp, icw, _RET_IP_);

	error = xfs_icwalk(mp, XFS_ICWALK_BLOCKGC, icw);
	if (error)
		return error;

	return xfs_inodegc_flush(mp);
}

 
int
xfs_blockgc_flush_all(
	struct xfs_mount	*mp)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno;

	trace_xfs_blockgc_flush_all(mp, __return_address);

	 
	for_each_perag_tag(mp, agno, pag, XFS_ICI_BLOCKGC_TAG)
		mod_delayed_work(pag->pag_mount->m_blockgc_wq,
				&pag->pag_blockgc_work, 0);

	for_each_perag_tag(mp, agno, pag, XFS_ICI_BLOCKGC_TAG)
		flush_delayed_work(&pag->pag_blockgc_work);

	return xfs_inodegc_flush(mp);
}

 
int
xfs_blockgc_free_dquots(
	struct xfs_mount	*mp,
	struct xfs_dquot	*udqp,
	struct xfs_dquot	*gdqp,
	struct xfs_dquot	*pdqp,
	unsigned int		iwalk_flags)
{
	struct xfs_icwalk	icw = {0};
	bool			do_work = false;

	if (!udqp && !gdqp && !pdqp)
		return 0;

	 
	icw.icw_flags = XFS_ICWALK_FLAG_UNION | iwalk_flags;

	if (XFS_IS_UQUOTA_ENFORCED(mp) && udqp && xfs_dquot_lowsp(udqp)) {
		icw.icw_uid = make_kuid(mp->m_super->s_user_ns, udqp->q_id);
		icw.icw_flags |= XFS_ICWALK_FLAG_UID;
		do_work = true;
	}

	if (XFS_IS_UQUOTA_ENFORCED(mp) && gdqp && xfs_dquot_lowsp(gdqp)) {
		icw.icw_gid = make_kgid(mp->m_super->s_user_ns, gdqp->q_id);
		icw.icw_flags |= XFS_ICWALK_FLAG_GID;
		do_work = true;
	}

	if (XFS_IS_PQUOTA_ENFORCED(mp) && pdqp && xfs_dquot_lowsp(pdqp)) {
		icw.icw_prid = pdqp->q_id;
		icw.icw_flags |= XFS_ICWALK_FLAG_PRID;
		do_work = true;
	}

	if (!do_work)
		return 0;

	return xfs_blockgc_free_space(mp, &icw);
}

 
int
xfs_blockgc_free_quota(
	struct xfs_inode	*ip,
	unsigned int		iwalk_flags)
{
	return xfs_blockgc_free_dquots(ip->i_mount,
			xfs_inode_dquot(ip, XFS_DQTYPE_USER),
			xfs_inode_dquot(ip, XFS_DQTYPE_GROUP),
			xfs_inode_dquot(ip, XFS_DQTYPE_PROJ), iwalk_flags);
}

 

 
#define XFS_LOOKUP_BATCH	32


 
static inline bool
xfs_icwalk_igrab(
	enum xfs_icwalk_goal	goal,
	struct xfs_inode	*ip,
	struct xfs_icwalk	*icw)
{
	switch (goal) {
	case XFS_ICWALK_BLOCKGC:
		return xfs_blockgc_igrab(ip);
	case XFS_ICWALK_RECLAIM:
		return xfs_reclaim_igrab(ip, icw);
	default:
		return false;
	}
}

 
static inline int
xfs_icwalk_process_inode(
	enum xfs_icwalk_goal	goal,
	struct xfs_inode	*ip,
	struct xfs_perag	*pag,
	struct xfs_icwalk	*icw)
{
	int			error = 0;

	switch (goal) {
	case XFS_ICWALK_BLOCKGC:
		error = xfs_blockgc_scan_inode(ip, icw);
		break;
	case XFS_ICWALK_RECLAIM:
		xfs_reclaim_inode(ip, pag);
		break;
	}
	return error;
}

 
static int
xfs_icwalk_ag(
	struct xfs_perag	*pag,
	enum xfs_icwalk_goal	goal,
	struct xfs_icwalk	*icw)
{
	struct xfs_mount	*mp = pag->pag_mount;
	uint32_t		first_index;
	int			last_error = 0;
	int			skipped;
	bool			done;
	int			nr_found;

restart:
	done = false;
	skipped = 0;
	if (goal == XFS_ICWALK_RECLAIM)
		first_index = READ_ONCE(pag->pag_ici_reclaim_cursor);
	else
		first_index = 0;
	nr_found = 0;
	do {
		struct xfs_inode *batch[XFS_LOOKUP_BATCH];
		int		error = 0;
		int		i;

		rcu_read_lock();

		nr_found = radix_tree_gang_lookup_tag(&pag->pag_ici_root,
				(void **) batch, first_index,
				XFS_LOOKUP_BATCH, goal);
		if (!nr_found) {
			done = true;
			rcu_read_unlock();
			break;
		}

		 
		for (i = 0; i < nr_found; i++) {
			struct xfs_inode *ip = batch[i];

			if (done || !xfs_icwalk_igrab(goal, ip, icw))
				batch[i] = NULL;

			 
			if (XFS_INO_TO_AGNO(mp, ip->i_ino) != pag->pag_agno)
				continue;
			first_index = XFS_INO_TO_AGINO(mp, ip->i_ino + 1);
			if (first_index < XFS_INO_TO_AGINO(mp, ip->i_ino))
				done = true;
		}

		 
		rcu_read_unlock();

		for (i = 0; i < nr_found; i++) {
			if (!batch[i])
				continue;
			error = xfs_icwalk_process_inode(goal, batch[i], pag,
					icw);
			if (error == -EAGAIN) {
				skipped++;
				continue;
			}
			if (error && last_error != -EFSCORRUPTED)
				last_error = error;
		}

		 
		if (error == -EFSCORRUPTED)
			break;

		cond_resched();

		if (icw && (icw->icw_flags & XFS_ICWALK_FLAG_SCAN_LIMIT)) {
			icw->icw_scan_limit -= XFS_LOOKUP_BATCH;
			if (icw->icw_scan_limit <= 0)
				break;
		}
	} while (nr_found && !done);

	if (goal == XFS_ICWALK_RECLAIM) {
		if (done)
			first_index = 0;
		WRITE_ONCE(pag->pag_ici_reclaim_cursor, first_index);
	}

	if (skipped) {
		delay(1);
		goto restart;
	}
	return last_error;
}

 
static int
xfs_icwalk(
	struct xfs_mount	*mp,
	enum xfs_icwalk_goal	goal,
	struct xfs_icwalk	*icw)
{
	struct xfs_perag	*pag;
	int			error = 0;
	int			last_error = 0;
	xfs_agnumber_t		agno;

	for_each_perag_tag(mp, agno, pag, goal) {
		error = xfs_icwalk_ag(pag, goal, icw);
		if (error) {
			last_error = error;
			if (error == -EFSCORRUPTED) {
				xfs_perag_rele(pag);
				break;
			}
		}
	}
	return last_error;
	BUILD_BUG_ON(XFS_ICWALK_PRIVATE_FLAGS & XFS_ICWALK_FLAGS_VALID);
}

#ifdef DEBUG
static void
xfs_check_delalloc(
	struct xfs_inode	*ip,
	int			whichfork)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_bmbt_irec	got;
	struct xfs_iext_cursor	icur;

	if (!ifp || !xfs_iext_lookup_extent(ip, ifp, 0, &icur, &got))
		return;
	do {
		if (isnullstartblock(got.br_startblock)) {
			xfs_warn(ip->i_mount,
	"ino %llx %s fork has delalloc extent at [0x%llx:0x%llx]",
				ip->i_ino,
				whichfork == XFS_DATA_FORK ? "data" : "cow",
				got.br_startoff, got.br_blockcount);
		}
	} while (xfs_iext_next_extent(ifp, &icur, &got));
}
#else
#define xfs_check_delalloc(ip, whichfork)	do { } while (0)
#endif

 
static void
xfs_inodegc_set_reclaimable(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_perag	*pag;

	if (!xfs_is_shutdown(mp) && ip->i_delayed_blks) {
		xfs_check_delalloc(ip, XFS_DATA_FORK);
		xfs_check_delalloc(ip, XFS_COW_FORK);
		ASSERT(0);
	}

	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, ip->i_ino));
	spin_lock(&pag->pag_ici_lock);
	spin_lock(&ip->i_flags_lock);

	trace_xfs_inode_set_reclaimable(ip);
	ip->i_flags &= ~(XFS_NEED_INACTIVE | XFS_INACTIVATING);
	ip->i_flags |= XFS_IRECLAIMABLE;
	xfs_perag_set_inode_tag(pag, XFS_INO_TO_AGINO(mp, ip->i_ino),
			XFS_ICI_RECLAIM_TAG);

	spin_unlock(&ip->i_flags_lock);
	spin_unlock(&pag->pag_ici_lock);
	xfs_perag_put(pag);
}

 
static int
xfs_inodegc_inactivate(
	struct xfs_inode	*ip)
{
	int			error;

	trace_xfs_inode_inactivating(ip);
	error = xfs_inactive(ip);
	xfs_inodegc_set_reclaimable(ip);
	return error;

}

void
xfs_inodegc_worker(
	struct work_struct	*work)
{
	struct xfs_inodegc	*gc = container_of(to_delayed_work(work),
						struct xfs_inodegc, work);
	struct llist_node	*node = llist_del_all(&gc->list);
	struct xfs_inode	*ip, *n;
	struct xfs_mount	*mp = gc->mp;
	unsigned int		nofs_flag;

	 
	cpumask_clear_cpu(gc->cpu, &mp->m_inodegc_cpumask);
	smp_mb__after_atomic();

	WRITE_ONCE(gc->items, 0);

	if (!node)
		return;

	 
	nofs_flag = memalloc_nofs_save();

	ip = llist_entry(node, struct xfs_inode, i_gclist);
	trace_xfs_inodegc_worker(mp, READ_ONCE(gc->shrinker_hits));

	WRITE_ONCE(gc->shrinker_hits, 0);
	llist_for_each_entry_safe(ip, n, node, i_gclist) {
		int	error;

		xfs_iflags_set(ip, XFS_INACTIVATING);
		error = xfs_inodegc_inactivate(ip);
		if (error && !gc->error)
			gc->error = error;
	}

	memalloc_nofs_restore(nofs_flag);
}

 
void
xfs_inodegc_push(
	struct xfs_mount	*mp)
{
	if (!xfs_is_inodegc_enabled(mp))
		return;
	trace_xfs_inodegc_push(mp, __return_address);
	xfs_inodegc_queue_all(mp);
}

 
int
xfs_inodegc_flush(
	struct xfs_mount	*mp)
{
	xfs_inodegc_push(mp);
	trace_xfs_inodegc_flush(mp, __return_address);
	return xfs_inodegc_wait_all(mp);
}

 
void
xfs_inodegc_stop(
	struct xfs_mount	*mp)
{
	bool			rerun;

	if (!xfs_clear_inodegc_enabled(mp))
		return;

	 
	xfs_inodegc_queue_all(mp);
	do {
		flush_workqueue(mp->m_inodegc_wq);
		rerun = xfs_inodegc_queue_all(mp);
	} while (rerun);

	trace_xfs_inodegc_stop(mp, __return_address);
}

 
void
xfs_inodegc_start(
	struct xfs_mount	*mp)
{
	if (xfs_set_inodegc_enabled(mp))
		return;

	trace_xfs_inodegc_start(mp, __return_address);
	xfs_inodegc_queue_all(mp);
}

#ifdef CONFIG_XFS_RT
static inline bool
xfs_inodegc_want_queue_rt_file(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;

	if (!XFS_IS_REALTIME_INODE(ip))
		return false;

	if (__percpu_counter_compare(&mp->m_frextents,
				mp->m_low_rtexts[XFS_LOWSP_5_PCNT],
				XFS_FDBLOCKS_BATCH) < 0)
		return true;

	return false;
}
#else
# define xfs_inodegc_want_queue_rt_file(ip)	(false)
#endif  

 
static inline bool
xfs_inodegc_want_queue_work(
	struct xfs_inode	*ip,
	unsigned int		items)
{
	struct xfs_mount	*mp = ip->i_mount;

	if (items > mp->m_ino_geo.inodes_per_cluster)
		return true;

	if (__percpu_counter_compare(&mp->m_fdblocks,
				mp->m_low_space[XFS_LOWSP_5_PCNT],
				XFS_FDBLOCKS_BATCH) < 0)
		return true;

	if (xfs_inodegc_want_queue_rt_file(ip))
		return true;

	if (xfs_inode_near_dquot_enforcement(ip, XFS_DQTYPE_USER))
		return true;

	if (xfs_inode_near_dquot_enforcement(ip, XFS_DQTYPE_GROUP))
		return true;

	if (xfs_inode_near_dquot_enforcement(ip, XFS_DQTYPE_PROJ))
		return true;

	return false;
}

 
#define XFS_INODEGC_MAX_BACKLOG		(4 * XFS_INODES_PER_CHUNK)

 
static inline bool
xfs_inodegc_want_flush_work(
	struct xfs_inode	*ip,
	unsigned int		items,
	unsigned int		shrinker_hits)
{
	if (current->journal_info)
		return false;

	if (shrinker_hits > 0)
		return true;

	if (items > XFS_INODEGC_MAX_BACKLOG)
		return true;

	return false;
}

 
static void
xfs_inodegc_queue(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_inodegc	*gc;
	int			items;
	unsigned int		shrinker_hits;
	unsigned int		cpu_nr;
	unsigned long		queue_delay = 1;

	trace_xfs_inode_set_need_inactive(ip);
	spin_lock(&ip->i_flags_lock);
	ip->i_flags |= XFS_NEED_INACTIVE;
	spin_unlock(&ip->i_flags_lock);

	cpu_nr = get_cpu();
	gc = this_cpu_ptr(mp->m_inodegc);
	llist_add(&ip->i_gclist, &gc->list);
	items = READ_ONCE(gc->items);
	WRITE_ONCE(gc->items, items + 1);
	shrinker_hits = READ_ONCE(gc->shrinker_hits);

	 
	smp_mb__before_atomic();
	if (!cpumask_test_cpu(cpu_nr, &mp->m_inodegc_cpumask))
		cpumask_test_and_set_cpu(cpu_nr, &mp->m_inodegc_cpumask);

	 
	if (!xfs_is_inodegc_enabled(mp)) {
		put_cpu();
		return;
	}

	if (xfs_inodegc_want_queue_work(ip, items))
		queue_delay = 0;

	trace_xfs_inodegc_queue(mp, __return_address);
	mod_delayed_work_on(current_cpu(), mp->m_inodegc_wq, &gc->work,
			queue_delay);
	put_cpu();

	if (xfs_inodegc_want_flush_work(ip, items, shrinker_hits)) {
		trace_xfs_inodegc_throttle(mp, __return_address);
		flush_delayed_work(&gc->work);
	}
}

 
void
xfs_inode_mark_reclaimable(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	bool			need_inactive;

	XFS_STATS_INC(mp, vn_reclaim);

	 
	ASSERT_ALWAYS(!xfs_iflags_test(ip, XFS_ALL_IRECLAIM_FLAGS));

	need_inactive = xfs_inode_needs_inactive(ip);
	if (need_inactive) {
		xfs_inodegc_queue(ip);
		return;
	}

	 
	xfs_qm_dqdetach(ip);
	xfs_inodegc_set_reclaimable(ip);
}

 
#define XFS_INODEGC_SHRINKER_COUNT	(1UL << DEF_PRIORITY)
#define XFS_INODEGC_SHRINKER_BATCH	((XFS_INODEGC_SHRINKER_COUNT / 2) + 1)

static unsigned long
xfs_inodegc_shrinker_count(
	struct shrinker		*shrink,
	struct shrink_control	*sc)
{
	struct xfs_mount	*mp = container_of(shrink, struct xfs_mount,
						   m_inodegc_shrinker);
	struct xfs_inodegc	*gc;
	int			cpu;

	if (!xfs_is_inodegc_enabled(mp))
		return 0;

	for_each_cpu(cpu, &mp->m_inodegc_cpumask) {
		gc = per_cpu_ptr(mp->m_inodegc, cpu);
		if (!llist_empty(&gc->list))
			return XFS_INODEGC_SHRINKER_COUNT;
	}

	return 0;
}

static unsigned long
xfs_inodegc_shrinker_scan(
	struct shrinker		*shrink,
	struct shrink_control	*sc)
{
	struct xfs_mount	*mp = container_of(shrink, struct xfs_mount,
						   m_inodegc_shrinker);
	struct xfs_inodegc	*gc;
	int			cpu;
	bool			no_items = true;

	if (!xfs_is_inodegc_enabled(mp))
		return SHRINK_STOP;

	trace_xfs_inodegc_shrinker_scan(mp, sc, __return_address);

	for_each_cpu(cpu, &mp->m_inodegc_cpumask) {
		gc = per_cpu_ptr(mp->m_inodegc, cpu);
		if (!llist_empty(&gc->list)) {
			unsigned int	h = READ_ONCE(gc->shrinker_hits);

			WRITE_ONCE(gc->shrinker_hits, h + 1);
			mod_delayed_work_on(cpu, mp->m_inodegc_wq, &gc->work, 0);
			no_items = false;
		}
	}

	 
	if (no_items)
		return LONG_MAX;

	return SHRINK_STOP;
}

 
int
xfs_inodegc_register_shrinker(
	struct xfs_mount	*mp)
{
	struct shrinker		*shrink = &mp->m_inodegc_shrinker;

	shrink->count_objects = xfs_inodegc_shrinker_count;
	shrink->scan_objects = xfs_inodegc_shrinker_scan;
	shrink->seeks = 0;
	shrink->flags = SHRINKER_NONSLAB;
	shrink->batch = XFS_INODEGC_SHRINKER_BATCH;

	return register_shrinker(shrink, "xfs-inodegc:%s", mp->m_super->s_id);
}
