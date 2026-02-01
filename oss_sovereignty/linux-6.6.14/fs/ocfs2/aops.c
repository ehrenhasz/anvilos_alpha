
 

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <asm/byteorder.h>
#include <linux/swap.h>
#include <linux/mpage.h>
#include <linux/quotaops.h>
#include <linux/blkdev.h>
#include <linux/uio.h>
#include <linux/mm.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "alloc.h"
#include "aops.h"
#include "dlmglue.h"
#include "extent_map.h"
#include "file.h"
#include "inode.h"
#include "journal.h"
#include "suballoc.h"
#include "super.h"
#include "symlink.h"
#include "refcounttree.h"
#include "ocfs2_trace.h"

#include "buffer_head_io.h"
#include "dir.h"
#include "namei.h"
#include "sysfile.h"

static int ocfs2_symlink_get_block(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	int err = -EIO;
	int status;
	struct ocfs2_dinode *fe = NULL;
	struct buffer_head *bh = NULL;
	struct buffer_head *buffer_cache_bh = NULL;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	void *kaddr;

	trace_ocfs2_symlink_get_block(
			(unsigned long long)OCFS2_I(inode)->ip_blkno,
			(unsigned long long)iblock, bh_result, create);

	BUG_ON(ocfs2_inode_is_fast_symlink(inode));

	if ((iblock << inode->i_sb->s_blocksize_bits) > PATH_MAX + 1) {
		mlog(ML_ERROR, "block offset > PATH_MAX: %llu",
		     (unsigned long long)iblock);
		goto bail;
	}

	status = ocfs2_read_inode_block(inode, &bh);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}
	fe = (struct ocfs2_dinode *) bh->b_data;

	if ((u64)iblock >= ocfs2_clusters_to_blocks(inode->i_sb,
						    le32_to_cpu(fe->i_clusters))) {
		err = -ENOMEM;
		mlog(ML_ERROR, "block offset is outside the allocated size: "
		     "%llu\n", (unsigned long long)iblock);
		goto bail;
	}

	 
	if (!buffer_uptodate(bh_result) && ocfs2_inode_is_new(inode)) {
		u64 blkno = le64_to_cpu(fe->id2.i_list.l_recs[0].e_blkno) +
			    iblock;
		buffer_cache_bh = sb_getblk(osb->sb, blkno);
		if (!buffer_cache_bh) {
			err = -ENOMEM;
			mlog(ML_ERROR, "couldn't getblock for symlink!\n");
			goto bail;
		}

		 
		if (buffer_jbd(buffer_cache_bh)
		    && ocfs2_inode_is_new(inode)) {
			kaddr = kmap_atomic(bh_result->b_page);
			if (!kaddr) {
				mlog(ML_ERROR, "couldn't kmap!\n");
				goto bail;
			}
			memcpy(kaddr + (bh_result->b_size * iblock),
			       buffer_cache_bh->b_data,
			       bh_result->b_size);
			kunmap_atomic(kaddr);
			set_buffer_uptodate(bh_result);
		}
		brelse(buffer_cache_bh);
	}

	map_bh(bh_result, inode->i_sb,
	       le64_to_cpu(fe->id2.i_list.l_recs[0].e_blkno) + iblock);

	err = 0;

bail:
	brelse(bh);

	return err;
}

static int ocfs2_lock_get_block(struct inode *inode, sector_t iblock,
		    struct buffer_head *bh_result, int create)
{
	int ret = 0;
	struct ocfs2_inode_info *oi = OCFS2_I(inode);

	down_read(&oi->ip_alloc_sem);
	ret = ocfs2_get_block(inode, iblock, bh_result, create);
	up_read(&oi->ip_alloc_sem);

	return ret;
}

int ocfs2_get_block(struct inode *inode, sector_t iblock,
		    struct buffer_head *bh_result, int create)
{
	int err = 0;
	unsigned int ext_flags;
	u64 max_blocks = bh_result->b_size >> inode->i_blkbits;
	u64 p_blkno, count, past_eof;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);

	trace_ocfs2_get_block((unsigned long long)OCFS2_I(inode)->ip_blkno,
			      (unsigned long long)iblock, bh_result, create);

	if (OCFS2_I(inode)->ip_flags & OCFS2_INODE_SYSTEM_FILE)
		mlog(ML_NOTICE, "get_block on system inode 0x%p (%lu)\n",
		     inode, inode->i_ino);

	if (S_ISLNK(inode->i_mode)) {
		 
		err = ocfs2_symlink_get_block(inode, iblock, bh_result, create);
		goto bail;
	}

	err = ocfs2_extent_map_get_blocks(inode, iblock, &p_blkno, &count,
					  &ext_flags);
	if (err) {
		mlog(ML_ERROR, "Error %d from get_blocks(0x%p, %llu, 1, "
		     "%llu, NULL)\n", err, inode, (unsigned long long)iblock,
		     (unsigned long long)p_blkno);
		goto bail;
	}

	if (max_blocks < count)
		count = max_blocks;

	 
	if (create && p_blkno == 0 && ocfs2_sparse_alloc(osb)) {
		clear_buffer_dirty(bh_result);
		clear_buffer_uptodate(bh_result);
		goto bail;
	}

	 
	if (p_blkno && !(ext_flags & OCFS2_EXT_UNWRITTEN))
		map_bh(bh_result, inode->i_sb, p_blkno);

	bh_result->b_size = count << inode->i_blkbits;

	if (!ocfs2_sparse_alloc(osb)) {
		if (p_blkno == 0) {
			err = -EIO;
			mlog(ML_ERROR,
			     "iblock = %llu p_blkno = %llu blkno=(%llu)\n",
			     (unsigned long long)iblock,
			     (unsigned long long)p_blkno,
			     (unsigned long long)OCFS2_I(inode)->ip_blkno);
			mlog(ML_ERROR, "Size %llu, clusters %u\n", (unsigned long long)i_size_read(inode), OCFS2_I(inode)->ip_clusters);
			dump_stack();
			goto bail;
		}
	}

	past_eof = ocfs2_blocks_for_bytes(inode->i_sb, i_size_read(inode));

	trace_ocfs2_get_block_end((unsigned long long)OCFS2_I(inode)->ip_blkno,
				  (unsigned long long)past_eof);
	if (create && (iblock >= past_eof))
		set_buffer_new(bh_result);

bail:
	if (err < 0)
		err = -EIO;

	return err;
}

int ocfs2_read_inline_data(struct inode *inode, struct page *page,
			   struct buffer_head *di_bh)
{
	void *kaddr;
	loff_t size;
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)di_bh->b_data;

	if (!(le16_to_cpu(di->i_dyn_features) & OCFS2_INLINE_DATA_FL)) {
		ocfs2_error(inode->i_sb, "Inode %llu lost inline data flag\n",
			    (unsigned long long)OCFS2_I(inode)->ip_blkno);
		return -EROFS;
	}

	size = i_size_read(inode);

	if (size > PAGE_SIZE ||
	    size > ocfs2_max_inline_data_with_xattr(inode->i_sb, di)) {
		ocfs2_error(inode->i_sb,
			    "Inode %llu has with inline data has bad size: %Lu\n",
			    (unsigned long long)OCFS2_I(inode)->ip_blkno,
			    (unsigned long long)size);
		return -EROFS;
	}

	kaddr = kmap_atomic(page);
	if (size)
		memcpy(kaddr, di->id2.i_data.id_data, size);
	 
	memset(kaddr + size, 0, PAGE_SIZE - size);
	flush_dcache_page(page);
	kunmap_atomic(kaddr);

	SetPageUptodate(page);

	return 0;
}

static int ocfs2_readpage_inline(struct inode *inode, struct page *page)
{
	int ret;
	struct buffer_head *di_bh = NULL;

	BUG_ON(!PageLocked(page));
	BUG_ON(!(OCFS2_I(inode)->ip_dyn_features & OCFS2_INLINE_DATA_FL));

	ret = ocfs2_read_inode_block(inode, &di_bh);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_read_inline_data(inode, page, di_bh);
out:
	unlock_page(page);

	brelse(di_bh);
	return ret;
}

static int ocfs2_read_folio(struct file *file, struct folio *folio)
{
	struct inode *inode = folio->mapping->host;
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	loff_t start = folio_pos(folio);
	int ret, unlock = 1;

	trace_ocfs2_readpage((unsigned long long)oi->ip_blkno, folio->index);

	ret = ocfs2_inode_lock_with_page(inode, NULL, 0, &folio->page);
	if (ret != 0) {
		if (ret == AOP_TRUNCATED_PAGE)
			unlock = 0;
		mlog_errno(ret);
		goto out;
	}

	if (down_read_trylock(&oi->ip_alloc_sem) == 0) {
		 
		ret = AOP_TRUNCATED_PAGE;
		folio_unlock(folio);
		unlock = 0;
		down_read(&oi->ip_alloc_sem);
		up_read(&oi->ip_alloc_sem);
		goto out_inode_unlock;
	}

	 
	if (start >= i_size_read(inode)) {
		folio_zero_segment(folio, 0, folio_size(folio));
		folio_mark_uptodate(folio);
		ret = 0;
		goto out_alloc;
	}

	if (oi->ip_dyn_features & OCFS2_INLINE_DATA_FL)
		ret = ocfs2_readpage_inline(inode, &folio->page);
	else
		ret = block_read_full_folio(folio, ocfs2_get_block);
	unlock = 0;

out_alloc:
	up_read(&oi->ip_alloc_sem);
out_inode_unlock:
	ocfs2_inode_unlock(inode, 0);
out:
	if (unlock)
		folio_unlock(folio);
	return ret;
}

 
static void ocfs2_readahead(struct readahead_control *rac)
{
	int ret;
	struct inode *inode = rac->mapping->host;
	struct ocfs2_inode_info *oi = OCFS2_I(inode);

	 
	ret = ocfs2_inode_lock_full(inode, NULL, 0, OCFS2_LOCK_NONBLOCK);
	if (ret)
		return;

	if (down_read_trylock(&oi->ip_alloc_sem) == 0)
		goto out_unlock;

	 
	if (oi->ip_dyn_features & OCFS2_INLINE_DATA_FL)
		goto out_up;

	 
	if (readahead_pos(rac) >= i_size_read(inode))
		goto out_up;

	mpage_readahead(rac, ocfs2_get_block);

out_up:
	up_read(&oi->ip_alloc_sem);
out_unlock:
	ocfs2_inode_unlock(inode, 0);
}

 
static int ocfs2_writepage(struct page *page, struct writeback_control *wbc)
{
	trace_ocfs2_writepage(
		(unsigned long long)OCFS2_I(page->mapping->host)->ip_blkno,
		page->index);

	return block_write_full_page(page, ocfs2_get_block, wbc);
}

 
int walk_page_buffers(	handle_t *handle,
			struct buffer_head *head,
			unsigned from,
			unsigned to,
			int *partial,
			int (*fn)(	handle_t *handle,
					struct buffer_head *bh))
{
	struct buffer_head *bh;
	unsigned block_start, block_end;
	unsigned blocksize = head->b_size;
	int err, ret = 0;
	struct buffer_head *next;

	for (	bh = head, block_start = 0;
		ret == 0 && (bh != head || !block_start);
	    	block_start = block_end, bh = next)
	{
		next = bh->b_this_page;
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (partial && !buffer_uptodate(bh))
				*partial = 1;
			continue;
		}
		err = (*fn)(handle, bh);
		if (!ret)
			ret = err;
	}
	return ret;
}

static sector_t ocfs2_bmap(struct address_space *mapping, sector_t block)
{
	sector_t status;
	u64 p_blkno = 0;
	int err = 0;
	struct inode *inode = mapping->host;

	trace_ocfs2_bmap((unsigned long long)OCFS2_I(inode)->ip_blkno,
			 (unsigned long long)block);

	 
	if (ocfs2_is_refcount_inode(inode))
		return 0;

	 
	if (!INODE_JOURNAL(inode)) {
		err = ocfs2_inode_lock(inode, NULL, 0);
		if (err) {
			if (err != -ENOENT)
				mlog_errno(err);
			goto bail;
		}
		down_read(&OCFS2_I(inode)->ip_alloc_sem);
	}

	if (!(OCFS2_I(inode)->ip_dyn_features & OCFS2_INLINE_DATA_FL))
		err = ocfs2_extent_map_get_blocks(inode, block, &p_blkno, NULL,
						  NULL);

	if (!INODE_JOURNAL(inode)) {
		up_read(&OCFS2_I(inode)->ip_alloc_sem);
		ocfs2_inode_unlock(inode, 0);
	}

	if (err) {
		mlog(ML_ERROR, "get_blocks() failed, block = %llu\n",
		     (unsigned long long)block);
		mlog_errno(err);
		goto bail;
	}

bail:
	status = err ? 0 : p_blkno;

	return status;
}

static bool ocfs2_release_folio(struct folio *folio, gfp_t wait)
{
	if (!folio_buffers(folio))
		return false;
	return try_to_free_buffers(folio);
}

static void ocfs2_figure_cluster_boundaries(struct ocfs2_super *osb,
					    u32 cpos,
					    unsigned int *start,
					    unsigned int *end)
{
	unsigned int cluster_start = 0, cluster_end = PAGE_SIZE;

	if (unlikely(PAGE_SHIFT > osb->s_clustersize_bits)) {
		unsigned int cpp;

		cpp = 1 << (PAGE_SHIFT - osb->s_clustersize_bits);

		cluster_start = cpos % cpp;
		cluster_start = cluster_start << osb->s_clustersize_bits;

		cluster_end = cluster_start + osb->s_clustersize;
	}

	BUG_ON(cluster_start > PAGE_SIZE);
	BUG_ON(cluster_end > PAGE_SIZE);

	if (start)
		*start = cluster_start;
	if (end)
		*end = cluster_end;
}

 
static void ocfs2_clear_page_regions(struct page *page,
				     struct ocfs2_super *osb, u32 cpos,
				     unsigned from, unsigned to)
{
	void *kaddr;
	unsigned int cluster_start, cluster_end;

	ocfs2_figure_cluster_boundaries(osb, cpos, &cluster_start, &cluster_end);

	kaddr = kmap_atomic(page);

	if (from || to) {
		if (from > cluster_start)
			memset(kaddr + cluster_start, 0, from - cluster_start);
		if (to < cluster_end)
			memset(kaddr + to, 0, cluster_end - to);
	} else {
		memset(kaddr + cluster_start, 0, cluster_end - cluster_start);
	}

	kunmap_atomic(kaddr);
}

 
static int ocfs2_should_read_blk(struct inode *inode, struct page *page,
				 unsigned int block_start)
{
	u64 offset = page_offset(page) + block_start;

	if (ocfs2_sparse_alloc(OCFS2_SB(inode->i_sb)))
		return 1;

	if (i_size_read(inode) > offset)
		return 1;

	return 0;
}

 
int ocfs2_map_page_blocks(struct page *page, u64 *p_blkno,
			  struct inode *inode, unsigned int from,
			  unsigned int to, int new)
{
	int ret = 0;
	struct buffer_head *head, *bh, *wait[2], **wait_bh = wait;
	unsigned int block_end, block_start;
	unsigned int bsize = i_blocksize(inode);

	if (!page_has_buffers(page))
		create_empty_buffers(page, bsize, 0);

	head = page_buffers(page);
	for (bh = head, block_start = 0; bh != head || !block_start;
	     bh = bh->b_this_page, block_start += bsize) {
		block_end = block_start + bsize;

		clear_buffer_new(bh);

		 
		if (block_start >= to || block_end <= from) {
			if (PageUptodate(page))
				set_buffer_uptodate(bh);
			continue;
		}

		 
		if (new)
			set_buffer_new(bh);

		if (!buffer_mapped(bh)) {
			map_bh(bh, inode->i_sb, *p_blkno);
			clean_bdev_bh_alias(bh);
		}

		if (PageUptodate(page)) {
			set_buffer_uptodate(bh);
		} else if (!buffer_uptodate(bh) && !buffer_delay(bh) &&
			   !buffer_new(bh) &&
			   ocfs2_should_read_blk(inode, page, block_start) &&
			   (block_start < from || block_end > to)) {
			bh_read_nowait(bh, 0);
			*wait_bh++=bh;
		}

		*p_blkno = *p_blkno + 1;
	}

	 
	while(wait_bh > wait) {
		wait_on_buffer(*--wait_bh);
		if (!buffer_uptodate(*wait_bh))
			ret = -EIO;
	}

	if (ret == 0 || !new)
		return ret;

	 
	bh = head;
	block_start = 0;
	do {
		block_end = block_start + bsize;
		if (block_end <= from)
			goto next_bh;
		if (block_start >= to)
			break;

		zero_user(page, block_start, bh->b_size);
		set_buffer_uptodate(bh);
		mark_buffer_dirty(bh);

next_bh:
		block_start = block_end;
		bh = bh->b_this_page;
	} while (bh != head);

	return ret;
}

#if (PAGE_SIZE >= OCFS2_MAX_CLUSTERSIZE)
#define OCFS2_MAX_CTXT_PAGES	1
#else
#define OCFS2_MAX_CTXT_PAGES	(OCFS2_MAX_CLUSTERSIZE / PAGE_SIZE)
#endif

#define OCFS2_MAX_CLUSTERS_PER_PAGE	(PAGE_SIZE / OCFS2_MIN_CLUSTERSIZE)

struct ocfs2_unwritten_extent {
	struct list_head	ue_node;
	struct list_head	ue_ip_node;
	u32			ue_cpos;
	u32			ue_phys;
};

 
struct ocfs2_write_cluster_desc {
	u32		c_cpos;
	u32		c_phys;
	 
	unsigned	c_new;
	unsigned	c_clear_unwritten;
	unsigned	c_needs_zero;
};

struct ocfs2_write_ctxt {
	 
	u32				w_cpos;
	u32				w_clen;

	 
	u32				w_first_new_cpos;

	 
	ocfs2_write_type_t		w_type;

	struct ocfs2_write_cluster_desc	w_desc[OCFS2_MAX_CLUSTERS_PER_PAGE];

	 
	unsigned int			w_large_pages;

	 
	unsigned int			w_num_pages;
	struct page			*w_pages[OCFS2_MAX_CTXT_PAGES];
	struct page			*w_target_page;

	 
	unsigned int			w_target_locked:1;

	 
	unsigned int			w_target_from;
	unsigned int			w_target_to;

	 
	handle_t			*w_handle;

	struct buffer_head		*w_di_bh;

	struct ocfs2_cached_dealloc_ctxt w_dealloc;

	struct list_head		w_unwritten_list;
	unsigned int			w_unwritten_count;
};

void ocfs2_unlock_and_free_pages(struct page **pages, int num_pages)
{
	int i;

	for(i = 0; i < num_pages; i++) {
		if (pages[i]) {
			unlock_page(pages[i]);
			mark_page_accessed(pages[i]);
			put_page(pages[i]);
		}
	}
}

static void ocfs2_unlock_pages(struct ocfs2_write_ctxt *wc)
{
	int i;

	 
	if (wc->w_target_locked) {
		BUG_ON(!wc->w_target_page);
		for (i = 0; i < wc->w_num_pages; i++) {
			if (wc->w_target_page == wc->w_pages[i]) {
				wc->w_pages[i] = NULL;
				break;
			}
		}
		mark_page_accessed(wc->w_target_page);
		put_page(wc->w_target_page);
	}
	ocfs2_unlock_and_free_pages(wc->w_pages, wc->w_num_pages);
}

static void ocfs2_free_unwritten_list(struct inode *inode,
				 struct list_head *head)
{
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_unwritten_extent *ue = NULL, *tmp = NULL;

	list_for_each_entry_safe(ue, tmp, head, ue_node) {
		list_del(&ue->ue_node);
		spin_lock(&oi->ip_lock);
		list_del(&ue->ue_ip_node);
		spin_unlock(&oi->ip_lock);
		kfree(ue);
	}
}

static void ocfs2_free_write_ctxt(struct inode *inode,
				  struct ocfs2_write_ctxt *wc)
{
	ocfs2_free_unwritten_list(inode, &wc->w_unwritten_list);
	ocfs2_unlock_pages(wc);
	brelse(wc->w_di_bh);
	kfree(wc);
}

static int ocfs2_alloc_write_ctxt(struct ocfs2_write_ctxt **wcp,
				  struct ocfs2_super *osb, loff_t pos,
				  unsigned len, ocfs2_write_type_t type,
				  struct buffer_head *di_bh)
{
	u32 cend;
	struct ocfs2_write_ctxt *wc;

	wc = kzalloc(sizeof(struct ocfs2_write_ctxt), GFP_NOFS);
	if (!wc)
		return -ENOMEM;

	wc->w_cpos = pos >> osb->s_clustersize_bits;
	wc->w_first_new_cpos = UINT_MAX;
	cend = (pos + len - 1) >> osb->s_clustersize_bits;
	wc->w_clen = cend - wc->w_cpos + 1;
	get_bh(di_bh);
	wc->w_di_bh = di_bh;
	wc->w_type = type;

	if (unlikely(PAGE_SHIFT > osb->s_clustersize_bits))
		wc->w_large_pages = 1;
	else
		wc->w_large_pages = 0;

	ocfs2_init_dealloc_ctxt(&wc->w_dealloc);
	INIT_LIST_HEAD(&wc->w_unwritten_list);

	*wcp = wc;

	return 0;
}

 
static void ocfs2_zero_new_buffers(struct page *page, unsigned from, unsigned to)
{
	unsigned int block_start, block_end;
	struct buffer_head *head, *bh;

	BUG_ON(!PageLocked(page));
	if (!page_has_buffers(page))
		return;

	bh = head = page_buffers(page);
	block_start = 0;
	do {
		block_end = block_start + bh->b_size;

		if (buffer_new(bh)) {
			if (block_end > from && block_start < to) {
				if (!PageUptodate(page)) {
					unsigned start, end;

					start = max(from, block_start);
					end = min(to, block_end);

					zero_user_segment(page, start, end);
					set_buffer_uptodate(bh);
				}

				clear_buffer_new(bh);
				mark_buffer_dirty(bh);
			}
		}

		block_start = block_end;
		bh = bh->b_this_page;
	} while (bh != head);
}

 
static void ocfs2_write_failure(struct inode *inode,
				struct ocfs2_write_ctxt *wc,
				loff_t user_pos, unsigned user_len)
{
	int i;
	unsigned from = user_pos & (PAGE_SIZE - 1),
		to = user_pos + user_len;
	struct page *tmppage;

	if (wc->w_target_page)
		ocfs2_zero_new_buffers(wc->w_target_page, from, to);

	for(i = 0; i < wc->w_num_pages; i++) {
		tmppage = wc->w_pages[i];

		if (tmppage && page_has_buffers(tmppage)) {
			if (ocfs2_should_order_data(inode))
				ocfs2_jbd2_inode_add_write(wc->w_handle, inode,
							   user_pos, user_len);

			block_commit_write(tmppage, from, to);
		}
	}
}

static int ocfs2_prepare_page_for_write(struct inode *inode, u64 *p_blkno,
					struct ocfs2_write_ctxt *wc,
					struct page *page, u32 cpos,
					loff_t user_pos, unsigned user_len,
					int new)
{
	int ret;
	unsigned int map_from = 0, map_to = 0;
	unsigned int cluster_start, cluster_end;
	unsigned int user_data_from = 0, user_data_to = 0;

	ocfs2_figure_cluster_boundaries(OCFS2_SB(inode->i_sb), cpos,
					&cluster_start, &cluster_end);

	 
	new = new | ((i_size_read(inode) <= page_offset(page)) &&
			(page_offset(page) <= user_pos));

	if (page == wc->w_target_page) {
		map_from = user_pos & (PAGE_SIZE - 1);
		map_to = map_from + user_len;

		if (new)
			ret = ocfs2_map_page_blocks(page, p_blkno, inode,
						    cluster_start, cluster_end,
						    new);
		else
			ret = ocfs2_map_page_blocks(page, p_blkno, inode,
						    map_from, map_to, new);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		user_data_from = map_from;
		user_data_to = map_to;
		if (new) {
			map_from = cluster_start;
			map_to = cluster_end;
		}
	} else {
		 
		BUG_ON(!new);

		map_from = cluster_start;
		map_to = cluster_end;

		ret = ocfs2_map_page_blocks(page, p_blkno, inode,
					    cluster_start, cluster_end, new);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

	 
	if (new && !PageUptodate(page))
		ocfs2_clear_page_regions(page, OCFS2_SB(inode->i_sb),
					 cpos, user_data_from, user_data_to);

	flush_dcache_page(page);

out:
	return ret;
}

 
static int ocfs2_grab_pages_for_write(struct address_space *mapping,
				      struct ocfs2_write_ctxt *wc,
				      u32 cpos, loff_t user_pos,
				      unsigned user_len, int new,
				      struct page *mmap_page)
{
	int ret = 0, i;
	unsigned long start, target_index, end_index, index;
	struct inode *inode = mapping->host;
	loff_t last_byte;

	target_index = user_pos >> PAGE_SHIFT;

	 
	if (new) {
		wc->w_num_pages = ocfs2_pages_per_cluster(inode->i_sb);
		start = ocfs2_align_clusters_to_page_index(inode->i_sb, cpos);
		 
		last_byte = max(user_pos + user_len, i_size_read(inode));
		BUG_ON(last_byte < 1);
		end_index = ((last_byte - 1) >> PAGE_SHIFT) + 1;
		if ((start + wc->w_num_pages) > end_index)
			wc->w_num_pages = end_index - start;
	} else {
		wc->w_num_pages = 1;
		start = target_index;
	}
	end_index = (user_pos + user_len - 1) >> PAGE_SHIFT;

	for(i = 0; i < wc->w_num_pages; i++) {
		index = start + i;

		if (index >= target_index && index <= end_index &&
		    wc->w_type == OCFS2_WRITE_MMAP) {
			 
			lock_page(mmap_page);

			 
			if (mmap_page->mapping != mapping) {
				WARN_ON(mmap_page->mapping);
				unlock_page(mmap_page);
				ret = -EAGAIN;
				goto out;
			}

			get_page(mmap_page);
			wc->w_pages[i] = mmap_page;
			wc->w_target_locked = true;
		} else if (index >= target_index && index <= end_index &&
			   wc->w_type == OCFS2_WRITE_DIRECT) {
			 
			wc->w_pages[i] = NULL;
			continue;
		} else {
			wc->w_pages[i] = find_or_create_page(mapping, index,
							     GFP_NOFS);
			if (!wc->w_pages[i]) {
				ret = -ENOMEM;
				mlog_errno(ret);
				goto out;
			}
		}
		wait_for_stable_page(wc->w_pages[i]);

		if (index == target_index)
			wc->w_target_page = wc->w_pages[i];
	}
out:
	if (ret)
		wc->w_target_locked = false;
	return ret;
}

 
static int ocfs2_write_cluster(struct address_space *mapping,
			       u32 *phys, unsigned int new,
			       unsigned int clear_unwritten,
			       unsigned int should_zero,
			       struct ocfs2_alloc_context *data_ac,
			       struct ocfs2_alloc_context *meta_ac,
			       struct ocfs2_write_ctxt *wc, u32 cpos,
			       loff_t user_pos, unsigned user_len)
{
	int ret, i;
	u64 p_blkno;
	struct inode *inode = mapping->host;
	struct ocfs2_extent_tree et;
	int bpc = ocfs2_clusters_to_blocks(inode->i_sb, 1);

	if (new) {
		u32 tmp_pos;

		 
		tmp_pos = cpos;
		ret = ocfs2_add_inode_data(OCFS2_SB(inode->i_sb), inode,
					   &tmp_pos, 1, !clear_unwritten,
					   wc->w_di_bh, wc->w_handle,
					   data_ac, meta_ac, NULL);
		 
		mlog_bug_on_msg(ret == -EAGAIN,
				"Inode %llu: EAGAIN return during allocation.\n",
				(unsigned long long)OCFS2_I(inode)->ip_blkno);
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
	} else if (clear_unwritten) {
		ocfs2_init_dinode_extent_tree(&et, INODE_CACHE(inode),
					      wc->w_di_bh);
		ret = ocfs2_mark_extent_written(inode, &et,
						wc->w_handle, cpos, 1, *phys,
						meta_ac, &wc->w_dealloc);
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
	}

	 
	ret = ocfs2_get_clusters(inode, cpos, phys, NULL, NULL);
	if (ret < 0) {
		mlog(ML_ERROR, "Get physical blkno failed for inode %llu, "
			    "at logical cluster %u",
			    (unsigned long long)OCFS2_I(inode)->ip_blkno, cpos);
		goto out;
	}

	BUG_ON(*phys == 0);

	p_blkno = ocfs2_clusters_to_blocks(inode->i_sb, *phys);
	if (!should_zero)
		p_blkno += (user_pos >> inode->i_sb->s_blocksize_bits) & (u64)(bpc - 1);

	for(i = 0; i < wc->w_num_pages; i++) {
		int tmpret;

		 
		if (wc->w_pages[i] == NULL) {
			p_blkno++;
			continue;
		}

		tmpret = ocfs2_prepare_page_for_write(inode, &p_blkno, wc,
						      wc->w_pages[i], cpos,
						      user_pos, user_len,
						      should_zero);
		if (tmpret) {
			mlog_errno(tmpret);
			if (ret == 0)
				ret = tmpret;
		}
	}

	 
	if (ret && new)
		ocfs2_write_failure(inode, wc, user_pos, user_len);

out:

	return ret;
}

static int ocfs2_write_cluster_by_desc(struct address_space *mapping,
				       struct ocfs2_alloc_context *data_ac,
				       struct ocfs2_alloc_context *meta_ac,
				       struct ocfs2_write_ctxt *wc,
				       loff_t pos, unsigned len)
{
	int ret, i;
	loff_t cluster_off;
	unsigned int local_len = len;
	struct ocfs2_write_cluster_desc *desc;
	struct ocfs2_super *osb = OCFS2_SB(mapping->host->i_sb);

	for (i = 0; i < wc->w_clen; i++) {
		desc = &wc->w_desc[i];

		 
		local_len = len;
		cluster_off = pos & (osb->s_clustersize - 1);
		if ((cluster_off + local_len) > osb->s_clustersize)
			local_len = osb->s_clustersize - cluster_off;

		ret = ocfs2_write_cluster(mapping, &desc->c_phys,
					  desc->c_new,
					  desc->c_clear_unwritten,
					  desc->c_needs_zero,
					  data_ac, meta_ac,
					  wc, desc->c_cpos, pos, local_len);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		len -= local_len;
		pos += local_len;
	}

	ret = 0;
out:
	return ret;
}

 
static void ocfs2_set_target_boundaries(struct ocfs2_super *osb,
					struct ocfs2_write_ctxt *wc,
					loff_t pos, unsigned len, int alloc)
{
	struct ocfs2_write_cluster_desc *desc;

	wc->w_target_from = pos & (PAGE_SIZE - 1);
	wc->w_target_to = wc->w_target_from + len;

	if (alloc == 0)
		return;

	 

	if (wc->w_large_pages) {
		 
		desc = &wc->w_desc[0];
		if (desc->c_needs_zero)
			ocfs2_figure_cluster_boundaries(osb,
							desc->c_cpos,
							&wc->w_target_from,
							NULL);

		desc = &wc->w_desc[wc->w_clen - 1];
		if (desc->c_needs_zero)
			ocfs2_figure_cluster_boundaries(osb,
							desc->c_cpos,
							NULL,
							&wc->w_target_to);
	} else {
		wc->w_target_from = 0;
		wc->w_target_to = PAGE_SIZE;
	}
}

 
static int ocfs2_unwritten_check(struct inode *inode,
				 struct ocfs2_write_ctxt *wc,
				 struct ocfs2_write_cluster_desc *desc)
{
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_unwritten_extent *ue = NULL, *new = NULL;
	int ret = 0;

	if (!desc->c_needs_zero)
		return 0;

retry:
	spin_lock(&oi->ip_lock);
	 
	list_for_each_entry(ue, &oi->ip_unwritten_list, ue_ip_node) {
		if (desc->c_cpos == ue->ue_cpos) {
			BUG_ON(desc->c_new);
			desc->c_needs_zero = 0;
			desc->c_clear_unwritten = 0;
			goto unlock;
		}
	}

	if (wc->w_type != OCFS2_WRITE_DIRECT)
		goto unlock;

	if (new == NULL) {
		spin_unlock(&oi->ip_lock);
		new = kmalloc(sizeof(struct ocfs2_unwritten_extent),
			     GFP_NOFS);
		if (new == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		goto retry;
	}
	 
	new->ue_cpos = desc->c_cpos;
	new->ue_phys = desc->c_phys;
	desc->c_clear_unwritten = 0;
	list_add_tail(&new->ue_ip_node, &oi->ip_unwritten_list);
	list_add_tail(&new->ue_node, &wc->w_unwritten_list);
	wc->w_unwritten_count++;
	new = NULL;
unlock:
	spin_unlock(&oi->ip_lock);
out:
	kfree(new);
	return ret;
}

 
static int ocfs2_populate_write_desc(struct inode *inode,
				     struct ocfs2_write_ctxt *wc,
				     unsigned int *clusters_to_alloc,
				     unsigned int *extents_to_split)
{
	int ret;
	struct ocfs2_write_cluster_desc *desc;
	unsigned int num_clusters = 0;
	unsigned int ext_flags = 0;
	u32 phys = 0;
	int i;

	*clusters_to_alloc = 0;
	*extents_to_split = 0;

	for (i = 0; i < wc->w_clen; i++) {
		desc = &wc->w_desc[i];
		desc->c_cpos = wc->w_cpos + i;

		if (num_clusters == 0) {
			 
			ret = ocfs2_get_clusters(inode, desc->c_cpos, &phys,
						 &num_clusters, &ext_flags);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			 
			BUG_ON(ext_flags & OCFS2_EXT_REFCOUNTED);

			 
			if (ext_flags & OCFS2_EXT_UNWRITTEN)
				*extents_to_split = *extents_to_split + 2;
		} else if (phys) {
			 
			phys++;
		}

		 
		if (desc->c_cpos >= wc->w_first_new_cpos) {
			BUG_ON(phys == 0);
			desc->c_needs_zero = 1;
		}

		desc->c_phys = phys;
		if (phys == 0) {
			desc->c_new = 1;
			desc->c_needs_zero = 1;
			desc->c_clear_unwritten = 1;
			*clusters_to_alloc = *clusters_to_alloc + 1;
		}

		if (ext_flags & OCFS2_EXT_UNWRITTEN) {
			desc->c_clear_unwritten = 1;
			desc->c_needs_zero = 1;
		}

		ret = ocfs2_unwritten_check(inode, wc, desc);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		num_clusters--;
	}

	ret = 0;
out:
	return ret;
}

static int ocfs2_write_begin_inline(struct address_space *mapping,
				    struct inode *inode,
				    struct ocfs2_write_ctxt *wc)
{
	int ret;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct page *page;
	handle_t *handle;
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)wc->w_di_bh->b_data;

	handle = ocfs2_start_trans(osb, OCFS2_INODE_UPDATE_CREDITS);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		mlog_errno(ret);
		goto out;
	}

	page = find_or_create_page(mapping, 0, GFP_NOFS);
	if (!page) {
		ocfs2_commit_trans(osb, handle);
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}
	 
	wc->w_pages[0] = wc->w_target_page = page;
	wc->w_num_pages = 1;

	ret = ocfs2_journal_access_di(handle, INODE_CACHE(inode), wc->w_di_bh,
				      OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		ocfs2_commit_trans(osb, handle);

		mlog_errno(ret);
		goto out;
	}

	if (!(OCFS2_I(inode)->ip_dyn_features & OCFS2_INLINE_DATA_FL))
		ocfs2_set_inode_data_inline(inode, di);

	if (!PageUptodate(page)) {
		ret = ocfs2_read_inline_data(inode, page, wc->w_di_bh);
		if (ret) {
			ocfs2_commit_trans(osb, handle);

			goto out;
		}
	}

	wc->w_handle = handle;
out:
	return ret;
}

int ocfs2_size_fits_inline_data(struct buffer_head *di_bh, u64 new_size)
{
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)di_bh->b_data;

	if (new_size <= le16_to_cpu(di->id2.i_data.id_count))
		return 1;
	return 0;
}

static int ocfs2_try_to_write_inline_data(struct address_space *mapping,
					  struct inode *inode, loff_t pos,
					  unsigned len, struct page *mmap_page,
					  struct ocfs2_write_ctxt *wc)
{
	int ret, written = 0;
	loff_t end = pos + len;
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_dinode *di = NULL;

	trace_ocfs2_try_to_write_inline_data((unsigned long long)oi->ip_blkno,
					     len, (unsigned long long)pos,
					     oi->ip_dyn_features);

	 
	if (oi->ip_dyn_features & OCFS2_INLINE_DATA_FL) {
		if (mmap_page == NULL &&
		    ocfs2_size_fits_inline_data(wc->w_di_bh, end))
			goto do_inline_write;

		 
		ret = ocfs2_convert_inline_data_to_extents(inode, wc->w_di_bh);
		if (ret)
			mlog_errno(ret);
		goto out;
	}

	 
	if (oi->ip_clusters != 0 || i_size_read(inode) != 0)
		return 0;

	 
	di = (struct ocfs2_dinode *)wc->w_di_bh->b_data;
	if (mmap_page ||
	    end > ocfs2_max_inline_data_with_xattr(inode->i_sb, di))
		return 0;

do_inline_write:
	ret = ocfs2_write_begin_inline(mapping, inode, wc);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	 
	written = 1;
out:
	return written ? written : ret;
}

 
static int ocfs2_expand_nonsparse_inode(struct inode *inode,
					struct buffer_head *di_bh,
					loff_t pos, unsigned len,
					struct ocfs2_write_ctxt *wc)
{
	int ret;
	loff_t newsize = pos + len;

	BUG_ON(ocfs2_sparse_alloc(OCFS2_SB(inode->i_sb)));

	if (newsize <= i_size_read(inode))
		return 0;

	ret = ocfs2_extend_no_holes(inode, di_bh, newsize, pos);
	if (ret)
		mlog_errno(ret);

	 
	if (wc)
		wc->w_first_new_cpos =
			ocfs2_clusters_for_bytes(inode->i_sb, i_size_read(inode));

	return ret;
}

static int ocfs2_zero_tail(struct inode *inode, struct buffer_head *di_bh,
			   loff_t pos)
{
	int ret = 0;

	BUG_ON(!ocfs2_sparse_alloc(OCFS2_SB(inode->i_sb)));
	if (pos > i_size_read(inode))
		ret = ocfs2_zero_extend(inode, di_bh, pos);

	return ret;
}

int ocfs2_write_begin_nolock(struct address_space *mapping,
			     loff_t pos, unsigned len, ocfs2_write_type_t type,
			     struct page **pagep, void **fsdata,
			     struct buffer_head *di_bh, struct page *mmap_page)
{
	int ret, cluster_of_pages, credits = OCFS2_INODE_UPDATE_CREDITS;
	unsigned int clusters_to_alloc, extents_to_split, clusters_need = 0;
	struct ocfs2_write_ctxt *wc;
	struct inode *inode = mapping->host;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct ocfs2_dinode *di;
	struct ocfs2_alloc_context *data_ac = NULL;
	struct ocfs2_alloc_context *meta_ac = NULL;
	handle_t *handle;
	struct ocfs2_extent_tree et;
	int try_free = 1, ret1;

try_again:
	ret = ocfs2_alloc_write_ctxt(&wc, osb, pos, len, type, di_bh);
	if (ret) {
		mlog_errno(ret);
		return ret;
	}

	if (ocfs2_supports_inline_data(osb)) {
		ret = ocfs2_try_to_write_inline_data(mapping, inode, pos, len,
						     mmap_page, wc);
		if (ret == 1) {
			ret = 0;
			goto success;
		}
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
	}

	 
	if (type != OCFS2_WRITE_DIRECT) {
		if (ocfs2_sparse_alloc(osb))
			ret = ocfs2_zero_tail(inode, di_bh, pos);
		else
			ret = ocfs2_expand_nonsparse_inode(inode, di_bh, pos,
							   len, wc);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

	ret = ocfs2_check_range_for_refcount(inode, pos, len);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	} else if (ret == 1) {
		clusters_need = wc->w_clen;
		ret = ocfs2_refcount_cow(inode, di_bh,
					 wc->w_cpos, wc->w_clen, UINT_MAX);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

	ret = ocfs2_populate_write_desc(inode, wc, &clusters_to_alloc,
					&extents_to_split);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}
	clusters_need += clusters_to_alloc;

	di = (struct ocfs2_dinode *)wc->w_di_bh->b_data;

	trace_ocfs2_write_begin_nolock(
			(unsigned long long)OCFS2_I(inode)->ip_blkno,
			(long long)i_size_read(inode),
			le32_to_cpu(di->i_clusters),
			pos, len, type, mmap_page,
			clusters_to_alloc, extents_to_split);

	 
	if (clusters_to_alloc || extents_to_split) {
		 
		ocfs2_init_dinode_extent_tree(&et, INODE_CACHE(inode),
					      wc->w_di_bh);
		ret = ocfs2_lock_allocators(inode, &et,
					    clusters_to_alloc, extents_to_split,
					    &data_ac, &meta_ac);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		if (data_ac)
			data_ac->ac_resv = &OCFS2_I(inode)->ip_la_data_resv;

		credits = ocfs2_calc_extend_credits(inode->i_sb,
						    &di->id2.i_list);
	} else if (type == OCFS2_WRITE_DIRECT)
		 
		goto success;

	 
	if (wc->w_clen && (wc->w_desc[0].c_needs_zero ||
			   wc->w_desc[wc->w_clen - 1].c_needs_zero))
		cluster_of_pages = 1;
	else
		cluster_of_pages = 0;

	ocfs2_set_target_boundaries(osb, wc, pos, len, cluster_of_pages);

	handle = ocfs2_start_trans(osb, credits);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		mlog_errno(ret);
		goto out;
	}

	wc->w_handle = handle;

	if (clusters_to_alloc) {
		ret = dquot_alloc_space_nodirty(inode,
			ocfs2_clusters_to_bytes(osb->sb, clusters_to_alloc));
		if (ret)
			goto out_commit;
	}

	ret = ocfs2_journal_access_di(handle, INODE_CACHE(inode), wc->w_di_bh,
				      OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto out_quota;
	}

	 
	ret = ocfs2_grab_pages_for_write(mapping, wc, wc->w_cpos, pos, len,
					 cluster_of_pages, mmap_page);
	if (ret) {
		 
		if (type == OCFS2_WRITE_MMAP && ret == -EAGAIN) {
			BUG_ON(wc->w_target_page);
			ret = 0;
			goto out_quota;
		}

		mlog_errno(ret);
		goto out_quota;
	}

	ret = ocfs2_write_cluster_by_desc(mapping, data_ac, meta_ac, wc, pos,
					  len);
	if (ret) {
		mlog_errno(ret);
		goto out_quota;
	}

	if (data_ac)
		ocfs2_free_alloc_context(data_ac);
	if (meta_ac)
		ocfs2_free_alloc_context(meta_ac);

success:
	if (pagep)
		*pagep = wc->w_target_page;
	*fsdata = wc;
	return 0;
out_quota:
	if (clusters_to_alloc)
		dquot_free_space(inode,
			  ocfs2_clusters_to_bytes(osb->sb, clusters_to_alloc));
out_commit:
	ocfs2_commit_trans(osb, handle);

out:
	 
	if (wc->w_target_locked)
		unlock_page(mmap_page);

	ocfs2_free_write_ctxt(inode, wc);

	if (data_ac) {
		ocfs2_free_alloc_context(data_ac);
		data_ac = NULL;
	}
	if (meta_ac) {
		ocfs2_free_alloc_context(meta_ac);
		meta_ac = NULL;
	}

	if (ret == -ENOSPC && try_free) {
		 
		try_free = 0;

		ret1 = ocfs2_try_to_free_truncate_log(osb, clusters_need);
		if (ret1 == 1)
			goto try_again;

		if (ret1 < 0)
			mlog_errno(ret1);
	}

	return ret;
}

static int ocfs2_write_begin(struct file *file, struct address_space *mapping,
			     loff_t pos, unsigned len,
			     struct page **pagep, void **fsdata)
{
	int ret;
	struct buffer_head *di_bh = NULL;
	struct inode *inode = mapping->host;

	ret = ocfs2_inode_lock(inode, &di_bh, 1);
	if (ret) {
		mlog_errno(ret);
		return ret;
	}

	 
	down_write(&OCFS2_I(inode)->ip_alloc_sem);

	ret = ocfs2_write_begin_nolock(mapping, pos, len, OCFS2_WRITE_BUFFER,
				       pagep, fsdata, di_bh, NULL);
	if (ret) {
		mlog_errno(ret);
		goto out_fail;
	}

	brelse(di_bh);

	return 0;

out_fail:
	up_write(&OCFS2_I(inode)->ip_alloc_sem);

	brelse(di_bh);
	ocfs2_inode_unlock(inode, 1);

	return ret;
}

static void ocfs2_write_end_inline(struct inode *inode, loff_t pos,
				   unsigned len, unsigned *copied,
				   struct ocfs2_dinode *di,
				   struct ocfs2_write_ctxt *wc)
{
	void *kaddr;

	if (unlikely(*copied < len)) {
		if (!PageUptodate(wc->w_target_page)) {
			*copied = 0;
			return;
		}
	}

	kaddr = kmap_atomic(wc->w_target_page);
	memcpy(di->id2.i_data.id_data + pos, kaddr + pos, *copied);
	kunmap_atomic(kaddr);

	trace_ocfs2_write_end_inline(
	     (unsigned long long)OCFS2_I(inode)->ip_blkno,
	     (unsigned long long)pos, *copied,
	     le16_to_cpu(di->id2.i_data.id_count),
	     le16_to_cpu(di->i_dyn_features));
}

int ocfs2_write_end_nolock(struct address_space *mapping,
			   loff_t pos, unsigned len, unsigned copied, void *fsdata)
{
	int i, ret;
	unsigned from, to, start = pos & (PAGE_SIZE - 1);
	struct inode *inode = mapping->host;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct ocfs2_write_ctxt *wc = fsdata;
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)wc->w_di_bh->b_data;
	handle_t *handle = wc->w_handle;
	struct page *tmppage;

	BUG_ON(!list_empty(&wc->w_unwritten_list));

	if (handle) {
		ret = ocfs2_journal_access_di(handle, INODE_CACHE(inode),
				wc->w_di_bh, OCFS2_JOURNAL_ACCESS_WRITE);
		if (ret) {
			copied = ret;
			mlog_errno(ret);
			goto out;
		}
	}

	if (OCFS2_I(inode)->ip_dyn_features & OCFS2_INLINE_DATA_FL) {
		ocfs2_write_end_inline(inode, pos, len, &copied, di, wc);
		goto out_write_size;
	}

	if (unlikely(copied < len) && wc->w_target_page) {
		loff_t new_isize;

		if (!PageUptodate(wc->w_target_page))
			copied = 0;

		new_isize = max_t(loff_t, i_size_read(inode), pos + copied);
		if (new_isize > page_offset(wc->w_target_page))
			ocfs2_zero_new_buffers(wc->w_target_page, start+copied,
					       start+len);
		else {
			 
			block_invalidate_folio(page_folio(wc->w_target_page),
						0, PAGE_SIZE);
		}
	}
	if (wc->w_target_page)
		flush_dcache_page(wc->w_target_page);

	for(i = 0; i < wc->w_num_pages; i++) {
		tmppage = wc->w_pages[i];

		 
		if (tmppage == NULL)
			continue;

		if (tmppage == wc->w_target_page) {
			from = wc->w_target_from;
			to = wc->w_target_to;

			BUG_ON(from > PAGE_SIZE ||
			       to > PAGE_SIZE ||
			       to < from);
		} else {
			 
			from = 0;
			to = PAGE_SIZE;
		}

		if (page_has_buffers(tmppage)) {
			if (handle && ocfs2_should_order_data(inode)) {
				loff_t start_byte =
					((loff_t)tmppage->index << PAGE_SHIFT) +
					from;
				loff_t length = to - from;
				ocfs2_jbd2_inode_add_write(handle, inode,
							   start_byte, length);
			}
			block_commit_write(tmppage, from, to);
		}
	}

out_write_size:
	 
	if (wc->w_type != OCFS2_WRITE_DIRECT) {
		pos += copied;
		if (pos > i_size_read(inode)) {
			i_size_write(inode, pos);
			mark_inode_dirty(inode);
		}
		inode->i_blocks = ocfs2_inode_sector_count(inode);
		di->i_size = cpu_to_le64((u64)i_size_read(inode));
		inode->i_mtime = inode_set_ctime_current(inode);
		di->i_mtime = di->i_ctime = cpu_to_le64(inode->i_mtime.tv_sec);
		di->i_mtime_nsec = di->i_ctime_nsec = cpu_to_le32(inode->i_mtime.tv_nsec);
		if (handle)
			ocfs2_update_inode_fsync_trans(handle, inode, 1);
	}
	if (handle)
		ocfs2_journal_dirty(handle, wc->w_di_bh);

out:
	 
	ocfs2_unlock_pages(wc);

	if (handle)
		ocfs2_commit_trans(osb, handle);

	ocfs2_run_deallocs(osb, &wc->w_dealloc);

	brelse(wc->w_di_bh);
	kfree(wc);

	return copied;
}

static int ocfs2_write_end(struct file *file, struct address_space *mapping,
			   loff_t pos, unsigned len, unsigned copied,
			   struct page *page, void *fsdata)
{
	int ret;
	struct inode *inode = mapping->host;

	ret = ocfs2_write_end_nolock(mapping, pos, len, copied, fsdata);

	up_write(&OCFS2_I(inode)->ip_alloc_sem);
	ocfs2_inode_unlock(inode, 1);

	return ret;
}

struct ocfs2_dio_write_ctxt {
	struct list_head	dw_zero_list;
	unsigned		dw_zero_count;
	int			dw_orphaned;
	pid_t			dw_writer_pid;
};

static struct ocfs2_dio_write_ctxt *
ocfs2_dio_alloc_write_ctx(struct buffer_head *bh, int *alloc)
{
	struct ocfs2_dio_write_ctxt *dwc = NULL;

	if (bh->b_private)
		return bh->b_private;

	dwc = kmalloc(sizeof(struct ocfs2_dio_write_ctxt), GFP_NOFS);
	if (dwc == NULL)
		return NULL;
	INIT_LIST_HEAD(&dwc->dw_zero_list);
	dwc->dw_zero_count = 0;
	dwc->dw_orphaned = 0;
	dwc->dw_writer_pid = task_pid_nr(current);
	bh->b_private = dwc;
	*alloc = 1;

	return dwc;
}

static void ocfs2_dio_free_write_ctx(struct inode *inode,
				     struct ocfs2_dio_write_ctxt *dwc)
{
	ocfs2_free_unwritten_list(inode, &dwc->dw_zero_list);
	kfree(dwc);
}

 
static int ocfs2_dio_wr_get_block(struct inode *inode, sector_t iblock,
			       struct buffer_head *bh_result, int create)
{
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_write_ctxt *wc;
	struct ocfs2_write_cluster_desc *desc = NULL;
	struct ocfs2_dio_write_ctxt *dwc = NULL;
	struct buffer_head *di_bh = NULL;
	u64 p_blkno;
	unsigned int i_blkbits = inode->i_sb->s_blocksize_bits;
	loff_t pos = iblock << i_blkbits;
	sector_t endblk = (i_size_read(inode) - 1) >> i_blkbits;
	unsigned len, total_len = bh_result->b_size;
	int ret = 0, first_get_block = 0;

	len = osb->s_clustersize - (pos & (osb->s_clustersize - 1));
	len = min(total_len, len);

	 

	if ((iblock <= endblk) &&
	    ((iblock + ((len - 1) >> i_blkbits)) > endblk))
		len = (endblk - iblock + 1) << i_blkbits;

	mlog(0, "get block of %lu at %llu:%u req %u\n",
			inode->i_ino, pos, len, total_len);

	 
	if (pos + total_len <= i_size_read(inode)) {

		 
		ret = ocfs2_lock_get_block(inode, iblock, bh_result, create);
		if (buffer_mapped(bh_result) &&
		    !buffer_new(bh_result) &&
		    ret == 0)
			goto out;

		 
		bh_result->b_state = 0;
	}

	dwc = ocfs2_dio_alloc_write_ctx(bh_result, &first_get_block);
	if (unlikely(dwc == NULL)) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}

	if (ocfs2_clusters_for_bytes(inode->i_sb, pos + total_len) >
	    ocfs2_clusters_for_bytes(inode->i_sb, i_size_read(inode)) &&
	    !dwc->dw_orphaned) {
		 
		ret = ocfs2_add_inode_to_orphan(osb, inode);
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
		dwc->dw_orphaned = 1;
	}

	ret = ocfs2_inode_lock(inode, &di_bh, 1);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	down_write(&oi->ip_alloc_sem);

	if (first_get_block) {
		if (ocfs2_sparse_alloc(osb))
			ret = ocfs2_zero_tail(inode, di_bh, pos);
		else
			ret = ocfs2_expand_nonsparse_inode(inode, di_bh, pos,
							   total_len, NULL);
		if (ret < 0) {
			mlog_errno(ret);
			goto unlock;
		}
	}

	ret = ocfs2_write_begin_nolock(inode->i_mapping, pos, len,
				       OCFS2_WRITE_DIRECT, NULL,
				       (void **)&wc, di_bh, NULL);
	if (ret) {
		mlog_errno(ret);
		goto unlock;
	}

	desc = &wc->w_desc[0];

	p_blkno = ocfs2_clusters_to_blocks(inode->i_sb, desc->c_phys);
	BUG_ON(p_blkno == 0);
	p_blkno += iblock & (u64)(ocfs2_clusters_to_blocks(inode->i_sb, 1) - 1);

	map_bh(bh_result, inode->i_sb, p_blkno);
	bh_result->b_size = len;
	if (desc->c_needs_zero)
		set_buffer_new(bh_result);

	if (iblock > endblk)
		set_buffer_new(bh_result);

	 
	set_buffer_defer_completion(bh_result);

	if (!list_empty(&wc->w_unwritten_list)) {
		struct ocfs2_unwritten_extent *ue = NULL;

		ue = list_first_entry(&wc->w_unwritten_list,
				      struct ocfs2_unwritten_extent,
				      ue_node);
		BUG_ON(ue->ue_cpos != desc->c_cpos);
		 
		ue->ue_phys = desc->c_phys;

		list_splice_tail_init(&wc->w_unwritten_list, &dwc->dw_zero_list);
		dwc->dw_zero_count += wc->w_unwritten_count;
	}

	ret = ocfs2_write_end_nolock(inode->i_mapping, pos, len, len, wc);
	BUG_ON(ret != len);
	ret = 0;
unlock:
	up_write(&oi->ip_alloc_sem);
	ocfs2_inode_unlock(inode, 1);
	brelse(di_bh);
out:
	if (ret < 0)
		ret = -EIO;
	return ret;
}

static int ocfs2_dio_end_io_write(struct inode *inode,
				  struct ocfs2_dio_write_ctxt *dwc,
				  loff_t offset,
				  ssize_t bytes)
{
	struct ocfs2_cached_dealloc_ctxt dealloc;
	struct ocfs2_extent_tree et;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_unwritten_extent *ue = NULL;
	struct buffer_head *di_bh = NULL;
	struct ocfs2_dinode *di;
	struct ocfs2_alloc_context *data_ac = NULL;
	struct ocfs2_alloc_context *meta_ac = NULL;
	handle_t *handle = NULL;
	loff_t end = offset + bytes;
	int ret = 0, credits = 0;

	ocfs2_init_dealloc_ctxt(&dealloc);

	 
	if (list_empty(&dwc->dw_zero_list) &&
	    end <= i_size_read(inode) &&
	    !dwc->dw_orphaned)
		goto out;

	ret = ocfs2_inode_lock(inode, &di_bh, 1);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	}

	down_write(&oi->ip_alloc_sem);

	 
	if (dwc->dw_orphaned) {
		BUG_ON(dwc->dw_writer_pid != task_pid_nr(current));

		end = end > i_size_read(inode) ? end : 0;

		ret = ocfs2_del_inode_from_orphan(osb, inode, di_bh,
				!!end, end);
		if (ret < 0)
			mlog_errno(ret);
	}

	di = (struct ocfs2_dinode *)di_bh->b_data;

	ocfs2_init_dinode_extent_tree(&et, INODE_CACHE(inode), di_bh);

	 
	et.et_dealloc = &dealloc;

	ret = ocfs2_lock_allocators(inode, &et, 0, dwc->dw_zero_count*2,
				    &data_ac, &meta_ac);
	if (ret) {
		mlog_errno(ret);
		goto unlock;
	}

	credits = ocfs2_calc_extend_credits(inode->i_sb, &di->id2.i_list);

	handle = ocfs2_start_trans(osb, credits);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		mlog_errno(ret);
		goto unlock;
	}
	ret = ocfs2_journal_access_di(handle, INODE_CACHE(inode), di_bh,
				      OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto commit;
	}

	list_for_each_entry(ue, &dwc->dw_zero_list, ue_node) {
		ret = ocfs2_mark_extent_written(inode, &et, handle,
						ue->ue_cpos, 1,
						ue->ue_phys,
						meta_ac, &dealloc);
		if (ret < 0) {
			mlog_errno(ret);
			break;
		}
	}

	if (end > i_size_read(inode)) {
		ret = ocfs2_set_inode_size(handle, inode, di_bh, end);
		if (ret < 0)
			mlog_errno(ret);
	}
commit:
	ocfs2_commit_trans(osb, handle);
unlock:
	up_write(&oi->ip_alloc_sem);
	ocfs2_inode_unlock(inode, 1);
	brelse(di_bh);
out:
	if (data_ac)
		ocfs2_free_alloc_context(data_ac);
	if (meta_ac)
		ocfs2_free_alloc_context(meta_ac);
	ocfs2_run_deallocs(osb, &dealloc);
	ocfs2_dio_free_write_ctx(inode, dwc);

	return ret;
}

 
static int ocfs2_dio_end_io(struct kiocb *iocb,
			    loff_t offset,
			    ssize_t bytes,
			    void *private)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	int level;
	int ret = 0;

	 
	BUG_ON(!ocfs2_iocb_is_rw_locked(iocb));

	if (bytes <= 0)
		mlog_ratelimited(ML_ERROR, "Direct IO failed, bytes = %lld",
				 (long long)bytes);
	if (private) {
		if (bytes > 0)
			ret = ocfs2_dio_end_io_write(inode, private, offset,
						     bytes);
		else
			ocfs2_dio_free_write_ctx(inode, private);
	}

	ocfs2_iocb_clear_rw_locked(iocb);

	level = ocfs2_iocb_rw_locked_level(iocb);
	ocfs2_rw_unlock(inode, level);
	return ret;
}

static ssize_t ocfs2_direct_IO(struct kiocb *iocb, struct iov_iter *iter)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	get_block_t *get_block;

	 
	if (OCFS2_I(inode)->ip_dyn_features & OCFS2_INLINE_DATA_FL)
		return 0;

	 
	if (iocb->ki_pos + iter->count > i_size_read(inode) &&
	    !ocfs2_supports_append_dio(osb))
		return 0;

	if (iov_iter_rw(iter) == READ)
		get_block = ocfs2_lock_get_block;
	else
		get_block = ocfs2_dio_wr_get_block;

	return __blockdev_direct_IO(iocb, inode, inode->i_sb->s_bdev,
				    iter, get_block,
				    ocfs2_dio_end_io, 0);
}

const struct address_space_operations ocfs2_aops = {
	.dirty_folio		= block_dirty_folio,
	.read_folio		= ocfs2_read_folio,
	.readahead		= ocfs2_readahead,
	.writepage		= ocfs2_writepage,
	.write_begin		= ocfs2_write_begin,
	.write_end		= ocfs2_write_end,
	.bmap			= ocfs2_bmap,
	.direct_IO		= ocfs2_direct_IO,
	.invalidate_folio	= block_invalidate_folio,
	.release_folio		= ocfs2_release_folio,
	.migrate_folio		= buffer_migrate_folio,
	.is_partially_uptodate	= block_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
};
