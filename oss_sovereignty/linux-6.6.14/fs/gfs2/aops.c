
 

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/buffer_head.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/mpage.h>
#include <linux/fs.h>
#include <linux/writeback.h>
#include <linux/swap.h>
#include <linux/gfs2_ondisk.h>
#include <linux/backing-dev.h>
#include <linux/uio.h>
#include <trace/events/writeback.h>
#include <linux/sched/signal.h>

#include "gfs2.h"
#include "incore.h"
#include "bmap.h"
#include "glock.h"
#include "inode.h"
#include "log.h"
#include "meta_io.h"
#include "quota.h"
#include "trans.h"
#include "rgrp.h"
#include "super.h"
#include "util.h"
#include "glops.h"
#include "aops.h"


void gfs2_trans_add_databufs(struct gfs2_inode *ip, struct folio *folio,
			     size_t from, size_t len)
{
	struct buffer_head *head = folio_buffers(folio);
	unsigned int bsize = head->b_size;
	struct buffer_head *bh;
	size_t to = from + len;
	size_t start, end;

	for (bh = head, start = 0; bh != head || !start;
	     bh = bh->b_this_page, start = end) {
		end = start + bsize;
		if (end <= from)
			continue;
		if (start >= to)
			break;
		set_buffer_uptodate(bh);
		gfs2_trans_add_data(ip->i_gl, bh);
	}
}

 

static int gfs2_get_block_noalloc(struct inode *inode, sector_t lblock,
				  struct buffer_head *bh_result, int create)
{
	int error;

	error = gfs2_block_map(inode, lblock, bh_result, 0);
	if (error)
		return error;
	if (!buffer_mapped(bh_result))
		return -ENODATA;
	return 0;
}

 
static int gfs2_write_jdata_folio(struct folio *folio,
				 struct writeback_control *wbc)
{
	struct inode * const inode = folio->mapping->host;
	loff_t i_size = i_size_read(inode);

	 
	if (folio_pos(folio) < i_size &&
	    i_size < folio_pos(folio) + folio_size(folio))
		folio_zero_segment(folio, offset_in_folio(folio, i_size),
				folio_size(folio));

	return __block_write_full_folio(inode, folio, gfs2_get_block_noalloc,
			wbc, end_buffer_async_write);
}

 
static int __gfs2_jdata_write_folio(struct folio *folio,
		struct writeback_control *wbc)
{
	struct inode *inode = folio->mapping->host;
	struct gfs2_inode *ip = GFS2_I(inode);

	if (folio_test_checked(folio)) {
		folio_clear_checked(folio);
		if (!folio_buffers(folio)) {
			folio_create_empty_buffers(folio,
					inode->i_sb->s_blocksize,
					BIT(BH_Dirty)|BIT(BH_Uptodate));
		}
		gfs2_trans_add_databufs(ip, folio, 0, folio_size(folio));
	}
	return gfs2_write_jdata_folio(folio, wbc);
}

 

static int gfs2_jdata_writepage(struct page *page, struct writeback_control *wbc)
{
	struct folio *folio = page_folio(page);
	struct inode *inode = page->mapping->host;
	struct gfs2_inode *ip = GFS2_I(inode);
	struct gfs2_sbd *sdp = GFS2_SB(inode);

	if (gfs2_assert_withdraw(sdp, gfs2_glock_is_held_excl(ip->i_gl)))
		goto out;
	if (folio_test_checked(folio) || current->journal_info)
		goto out_ignore;
	return __gfs2_jdata_write_folio(folio, wbc);

out_ignore:
	folio_redirty_for_writepage(wbc, folio);
out:
	folio_unlock(folio);
	return 0;
}

 
static int gfs2_writepages(struct address_space *mapping,
			   struct writeback_control *wbc)
{
	struct gfs2_sbd *sdp = gfs2_mapping2sbd(mapping);
	struct iomap_writepage_ctx wpc = { };
	int ret;

	 
	ret = iomap_writepages(mapping, wbc, &wpc, &gfs2_writeback_ops);
	if (ret == 0 && wbc->nr_to_write > 0)
		set_bit(SDF_FORCE_AIL_FLUSH, &sdp->sd_flags);
	return ret;
}

 

static int gfs2_write_jdata_batch(struct address_space *mapping,
				    struct writeback_control *wbc,
				    struct folio_batch *fbatch,
				    pgoff_t *done_index)
{
	struct inode *inode = mapping->host;
	struct gfs2_sbd *sdp = GFS2_SB(inode);
	unsigned nrblocks;
	int i;
	int ret;
	int nr_pages = 0;
	int nr_folios = folio_batch_count(fbatch);

	for (i = 0; i < nr_folios; i++)
		nr_pages += folio_nr_pages(fbatch->folios[i]);
	nrblocks = nr_pages * (PAGE_SIZE >> inode->i_blkbits);

	ret = gfs2_trans_begin(sdp, nrblocks, nrblocks);
	if (ret < 0)
		return ret;

	for (i = 0; i < nr_folios; i++) {
		struct folio *folio = fbatch->folios[i];

		*done_index = folio->index;

		folio_lock(folio);

		if (unlikely(folio->mapping != mapping)) {
continue_unlock:
			folio_unlock(folio);
			continue;
		}

		if (!folio_test_dirty(folio)) {
			 
			goto continue_unlock;
		}

		if (folio_test_writeback(folio)) {
			if (wbc->sync_mode != WB_SYNC_NONE)
				folio_wait_writeback(folio);
			else
				goto continue_unlock;
		}

		BUG_ON(folio_test_writeback(folio));
		if (!folio_clear_dirty_for_io(folio))
			goto continue_unlock;

		trace_wbc_writepage(wbc, inode_to_bdi(inode));

		ret = __gfs2_jdata_write_folio(folio, wbc);
		if (unlikely(ret)) {
			if (ret == AOP_WRITEPAGE_ACTIVATE) {
				folio_unlock(folio);
				ret = 0;
			} else {

				 
				*done_index = folio_next_index(folio);
				ret = 1;
				break;
			}
		}

		 
		if (--wbc->nr_to_write <= 0 && wbc->sync_mode == WB_SYNC_NONE) {
			ret = 1;
			break;
		}

	}
	gfs2_trans_end(sdp);
	return ret;
}

 

static int gfs2_write_cache_jdata(struct address_space *mapping,
				  struct writeback_control *wbc)
{
	int ret = 0;
	int done = 0;
	struct folio_batch fbatch;
	int nr_folios;
	pgoff_t writeback_index;
	pgoff_t index;
	pgoff_t end;
	pgoff_t done_index;
	int cycled;
	int range_whole = 0;
	xa_mark_t tag;

	folio_batch_init(&fbatch);
	if (wbc->range_cyclic) {
		writeback_index = mapping->writeback_index;  
		index = writeback_index;
		if (index == 0)
			cycled = 1;
		else
			cycled = 0;
		end = -1;
	} else {
		index = wbc->range_start >> PAGE_SHIFT;
		end = wbc->range_end >> PAGE_SHIFT;
		if (wbc->range_start == 0 && wbc->range_end == LLONG_MAX)
			range_whole = 1;
		cycled = 1;  
	}
	if (wbc->sync_mode == WB_SYNC_ALL || wbc->tagged_writepages)
		tag = PAGECACHE_TAG_TOWRITE;
	else
		tag = PAGECACHE_TAG_DIRTY;

retry:
	if (wbc->sync_mode == WB_SYNC_ALL || wbc->tagged_writepages)
		tag_pages_for_writeback(mapping, index, end);
	done_index = index;
	while (!done && (index <= end)) {
		nr_folios = filemap_get_folios_tag(mapping, &index, end,
				tag, &fbatch);
		if (nr_folios == 0)
			break;

		ret = gfs2_write_jdata_batch(mapping, wbc, &fbatch,
				&done_index);
		if (ret)
			done = 1;
		if (ret > 0)
			ret = 0;
		folio_batch_release(&fbatch);
		cond_resched();
	}

	if (!cycled && !done) {
		 
		cycled = 1;
		index = 0;
		end = writeback_index - 1;
		goto retry;
	}

	if (wbc->range_cyclic || (range_whole && wbc->nr_to_write > 0))
		mapping->writeback_index = done_index;

	return ret;
}


 

static int gfs2_jdata_writepages(struct address_space *mapping,
				 struct writeback_control *wbc)
{
	struct gfs2_inode *ip = GFS2_I(mapping->host);
	struct gfs2_sbd *sdp = GFS2_SB(mapping->host);
	int ret;

	ret = gfs2_write_cache_jdata(mapping, wbc);
	if (ret == 0 && wbc->sync_mode == WB_SYNC_ALL) {
		gfs2_log_flush(sdp, ip->i_gl, GFS2_LOG_HEAD_FLUSH_NORMAL |
			       GFS2_LFC_JDATA_WPAGES);
		ret = gfs2_write_cache_jdata(mapping, wbc);
	}
	return ret;
}

 
static int stuffed_readpage(struct gfs2_inode *ip, struct page *page)
{
	struct buffer_head *dibh;
	u64 dsize = i_size_read(&ip->i_inode);
	void *kaddr;
	int error;

	 
	if (unlikely(page->index)) {
		zero_user(page, 0, PAGE_SIZE);
		SetPageUptodate(page);
		return 0;
	}

	error = gfs2_meta_inode_buffer(ip, &dibh);
	if (error)
		return error;

	kaddr = kmap_local_page(page);
	memcpy(kaddr, dibh->b_data + sizeof(struct gfs2_dinode), dsize);
	memset(kaddr + dsize, 0, PAGE_SIZE - dsize);
	kunmap_local(kaddr);
	flush_dcache_page(page);
	brelse(dibh);
	SetPageUptodate(page);

	return 0;
}

 
static int gfs2_read_folio(struct file *file, struct folio *folio)
{
	struct inode *inode = folio->mapping->host;
	struct gfs2_inode *ip = GFS2_I(inode);
	struct gfs2_sbd *sdp = GFS2_SB(inode);
	int error;

	if (!gfs2_is_jdata(ip) ||
	    (i_blocksize(inode) == PAGE_SIZE && !folio_buffers(folio))) {
		error = iomap_read_folio(folio, &gfs2_iomap_ops);
	} else if (gfs2_is_stuffed(ip)) {
		error = stuffed_readpage(ip, &folio->page);
		folio_unlock(folio);
	} else {
		error = mpage_read_folio(folio, gfs2_block_map);
	}

	if (unlikely(gfs2_withdrawn(sdp)))
		return -EIO;

	return error;
}

 

int gfs2_internal_read(struct gfs2_inode *ip, char *buf, loff_t *pos,
                       unsigned size)
{
	struct address_space *mapping = ip->i_inode.i_mapping;
	unsigned long index = *pos >> PAGE_SHIFT;
	unsigned offset = *pos & (PAGE_SIZE - 1);
	unsigned copied = 0;
	unsigned amt;
	struct page *page;

	do {
		page = read_cache_page(mapping, index, gfs2_read_folio, NULL);
		if (IS_ERR(page)) {
			if (PTR_ERR(page) == -EINTR)
				continue;
			return PTR_ERR(page);
		}
		amt = size - copied;
		if (offset + size > PAGE_SIZE)
			amt = PAGE_SIZE - offset;
		memcpy_from_page(buf + copied, page, offset, amt);
		put_page(page);
		copied += amt;
		index++;
		offset = 0;
	} while(copied < size);
	(*pos) += size;
	return size;
}

 

static void gfs2_readahead(struct readahead_control *rac)
{
	struct inode *inode = rac->mapping->host;
	struct gfs2_inode *ip = GFS2_I(inode);

	if (gfs2_is_stuffed(ip))
		;
	else if (gfs2_is_jdata(ip))
		mpage_readahead(rac, gfs2_block_map);
	else
		iomap_readahead(rac, &gfs2_iomap_ops);
}

 
void adjust_fs_space(struct inode *inode)
{
	struct gfs2_sbd *sdp = GFS2_SB(inode);
	struct gfs2_inode *m_ip = GFS2_I(sdp->sd_statfs_inode);
	struct gfs2_statfs_change_host *m_sc = &sdp->sd_statfs_master;
	struct gfs2_statfs_change_host *l_sc = &sdp->sd_statfs_local;
	struct buffer_head *m_bh;
	u64 fs_total, new_free;

	if (gfs2_trans_begin(sdp, 2 * RES_STATFS, 0) != 0)
		return;

	 
	fs_total = gfs2_ri_total(sdp);
	if (gfs2_meta_inode_buffer(m_ip, &m_bh) != 0)
		goto out;

	spin_lock(&sdp->sd_statfs_spin);
	gfs2_statfs_change_in(m_sc, m_bh->b_data +
			      sizeof(struct gfs2_dinode));
	if (fs_total > (m_sc->sc_total + l_sc->sc_total))
		new_free = fs_total - (m_sc->sc_total + l_sc->sc_total);
	else
		new_free = 0;
	spin_unlock(&sdp->sd_statfs_spin);
	fs_warn(sdp, "File system extended by %llu blocks.\n",
		(unsigned long long)new_free);
	gfs2_statfs_change(sdp, new_free, new_free, 0);

	update_statfs(sdp, m_bh);
	brelse(m_bh);
out:
	sdp->sd_rindex_uptodate = 0;
	gfs2_trans_end(sdp);
}

static bool jdata_dirty_folio(struct address_space *mapping,
		struct folio *folio)
{
	if (current->journal_info)
		folio_set_checked(folio);
	return block_dirty_folio(mapping, folio);
}

 

static sector_t gfs2_bmap(struct address_space *mapping, sector_t lblock)
{
	struct gfs2_inode *ip = GFS2_I(mapping->host);
	struct gfs2_holder i_gh;
	sector_t dblock = 0;
	int error;

	error = gfs2_glock_nq_init(ip->i_gl, LM_ST_SHARED, LM_FLAG_ANY, &i_gh);
	if (error)
		return 0;

	if (!gfs2_is_stuffed(ip))
		dblock = iomap_bmap(mapping, lblock, &gfs2_iomap_ops);

	gfs2_glock_dq_uninit(&i_gh);

	return dblock;
}

static void gfs2_discard(struct gfs2_sbd *sdp, struct buffer_head *bh)
{
	struct gfs2_bufdata *bd;

	lock_buffer(bh);
	gfs2_log_lock(sdp);
	clear_buffer_dirty(bh);
	bd = bh->b_private;
	if (bd) {
		if (!list_empty(&bd->bd_list) && !buffer_pinned(bh))
			list_del_init(&bd->bd_list);
		else {
			spin_lock(&sdp->sd_ail_lock);
			gfs2_remove_from_journal(bh, REMOVE_JDATA);
			spin_unlock(&sdp->sd_ail_lock);
		}
	}
	bh->b_bdev = NULL;
	clear_buffer_mapped(bh);
	clear_buffer_req(bh);
	clear_buffer_new(bh);
	gfs2_log_unlock(sdp);
	unlock_buffer(bh);
}

static void gfs2_invalidate_folio(struct folio *folio, size_t offset,
				size_t length)
{
	struct gfs2_sbd *sdp = GFS2_SB(folio->mapping->host);
	size_t stop = offset + length;
	int partial_page = (offset || length < folio_size(folio));
	struct buffer_head *bh, *head;
	unsigned long pos = 0;

	BUG_ON(!folio_test_locked(folio));
	if (!partial_page)
		folio_clear_checked(folio);
	head = folio_buffers(folio);
	if (!head)
		goto out;

	bh = head;
	do {
		if (pos + bh->b_size > stop)
			return;

		if (offset <= pos)
			gfs2_discard(sdp, bh);
		pos += bh->b_size;
		bh = bh->b_this_page;
	} while (bh != head);
out:
	if (!partial_page)
		filemap_release_folio(folio, 0);
}

 

bool gfs2_release_folio(struct folio *folio, gfp_t gfp_mask)
{
	struct address_space *mapping = folio->mapping;
	struct gfs2_sbd *sdp = gfs2_mapping2sbd(mapping);
	struct buffer_head *bh, *head;
	struct gfs2_bufdata *bd;

	head = folio_buffers(folio);
	if (!head)
		return false;

	 

	gfs2_log_lock(sdp);
	bh = head;
	do {
		if (atomic_read(&bh->b_count))
			goto cannot_release;
		bd = bh->b_private;
		if (bd && bd->bd_tr)
			goto cannot_release;
		if (buffer_dirty(bh) || WARN_ON(buffer_pinned(bh)))
			goto cannot_release;
		bh = bh->b_this_page;
	} while (bh != head);

	bh = head;
	do {
		bd = bh->b_private;
		if (bd) {
			gfs2_assert_warn(sdp, bd->bd_bh == bh);
			bd->bd_bh = NULL;
			bh->b_private = NULL;
			 
			if (!bd->bd_blkno && !list_empty(&bd->bd_list))
				list_del_init(&bd->bd_list);
			if (list_empty(&bd->bd_list))
				kmem_cache_free(gfs2_bufdata_cachep, bd);
		}

		bh = bh->b_this_page;
	} while (bh != head);
	gfs2_log_unlock(sdp);

	return try_to_free_buffers(folio);

cannot_release:
	gfs2_log_unlock(sdp);
	return false;
}

static const struct address_space_operations gfs2_aops = {
	.writepages = gfs2_writepages,
	.read_folio = gfs2_read_folio,
	.readahead = gfs2_readahead,
	.dirty_folio = iomap_dirty_folio,
	.release_folio = iomap_release_folio,
	.invalidate_folio = iomap_invalidate_folio,
	.bmap = gfs2_bmap,
	.migrate_folio = filemap_migrate_folio,
	.is_partially_uptodate = iomap_is_partially_uptodate,
	.error_remove_page = generic_error_remove_page,
};

static const struct address_space_operations gfs2_jdata_aops = {
	.writepage = gfs2_jdata_writepage,
	.writepages = gfs2_jdata_writepages,
	.read_folio = gfs2_read_folio,
	.readahead = gfs2_readahead,
	.dirty_folio = jdata_dirty_folio,
	.bmap = gfs2_bmap,
	.invalidate_folio = gfs2_invalidate_folio,
	.release_folio = gfs2_release_folio,
	.is_partially_uptodate = block_is_partially_uptodate,
	.error_remove_page = generic_error_remove_page,
};

void gfs2_set_aops(struct inode *inode)
{
	if (gfs2_is_jdata(GFS2_I(inode)))
		inode->i_mapping->a_ops = &gfs2_jdata_aops;
	else
		inode->i_mapping->a_ops = &gfs2_aops;
}
