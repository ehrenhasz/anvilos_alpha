
 
#include "xfs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_iomap.h"
#include "xfs_trace.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_reflink.h"
#include "xfs_errortag.h"
#include "xfs_error.h"

struct xfs_writepage_ctx {
	struct iomap_writepage_ctx ctx;
	unsigned int		data_seq;
	unsigned int		cow_seq;
};

static inline struct xfs_writepage_ctx *
XFS_WPC(struct iomap_writepage_ctx *ctx)
{
	return container_of(ctx, struct xfs_writepage_ctx, ctx);
}

 
static inline bool xfs_ioend_is_append(struct iomap_ioend *ioend)
{
	return ioend->io_offset + ioend->io_size >
		XFS_I(ioend->io_inode)->i_disk_size;
}

 
int
xfs_setfilesize(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	size_t			size)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	xfs_fsize_t		isize;
	int			error;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_fsyncts, 0, 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	isize = xfs_new_eof(ip, offset + size);
	if (!isize) {
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		xfs_trans_cancel(tp);
		return 0;
	}

	trace_xfs_setfilesize(ip, offset, size);

	ip->i_disk_size = isize;
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	return xfs_trans_commit(tp);
}

 
STATIC void
xfs_end_ioend(
	struct iomap_ioend	*ioend)
{
	struct xfs_inode	*ip = XFS_I(ioend->io_inode);
	struct xfs_mount	*mp = ip->i_mount;
	xfs_off_t		offset = ioend->io_offset;
	size_t			size = ioend->io_size;
	unsigned int		nofs_flag;
	int			error;

	 
	nofs_flag = memalloc_nofs_save();

	 
	if (xfs_is_shutdown(mp)) {
		error = -EIO;
		goto done;
	}

	 
	error = blk_status_to_errno(ioend->io_bio->bi_status);
	if (unlikely(error)) {
		if (ioend->io_flags & IOMAP_F_SHARED) {
			xfs_reflink_cancel_cow_range(ip, offset, size, true);
			xfs_bmap_punch_delalloc_range(ip, offset,
					offset + size);
		}
		goto done;
	}

	 
	if (ioend->io_flags & IOMAP_F_SHARED)
		error = xfs_reflink_end_cow(ip, offset, size);
	else if (ioend->io_type == IOMAP_UNWRITTEN)
		error = xfs_iomap_write_unwritten(ip, offset, size, false);

	if (!error && xfs_ioend_is_append(ioend))
		error = xfs_setfilesize(ip, ioend->io_offset, ioend->io_size);
done:
	iomap_finish_ioends(ioend, error);
	memalloc_nofs_restore(nofs_flag);
}

 
void
xfs_end_io(
	struct work_struct	*work)
{
	struct xfs_inode	*ip =
		container_of(work, struct xfs_inode, i_ioend_work);
	struct iomap_ioend	*ioend;
	struct list_head	tmp;
	unsigned long		flags;

	spin_lock_irqsave(&ip->i_ioend_lock, flags);
	list_replace_init(&ip->i_ioend_list, &tmp);
	spin_unlock_irqrestore(&ip->i_ioend_lock, flags);

	iomap_sort_ioends(&tmp);
	while ((ioend = list_first_entry_or_null(&tmp, struct iomap_ioend,
			io_list))) {
		list_del_init(&ioend->io_list);
		iomap_ioend_try_merge(ioend, &tmp);
		xfs_end_ioend(ioend);
		cond_resched();
	}
}

STATIC void
xfs_end_bio(
	struct bio		*bio)
{
	struct iomap_ioend	*ioend = bio->bi_private;
	struct xfs_inode	*ip = XFS_I(ioend->io_inode);
	unsigned long		flags;

	spin_lock_irqsave(&ip->i_ioend_lock, flags);
	if (list_empty(&ip->i_ioend_list))
		WARN_ON_ONCE(!queue_work(ip->i_mount->m_unwritten_workqueue,
					 &ip->i_ioend_work));
	list_add_tail(&ioend->io_list, &ip->i_ioend_list);
	spin_unlock_irqrestore(&ip->i_ioend_lock, flags);
}

 
static bool
xfs_imap_valid(
	struct iomap_writepage_ctx	*wpc,
	struct xfs_inode		*ip,
	loff_t				offset)
{
	if (offset < wpc->iomap.offset ||
	    offset >= wpc->iomap.offset + wpc->iomap.length)
		return false;
	 
	if (wpc->iomap.flags & IOMAP_F_SHARED)
		return true;

	 
	if (XFS_WPC(wpc)->data_seq != READ_ONCE(ip->i_df.if_seq)) {
		trace_xfs_wb_data_iomap_invalid(ip, &wpc->iomap,
				XFS_WPC(wpc)->data_seq, XFS_DATA_FORK);
		return false;
	}
	if (xfs_inode_has_cow_data(ip) &&
	    XFS_WPC(wpc)->cow_seq != READ_ONCE(ip->i_cowfp->if_seq)) {
		trace_xfs_wb_cow_iomap_invalid(ip, &wpc->iomap,
				XFS_WPC(wpc)->cow_seq, XFS_COW_FORK);
		return false;
	}
	return true;
}

 
static int
xfs_convert_blocks(
	struct iomap_writepage_ctx *wpc,
	struct xfs_inode	*ip,
	int			whichfork,
	loff_t			offset)
{
	int			error;
	unsigned		*seq;

	if (whichfork == XFS_COW_FORK)
		seq = &XFS_WPC(wpc)->cow_seq;
	else
		seq = &XFS_WPC(wpc)->data_seq;

	 
	do {
		error = xfs_bmapi_convert_delalloc(ip, whichfork, offset,
				&wpc->iomap, seq);
		if (error)
			return error;
	} while (wpc->iomap.offset + wpc->iomap.length <= offset);

	return 0;
}

static int
xfs_map_blocks(
	struct iomap_writepage_ctx *wpc,
	struct inode		*inode,
	loff_t			offset)
{
	struct xfs_inode	*ip = XFS_I(inode);
	struct xfs_mount	*mp = ip->i_mount;
	ssize_t			count = i_blocksize(inode);
	xfs_fileoff_t		offset_fsb = XFS_B_TO_FSBT(mp, offset);
	xfs_fileoff_t		end_fsb = XFS_B_TO_FSB(mp, offset + count);
	xfs_fileoff_t		cow_fsb;
	int			whichfork;
	struct xfs_bmbt_irec	imap;
	struct xfs_iext_cursor	icur;
	int			retries = 0;
	int			error = 0;

	if (xfs_is_shutdown(mp))
		return -EIO;

	XFS_ERRORTAG_DELAY(mp, XFS_ERRTAG_WB_DELAY_MS);

	 
	if (xfs_imap_valid(wpc, ip, offset))
		return 0;

	 
retry:
	cow_fsb = NULLFILEOFF;
	whichfork = XFS_DATA_FORK;
	xfs_ilock(ip, XFS_ILOCK_SHARED);
	ASSERT(!xfs_need_iread_extents(&ip->i_df));

	 
	if (xfs_inode_has_cow_data(ip) &&
	    xfs_iext_lookup_extent(ip, ip->i_cowfp, offset_fsb, &icur, &imap))
		cow_fsb = imap.br_startoff;
	if (cow_fsb != NULLFILEOFF && cow_fsb <= offset_fsb) {
		XFS_WPC(wpc)->cow_seq = READ_ONCE(ip->i_cowfp->if_seq);
		xfs_iunlock(ip, XFS_ILOCK_SHARED);

		whichfork = XFS_COW_FORK;
		goto allocate_blocks;
	}

	 
	if (xfs_imap_valid(wpc, ip, offset)) {
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
		return 0;
	}

	 
	if (!xfs_iext_lookup_extent(ip, &ip->i_df, offset_fsb, &icur, &imap))
		imap.br_startoff = end_fsb;	 
	XFS_WPC(wpc)->data_seq = READ_ONCE(ip->i_df.if_seq);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);

	 
	if (imap.br_startoff > offset_fsb) {
		imap.br_blockcount = imap.br_startoff - offset_fsb;
		imap.br_startoff = offset_fsb;
		imap.br_startblock = HOLESTARTBLOCK;
		imap.br_state = XFS_EXT_NORM;
	}

	 
	if (cow_fsb != NULLFILEOFF &&
	    cow_fsb < imap.br_startoff + imap.br_blockcount)
		imap.br_blockcount = cow_fsb - imap.br_startoff;

	 
	if (imap.br_startblock != HOLESTARTBLOCK &&
	    isnullstartblock(imap.br_startblock))
		goto allocate_blocks;

	xfs_bmbt_to_iomap(ip, &wpc->iomap, &imap, 0, 0, XFS_WPC(wpc)->data_seq);
	trace_xfs_map_blocks_found(ip, offset, count, whichfork, &imap);
	return 0;
allocate_blocks:
	error = xfs_convert_blocks(wpc, ip, whichfork, offset);
	if (error) {
		 
		if (error == -EAGAIN && whichfork == XFS_COW_FORK && !retries++)
			goto retry;
		ASSERT(error != -EAGAIN);
		return error;
	}

	 
	if (whichfork != XFS_COW_FORK && cow_fsb != NULLFILEOFF) {
		loff_t		cow_offset = XFS_FSB_TO_B(mp, cow_fsb);

		if (cow_offset < wpc->iomap.offset + wpc->iomap.length)
			wpc->iomap.length = cow_offset - wpc->iomap.offset;
	}

	ASSERT(wpc->iomap.offset <= offset);
	ASSERT(wpc->iomap.offset + wpc->iomap.length > offset);
	trace_xfs_map_blocks_alloc(ip, offset, count, whichfork, &imap);
	return 0;
}

static int
xfs_prepare_ioend(
	struct iomap_ioend	*ioend,
	int			status)
{
	unsigned int		nofs_flag;

	 
	nofs_flag = memalloc_nofs_save();

	 
	if (!status && (ioend->io_flags & IOMAP_F_SHARED)) {
		status = xfs_reflink_convert_cow(XFS_I(ioend->io_inode),
				ioend->io_offset, ioend->io_size);
	}

	memalloc_nofs_restore(nofs_flag);

	 
	if (xfs_ioend_is_append(ioend) || ioend->io_type == IOMAP_UNWRITTEN ||
	    (ioend->io_flags & IOMAP_F_SHARED))
		ioend->io_bio->bi_end_io = xfs_end_bio;
	return status;
}

 
static void
xfs_discard_folio(
	struct folio		*folio,
	loff_t			pos)
{
	struct xfs_inode	*ip = XFS_I(folio->mapping->host);
	struct xfs_mount	*mp = ip->i_mount;
	int			error;

	if (xfs_is_shutdown(mp))
		return;

	xfs_alert_ratelimited(mp,
		"page discard on page "PTR_FMT", inode 0x%llx, pos %llu.",
			folio, ip->i_ino, pos);

	 
	error = xfs_bmap_punch_delalloc_range(ip, pos,
				folio_pos(folio) + folio_size(folio));

	if (error && !xfs_is_shutdown(mp))
		xfs_alert(mp, "page discard unable to remove delalloc mapping.");
}

static const struct iomap_writeback_ops xfs_writeback_ops = {
	.map_blocks		= xfs_map_blocks,
	.prepare_ioend		= xfs_prepare_ioend,
	.discard_folio		= xfs_discard_folio,
};

STATIC int
xfs_vm_writepages(
	struct address_space	*mapping,
	struct writeback_control *wbc)
{
	struct xfs_writepage_ctx wpc = { };

	 
	if (WARN_ON_ONCE(current->journal_info))
		return 0;

	xfs_iflags_clear(XFS_I(mapping->host), XFS_ITRUNCATED);
	return iomap_writepages(mapping, wbc, &wpc.ctx, &xfs_writeback_ops);
}

STATIC int
xfs_dax_writepages(
	struct address_space	*mapping,
	struct writeback_control *wbc)
{
	struct xfs_inode	*ip = XFS_I(mapping->host);

	xfs_iflags_clear(ip, XFS_ITRUNCATED);
	return dax_writeback_mapping_range(mapping,
			xfs_inode_buftarg(ip)->bt_daxdev, wbc);
}

STATIC sector_t
xfs_vm_bmap(
	struct address_space	*mapping,
	sector_t		block)
{
	struct xfs_inode	*ip = XFS_I(mapping->host);

	trace_xfs_vm_bmap(ip);

	 
	if (xfs_is_cow_inode(ip) || XFS_IS_REALTIME_INODE(ip))
		return 0;
	return iomap_bmap(mapping, block, &xfs_read_iomap_ops);
}

STATIC int
xfs_vm_read_folio(
	struct file		*unused,
	struct folio		*folio)
{
	return iomap_read_folio(folio, &xfs_read_iomap_ops);
}

STATIC void
xfs_vm_readahead(
	struct readahead_control	*rac)
{
	iomap_readahead(rac, &xfs_read_iomap_ops);
}

static int
xfs_iomap_swapfile_activate(
	struct swap_info_struct		*sis,
	struct file			*swap_file,
	sector_t			*span)
{
	sis->bdev = xfs_inode_buftarg(XFS_I(file_inode(swap_file)))->bt_bdev;
	return iomap_swapfile_activate(sis, swap_file, span,
			&xfs_read_iomap_ops);
}

const struct address_space_operations xfs_address_space_operations = {
	.read_folio		= xfs_vm_read_folio,
	.readahead		= xfs_vm_readahead,
	.writepages		= xfs_vm_writepages,
	.dirty_folio		= iomap_dirty_folio,
	.release_folio		= iomap_release_folio,
	.invalidate_folio	= iomap_invalidate_folio,
	.bmap			= xfs_vm_bmap,
	.migrate_folio		= filemap_migrate_folio,
	.is_partially_uptodate  = iomap_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
	.swap_activate		= xfs_iomap_swapfile_activate,
};

const struct address_space_operations xfs_dax_aops = {
	.writepages		= xfs_dax_writepages,
	.dirty_folio		= noop_dirty_folio,
	.swap_activate		= xfs_iomap_swapfile_activate,
};
