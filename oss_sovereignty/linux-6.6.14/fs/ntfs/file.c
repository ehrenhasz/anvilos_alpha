
 

#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/buffer_head.h>
#include <linux/gfp.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/sched/signal.h>
#include <linux/swap.h>
#include <linux/uio.h>
#include <linux/writeback.h>

#include <asm/page.h>
#include <linux/uaccess.h>

#include "attrib.h"
#include "bitmap.h"
#include "inode.h"
#include "debug.h"
#include "lcnalloc.h"
#include "malloc.h"
#include "mft.h"
#include "ntfs.h"

 
static int ntfs_file_open(struct inode *vi, struct file *filp)
{
	if (sizeof(unsigned long) < 8) {
		if (i_size_read(vi) > MAX_LFS_FILESIZE)
			return -EOVERFLOW;
	}
	return generic_file_open(vi, filp);
}

#ifdef NTFS_RW

 
static int ntfs_attr_extend_initialized(ntfs_inode *ni, const s64 new_init_size)
{
	s64 old_init_size;
	loff_t old_i_size;
	pgoff_t index, end_index;
	unsigned long flags;
	struct inode *vi = VFS_I(ni);
	ntfs_inode *base_ni;
	MFT_RECORD *m = NULL;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx = NULL;
	struct address_space *mapping;
	struct page *page = NULL;
	u8 *kattr;
	int err;
	u32 attr_len;

	read_lock_irqsave(&ni->size_lock, flags);
	old_init_size = ni->initialized_size;
	old_i_size = i_size_read(vi);
	BUG_ON(new_init_size > ni->allocated_size);
	read_unlock_irqrestore(&ni->size_lock, flags);
	ntfs_debug("Entering for i_ino 0x%lx, attribute type 0x%x, "
			"old_initialized_size 0x%llx, "
			"new_initialized_size 0x%llx, i_size 0x%llx.",
			vi->i_ino, (unsigned)le32_to_cpu(ni->type),
			(unsigned long long)old_init_size,
			(unsigned long long)new_init_size, old_i_size);
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	 
	if (NInoNonResident(ni))
		goto do_non_resident_extend;
	BUG_ON(old_init_size != old_i_size);
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		m = NULL;
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto err_out;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT)
			err = -EIO;
		goto err_out;
	}
	m = ctx->mrec;
	a = ctx->attr;
	BUG_ON(a->non_resident);
	 
	attr_len = le32_to_cpu(a->data.resident.value_length);
	BUG_ON(old_i_size != (loff_t)attr_len);
	 
	kattr = (u8*)a + le16_to_cpu(a->data.resident.value_offset);
	memset(kattr + attr_len, 0, new_init_size - attr_len);
	a->data.resident.value_length = cpu_to_le32((u32)new_init_size);
	 
	write_lock_irqsave(&ni->size_lock, flags);
	i_size_write(vi, new_init_size);
	ni->initialized_size = new_init_size;
	write_unlock_irqrestore(&ni->size_lock, flags);
	goto done;
do_non_resident_extend:
	 
	if (new_init_size > old_i_size) {
		m = map_mft_record(base_ni);
		if (IS_ERR(m)) {
			err = PTR_ERR(m);
			m = NULL;
			goto err_out;
		}
		ctx = ntfs_attr_get_search_ctx(base_ni, m);
		if (unlikely(!ctx)) {
			err = -ENOMEM;
			goto err_out;
		}
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
				CASE_SENSITIVE, 0, NULL, 0, ctx);
		if (unlikely(err)) {
			if (err == -ENOENT)
				err = -EIO;
			goto err_out;
		}
		m = ctx->mrec;
		a = ctx->attr;
		BUG_ON(!a->non_resident);
		BUG_ON(old_i_size != (loff_t)
				sle64_to_cpu(a->data.non_resident.data_size));
		a->data.non_resident.data_size = cpu_to_sle64(new_init_size);
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
		 
		i_size_write(vi, new_init_size);
		ntfs_attr_put_search_ctx(ctx);
		ctx = NULL;
		unmap_mft_record(base_ni);
		m = NULL;
	}
	mapping = vi->i_mapping;
	index = old_init_size >> PAGE_SHIFT;
	end_index = (new_init_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	do {
		 
		page = read_mapping_page(mapping, index, NULL);
		if (IS_ERR(page)) {
			err = PTR_ERR(page);
			goto init_err_out;
		}
		 
		write_lock_irqsave(&ni->size_lock, flags);
		ni->initialized_size = (s64)(index + 1) << PAGE_SHIFT;
		if (ni->initialized_size > new_init_size)
			ni->initialized_size = new_init_size;
		write_unlock_irqrestore(&ni->size_lock, flags);
		 
		set_page_dirty(page);
		put_page(page);
		 
		balance_dirty_pages_ratelimited(mapping);
		cond_resched();
	} while (++index < end_index);
	read_lock_irqsave(&ni->size_lock, flags);
	BUG_ON(ni->initialized_size != new_init_size);
	read_unlock_irqrestore(&ni->size_lock, flags);
	 
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		m = NULL;
		goto init_err_out;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto init_err_out;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT)
			err = -EIO;
		goto init_err_out;
	}
	m = ctx->mrec;
	a = ctx->attr;
	BUG_ON(!a->non_resident);
	a->data.non_resident.initialized_size = cpu_to_sle64(new_init_size);
done:
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
	ntfs_debug("Done, initialized_size 0x%llx, i_size 0x%llx.",
			(unsigned long long)new_init_size, i_size_read(vi));
	return 0;
init_err_out:
	write_lock_irqsave(&ni->size_lock, flags);
	ni->initialized_size = old_init_size;
	write_unlock_irqrestore(&ni->size_lock, flags);
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
	ntfs_debug("Failed.  Returning error code %i.", err);
	return err;
}

static ssize_t ntfs_prepare_file_for_write(struct kiocb *iocb,
		struct iov_iter *from)
{
	loff_t pos;
	s64 end, ll;
	ssize_t err;
	unsigned long flags;
	struct file *file = iocb->ki_filp;
	struct inode *vi = file_inode(file);
	ntfs_inode *ni = NTFS_I(vi);
	ntfs_volume *vol = ni->vol;

	ntfs_debug("Entering for i_ino 0x%lx, attribute type 0x%x, pos "
			"0x%llx, count 0x%zx.", vi->i_ino,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned long long)iocb->ki_pos,
			iov_iter_count(from));
	err = generic_write_checks(iocb, from);
	if (unlikely(err <= 0))
		goto out;
	 
	BUG_ON(NInoMstProtected(ni));
	BUG_ON(ni->type != AT_DATA);
	 
	if (NInoEncrypted(ni)) {
		 
		 
		ntfs_debug("Denying write access to encrypted file.");
		err = -EACCES;
		goto out;
	}
	if (NInoCompressed(ni)) {
		 
		BUG_ON(ni->name_len);
		 
		ntfs_error(vi->i_sb, "Writing to compressed files is not "
				"implemented yet.  Sorry.");
		err = -EOPNOTSUPP;
		goto out;
	}
	err = file_remove_privs(file);
	if (unlikely(err))
		goto out;
	 
	file_update_time(file);
	pos = iocb->ki_pos;
	 
	end = (pos + iov_iter_count(from) + vol->cluster_size_mask) &
			~(u64)vol->cluster_size_mask;
	 
	read_lock_irqsave(&ni->size_lock, flags);
	ll = ni->allocated_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	if (end > ll) {
		 
		ll = ntfs_attr_extend_allocation(ni, end, -1, pos);
		if (likely(ll >= 0)) {
			BUG_ON(pos >= ll);
			 
			if (end > ll) {
				ntfs_debug("Truncating write to inode 0x%lx, "
						"attribute type 0x%x, because "
						"the allocation was only "
						"partially extended.",
						vi->i_ino, (unsigned)
						le32_to_cpu(ni->type));
				iov_iter_truncate(from, ll - pos);
			}
		} else {
			err = ll;
			read_lock_irqsave(&ni->size_lock, flags);
			ll = ni->allocated_size;
			read_unlock_irqrestore(&ni->size_lock, flags);
			 
			if (pos < ll) {
				ntfs_debug("Truncating write to inode 0x%lx "
						"attribute type 0x%x, because "
						"extending the allocation "
						"failed (error %d).",
						vi->i_ino, (unsigned)
						le32_to_cpu(ni->type),
						(int)-err);
				iov_iter_truncate(from, ll - pos);
			} else {
				if (err != -ENOSPC)
					ntfs_error(vi->i_sb, "Cannot perform "
							"write to inode "
							"0x%lx, attribute "
							"type 0x%x, because "
							"extending the "
							"allocation failed "
							"(error %ld).",
							vi->i_ino, (unsigned)
							le32_to_cpu(ni->type),
							(long)-err);
				else
					ntfs_debug("Cannot perform write to "
							"inode 0x%lx, "
							"attribute type 0x%x, "
							"because there is not "
							"space left.",
							vi->i_ino, (unsigned)
							le32_to_cpu(ni->type));
				goto out;
			}
		}
	}
	 
	read_lock_irqsave(&ni->size_lock, flags);
	ll = ni->initialized_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	if (pos > ll) {
		 
		inode_dio_wait(vi);
		err = ntfs_attr_extend_initialized(ni, pos);
		if (unlikely(err < 0))
			ntfs_error(vi->i_sb, "Cannot perform write to inode "
					"0x%lx, attribute type 0x%x, because "
					"extending the initialized size "
					"failed (error %d).", vi->i_ino,
					(unsigned)le32_to_cpu(ni->type),
					(int)-err);
	}
out:
	return err;
}

 
static inline int __ntfs_grab_cache_pages(struct address_space *mapping,
		pgoff_t index, const unsigned nr_pages, struct page **pages,
		struct page **cached_page)
{
	int err, nr;

	BUG_ON(!nr_pages);
	err = nr = 0;
	do {
		pages[nr] = find_get_page_flags(mapping, index, FGP_LOCK |
				FGP_ACCESSED);
		if (!pages[nr]) {
			if (!*cached_page) {
				*cached_page = page_cache_alloc(mapping);
				if (unlikely(!*cached_page)) {
					err = -ENOMEM;
					goto err_out;
				}
			}
			err = add_to_page_cache_lru(*cached_page, mapping,
				   index,
				   mapping_gfp_constraint(mapping, GFP_KERNEL));
			if (unlikely(err)) {
				if (err == -EEXIST)
					continue;
				goto err_out;
			}
			pages[nr] = *cached_page;
			*cached_page = NULL;
		}
		index++;
		nr++;
	} while (nr < nr_pages);
out:
	return err;
err_out:
	while (nr > 0) {
		unlock_page(pages[--nr]);
		put_page(pages[nr]);
	}
	goto out;
}

static inline void ntfs_submit_bh_for_read(struct buffer_head *bh)
{
	lock_buffer(bh);
	get_bh(bh);
	bh->b_end_io = end_buffer_read_sync;
	submit_bh(REQ_OP_READ, bh);
}

 
static int ntfs_prepare_pages_for_non_resident_write(struct page **pages,
		unsigned nr_pages, s64 pos, size_t bytes)
{
	VCN vcn, highest_vcn = 0, cpos, cend, bh_cpos, bh_cend;
	LCN lcn;
	s64 bh_pos, vcn_len, end, initialized_size;
	sector_t lcn_block;
	struct page *page;
	struct inode *vi;
	ntfs_inode *ni, *base_ni = NULL;
	ntfs_volume *vol;
	runlist_element *rl, *rl2;
	struct buffer_head *bh, *head, *wait[2], **wait_bh = wait;
	ntfs_attr_search_ctx *ctx = NULL;
	MFT_RECORD *m = NULL;
	ATTR_RECORD *a = NULL;
	unsigned long flags;
	u32 attr_rec_len = 0;
	unsigned blocksize, u;
	int err, mp_size;
	bool rl_write_locked, was_hole, is_retry;
	unsigned char blocksize_bits;
	struct {
		u8 runlist_merged:1;
		u8 mft_attr_mapped:1;
		u8 mp_rebuilt:1;
		u8 attr_switched:1;
	} status = { 0, 0, 0, 0 };

	BUG_ON(!nr_pages);
	BUG_ON(!pages);
	BUG_ON(!*pages);
	vi = pages[0]->mapping->host;
	ni = NTFS_I(vi);
	vol = ni->vol;
	ntfs_debug("Entering for inode 0x%lx, attribute type 0x%x, start page "
			"index 0x%lx, nr_pages 0x%x, pos 0x%llx, bytes 0x%zx.",
			vi->i_ino, ni->type, pages[0]->index, nr_pages,
			(long long)pos, bytes);
	blocksize = vol->sb->s_blocksize;
	blocksize_bits = vol->sb->s_blocksize_bits;
	u = 0;
	do {
		page = pages[u];
		BUG_ON(!page);
		 
		if (!page_has_buffers(page)) {
			create_empty_buffers(page, blocksize, 0);
			if (unlikely(!page_has_buffers(page)))
				return -ENOMEM;
		}
	} while (++u < nr_pages);
	rl_write_locked = false;
	rl = NULL;
	err = 0;
	vcn = lcn = -1;
	vcn_len = 0;
	lcn_block = -1;
	was_hole = false;
	cpos = pos >> vol->cluster_size_bits;
	end = pos + bytes;
	cend = (end + vol->cluster_size - 1) >> vol->cluster_size_bits;
	 
	u = 0;
do_next_page:
	page = pages[u];
	bh_pos = (s64)page->index << PAGE_SHIFT;
	bh = head = page_buffers(page);
	do {
		VCN cdelta;
		s64 bh_end;
		unsigned bh_cofs;

		 
		if (buffer_new(bh))
			clear_buffer_new(bh);
		bh_end = bh_pos + blocksize;
		bh_cpos = bh_pos >> vol->cluster_size_bits;
		bh_cofs = bh_pos & vol->cluster_size_mask;
		if (buffer_mapped(bh)) {
			 
			if (buffer_uptodate(bh))
				continue;
			 
			if (PageUptodate(page)) {
				set_buffer_uptodate(bh);
				continue;
			}
			 
			if ((bh_pos < pos && bh_end > pos) ||
					(bh_pos < end && bh_end > end)) {
				 
				read_lock_irqsave(&ni->size_lock, flags);
				initialized_size = ni->initialized_size;
				read_unlock_irqrestore(&ni->size_lock, flags);
				if (bh_pos < initialized_size) {
					ntfs_submit_bh_for_read(bh);
					*wait_bh++ = bh;
				} else {
					zero_user(page, bh_offset(bh),
							blocksize);
					set_buffer_uptodate(bh);
				}
			}
			continue;
		}
		 
		bh->b_bdev = vol->sb->s_bdev;
		 
		cdelta = bh_cpos - vcn;
		if (likely(!cdelta || (cdelta > 0 && cdelta < vcn_len))) {
map_buffer_cached:
			BUG_ON(lcn < 0);
			bh->b_blocknr = lcn_block +
					(cdelta << (vol->cluster_size_bits -
					blocksize_bits)) +
					(bh_cofs >> blocksize_bits);
			set_buffer_mapped(bh);
			 
			if (PageUptodate(page)) {
				if (!buffer_uptodate(bh))
					set_buffer_uptodate(bh);
				if (unlikely(was_hole)) {
					 
					clean_bdev_bh_alias(bh);
					if (bh_end <= pos || bh_pos >= end)
						mark_buffer_dirty(bh);
					else
						set_buffer_new(bh);
				}
				continue;
			}
			 
			if (likely(!was_hole)) {
				 
				if (!buffer_uptodate(bh) && bh_pos < end &&
						bh_end > pos &&
						(bh_pos < pos ||
						bh_end > end)) {
					 
					read_lock_irqsave(&ni->size_lock,
							flags);
					initialized_size = ni->initialized_size;
					read_unlock_irqrestore(&ni->size_lock,
							flags);
					if (bh_pos < initialized_size) {
						ntfs_submit_bh_for_read(bh);
						*wait_bh++ = bh;
					} else {
						zero_user(page, bh_offset(bh),
								blocksize);
						set_buffer_uptodate(bh);
					}
				}
				continue;
			}
			 
			clean_bdev_bh_alias(bh);
			 
			if (bh_end <= pos || bh_pos >= end) {
				if (!buffer_uptodate(bh)) {
					zero_user(page, bh_offset(bh),
							blocksize);
					set_buffer_uptodate(bh);
				}
				mark_buffer_dirty(bh);
				continue;
			}
			set_buffer_new(bh);
			if (!buffer_uptodate(bh) &&
					(bh_pos < pos || bh_end > end)) {
				u8 *kaddr;
				unsigned pofs;
					
				kaddr = kmap_atomic(page);
				if (bh_pos < pos) {
					pofs = bh_pos & ~PAGE_MASK;
					memset(kaddr + pofs, 0, pos - bh_pos);
				}
				if (bh_end > end) {
					pofs = end & ~PAGE_MASK;
					memset(kaddr + pofs, 0, bh_end - end);
				}
				kunmap_atomic(kaddr);
				flush_dcache_page(page);
			}
			continue;
		}
		 
		read_lock_irqsave(&ni->size_lock, flags);
		initialized_size = ni->allocated_size;
		read_unlock_irqrestore(&ni->size_lock, flags);
		if (bh_pos > initialized_size) {
			if (PageUptodate(page)) {
				if (!buffer_uptodate(bh))
					set_buffer_uptodate(bh);
			} else if (!buffer_uptodate(bh)) {
				zero_user(page, bh_offset(bh), blocksize);
				set_buffer_uptodate(bh);
			}
			continue;
		}
		is_retry = false;
		if (!rl) {
			down_read(&ni->runlist.lock);
retry_remap:
			rl = ni->runlist.rl;
		}
		if (likely(rl != NULL)) {
			 
			while (rl->length && rl[1].vcn <= bh_cpos)
				rl++;
			lcn = ntfs_rl_vcn_to_lcn(rl, bh_cpos);
			if (likely(lcn >= 0)) {
				 
				was_hole = false;
				vcn = bh_cpos;
				vcn_len = rl[1].vcn - vcn;
				lcn_block = lcn << (vol->cluster_size_bits -
						blocksize_bits);
				cdelta = 0;
				 
				if (likely(vcn + vcn_len >= cend)) {
					if (rl_write_locked) {
						up_write(&ni->runlist.lock);
						rl_write_locked = false;
					} else
						up_read(&ni->runlist.lock);
					rl = NULL;
				}
				goto map_buffer_cached;
			}
		} else
			lcn = LCN_RL_NOT_MAPPED;
		 
		if (unlikely(lcn != LCN_HOLE && lcn != LCN_ENOENT)) {
			if (likely(!is_retry && lcn == LCN_RL_NOT_MAPPED)) {
				 
				if (!rl_write_locked) {
					 
					up_read(&ni->runlist.lock);
					down_write(&ni->runlist.lock);
					rl_write_locked = true;
					goto retry_remap;
				}
				err = ntfs_map_runlist_nolock(ni, bh_cpos,
						NULL);
				if (likely(!err)) {
					is_retry = true;
					goto retry_remap;
				}
				 
				if (err == -ENOENT) {
					lcn = LCN_ENOENT;
					err = 0;
					goto rl_not_mapped_enoent;
				}
			} else
				err = -EIO;
			 
			bh->b_blocknr = -1;
			ntfs_error(vol->sb, "Failed to write to inode 0x%lx, "
					"attribute type 0x%x, vcn 0x%llx, "
					"vcn offset 0x%x, because its "
					"location on disk could not be "
					"determined%s (error code %i).",
					ni->mft_no, ni->type,
					(unsigned long long)bh_cpos,
					(unsigned)bh_pos &
					vol->cluster_size_mask,
					is_retry ? " even after retrying" : "",
					err);
			break;
		}
rl_not_mapped_enoent:
		 
		if (unlikely(vol->cluster_size < PAGE_SIZE)) {
			bh_cend = (bh_end + vol->cluster_size - 1) >>
					vol->cluster_size_bits;
			if ((bh_cend <= cpos || bh_cpos >= cend)) {
				bh->b_blocknr = -1;
				 
				if (PageUptodate(page)) {
					if (!buffer_uptodate(bh))
						set_buffer_uptodate(bh);
				} else if (!buffer_uptodate(bh)) {
					zero_user(page, bh_offset(bh),
						blocksize);
					set_buffer_uptodate(bh);
				}
				continue;
			}
		}
		 
		BUG_ON(lcn != LCN_HOLE);
		 
		BUG_ON(!rl);
		if (!rl_write_locked) {
			up_read(&ni->runlist.lock);
			down_write(&ni->runlist.lock);
			rl_write_locked = true;
			goto retry_remap;
		}
		 
		BUG_ON(rl->lcn != LCN_HOLE);
		lcn = -1;
		rl2 = rl;
		while (--rl2 >= ni->runlist.rl) {
			if (rl2->lcn >= 0) {
				lcn = rl2->lcn + rl2->length;
				break;
			}
		}
		rl2 = ntfs_cluster_alloc(vol, bh_cpos, 1, lcn, DATA_ZONE,
				false);
		if (IS_ERR(rl2)) {
			err = PTR_ERR(rl2);
			ntfs_debug("Failed to allocate cluster, error code %i.",
					err);
			break;
		}
		lcn = rl2->lcn;
		rl = ntfs_runlists_merge(ni->runlist.rl, rl2);
		if (IS_ERR(rl)) {
			err = PTR_ERR(rl);
			if (err != -ENOMEM)
				err = -EIO;
			if (ntfs_cluster_free_from_rl(vol, rl2)) {
				ntfs_error(vol->sb, "Failed to release "
						"allocated cluster in error "
						"code path.  Run chkdsk to "
						"recover the lost cluster.");
				NVolSetErrors(vol);
			}
			ntfs_free(rl2);
			break;
		}
		ni->runlist.rl = rl;
		status.runlist_merged = 1;
		ntfs_debug("Allocated cluster, lcn 0x%llx.",
				(unsigned long long)lcn);
		 
		if (!NInoAttr(ni))
			base_ni = ni;
		else
			base_ni = ni->ext.base_ntfs_ino;
		m = map_mft_record(base_ni);
		if (IS_ERR(m)) {
			err = PTR_ERR(m);
			break;
		}
		ctx = ntfs_attr_get_search_ctx(base_ni, m);
		if (unlikely(!ctx)) {
			err = -ENOMEM;
			unmap_mft_record(base_ni);
			break;
		}
		status.mft_attr_mapped = 1;
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
				CASE_SENSITIVE, bh_cpos, NULL, 0, ctx);
		if (unlikely(err)) {
			if (err == -ENOENT)
				err = -EIO;
			break;
		}
		m = ctx->mrec;
		a = ctx->attr;
		 
		vcn = sle64_to_cpu(a->data.non_resident.lowest_vcn);
		rl2 = ntfs_rl_find_vcn_nolock(rl, vcn);
		BUG_ON(!rl2);
		BUG_ON(!rl2->length);
		BUG_ON(rl2->lcn < LCN_HOLE);
		highest_vcn = sle64_to_cpu(a->data.non_resident.highest_vcn);
		 
		if (!highest_vcn)
			highest_vcn = (sle64_to_cpu(
					a->data.non_resident.allocated_size) >>
					vol->cluster_size_bits) - 1;
		 
		mp_size = ntfs_get_size_for_mapping_pairs(vol, rl2, vcn,
				highest_vcn);
		if (unlikely(mp_size <= 0)) {
			if (!(err = mp_size))
				err = -EIO;
			ntfs_debug("Failed to get size for mapping pairs "
					"array, error code %i.", err);
			break;
		}
		 
		attr_rec_len = le32_to_cpu(a->length);
		err = ntfs_attr_record_resize(m, a, mp_size + le16_to_cpu(
				a->data.non_resident.mapping_pairs_offset));
		if (unlikely(err)) {
			BUG_ON(err != -ENOSPC);
			 
			 
			 
			 
			 
			 
			 
			 
			 
			 
			 
			 
			ntfs_error(vol->sb, "Not enough space in the mft "
					"record for the extended attribute "
					"record.  This case is not "
					"implemented yet.");
			err = -EOPNOTSUPP;
			break ;
		}
		status.mp_rebuilt = 1;
		 
		err = ntfs_mapping_pairs_build(vol, (u8*)a + le16_to_cpu(
				a->data.non_resident.mapping_pairs_offset),
				mp_size, rl2, vcn, highest_vcn, NULL);
		if (unlikely(err)) {
			ntfs_error(vol->sb, "Cannot fill hole in inode 0x%lx, "
					"attribute type 0x%x, because building "
					"the mapping pairs failed with error "
					"code %i.", vi->i_ino,
					(unsigned)le32_to_cpu(ni->type), err);
			err = -EIO;
			break;
		}
		 
		if (unlikely(!a->data.non_resident.highest_vcn))
			a->data.non_resident.highest_vcn =
					cpu_to_sle64(highest_vcn);
		 
		if (likely(NInoSparse(ni) || NInoCompressed(ni))) {
			 
			if (a->data.non_resident.lowest_vcn) {
				flush_dcache_mft_record_page(ctx->ntfs_ino);
				mark_mft_record_dirty(ctx->ntfs_ino);
				ntfs_attr_reinit_search_ctx(ctx);
				err = ntfs_attr_lookup(ni->type, ni->name,
						ni->name_len, CASE_SENSITIVE,
						0, NULL, 0, ctx);
				if (unlikely(err)) {
					status.attr_switched = 1;
					break;
				}
				 
				a = ctx->attr;
			}
			write_lock_irqsave(&ni->size_lock, flags);
			ni->itype.compressed.size += vol->cluster_size;
			a->data.non_resident.compressed_size =
					cpu_to_sle64(ni->itype.compressed.size);
			write_unlock_irqrestore(&ni->size_lock, flags);
		}
		 
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(base_ni);
		 
		status.runlist_merged = 0;
		status.mft_attr_mapped = 0;
		status.mp_rebuilt = 0;
		 
		was_hole = true;
		vcn = bh_cpos;
		vcn_len = 1;
		lcn_block = lcn << (vol->cluster_size_bits - blocksize_bits);
		cdelta = 0;
		 
		if (likely(vcn + vcn_len >= cend)) {
			up_write(&ni->runlist.lock);
			rl_write_locked = false;
			rl = NULL;
		}
		goto map_buffer_cached;
	} while (bh_pos += blocksize, (bh = bh->b_this_page) != head);
	 
	if (likely(!err && ++u < nr_pages))
		goto do_next_page;
	 
	if (likely(!err)) {
		if (unlikely(rl_write_locked)) {
			up_write(&ni->runlist.lock);
			rl_write_locked = false;
		} else if (unlikely(rl))
			up_read(&ni->runlist.lock);
		rl = NULL;
	}
	 
	read_lock_irqsave(&ni->size_lock, flags);
	initialized_size = ni->initialized_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	while (wait_bh > wait) {
		bh = *--wait_bh;
		wait_on_buffer(bh);
		if (likely(buffer_uptodate(bh))) {
			page = bh->b_page;
			bh_pos = ((s64)page->index << PAGE_SHIFT) +
					bh_offset(bh);
			 
			if (unlikely(bh_pos + blocksize > initialized_size)) {
				int ofs = 0;

				if (likely(bh_pos < initialized_size))
					ofs = initialized_size - bh_pos;
				zero_user_segment(page, bh_offset(bh) + ofs,
						blocksize);
			}
		} else  
			err = -EIO;
	}
	if (likely(!err)) {
		 
		u = 0;
		do {
			bh = head = page_buffers(pages[u]);
			do {
				if (buffer_new(bh))
					clear_buffer_new(bh);
			} while ((bh = bh->b_this_page) != head);
		} while (++u < nr_pages);
		ntfs_debug("Done.");
		return err;
	}
	if (status.attr_switched) {
		 
		ntfs_attr_reinit_search_ctx(ctx);
		if (ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
				CASE_SENSITIVE, bh_cpos, NULL, 0, ctx)) {
			ntfs_error(vol->sb, "Failed to find required "
					"attribute extent of attribute in "
					"error code path.  Run chkdsk to "
					"recover.");
			write_lock_irqsave(&ni->size_lock, flags);
			ni->itype.compressed.size += vol->cluster_size;
			write_unlock_irqrestore(&ni->size_lock, flags);
			flush_dcache_mft_record_page(ctx->ntfs_ino);
			mark_mft_record_dirty(ctx->ntfs_ino);
			 
			NVolSetErrors(vol);
		} else {
			m = ctx->mrec;
			a = ctx->attr;
			status.attr_switched = 0;
		}
	}
	 
	if (status.runlist_merged && !status.attr_switched) {
		BUG_ON(!rl_write_locked);
		 
		if (ntfs_rl_punch_nolock(vol, &ni->runlist, bh_cpos, 1)) {
			ntfs_error(vol->sb, "Failed to punch hole into "
					"attribute runlist in error code "
					"path.  Run chkdsk to recover the "
					"lost cluster.");
			NVolSetErrors(vol);
		} else   {
			status.runlist_merged = 0;
			 
			down_write(&vol->lcnbmp_lock);
			if (ntfs_bitmap_clear_bit(vol->lcnbmp_ino, lcn)) {
				ntfs_error(vol->sb, "Failed to release "
						"allocated cluster in error "
						"code path.  Run chkdsk to "
						"recover the lost cluster.");
				NVolSetErrors(vol);
			}
			up_write(&vol->lcnbmp_lock);
		}
	}
	 
	if (status.mp_rebuilt && !status.runlist_merged) {
		if (ntfs_attr_record_resize(m, a, attr_rec_len)) {
			ntfs_error(vol->sb, "Failed to restore attribute "
					"record in error code path.  Run "
					"chkdsk to recover.");
			NVolSetErrors(vol);
		} else   {
			if (ntfs_mapping_pairs_build(vol, (u8*)a +
					le16_to_cpu(a->data.non_resident.
					mapping_pairs_offset), attr_rec_len -
					le16_to_cpu(a->data.non_resident.
					mapping_pairs_offset), ni->runlist.rl,
					vcn, highest_vcn, NULL)) {
				ntfs_error(vol->sb, "Failed to restore "
						"mapping pairs array in error "
						"code path.  Run chkdsk to "
						"recover.");
				NVolSetErrors(vol);
			}
			flush_dcache_mft_record_page(ctx->ntfs_ino);
			mark_mft_record_dirty(ctx->ntfs_ino);
		}
	}
	 
	if (status.mft_attr_mapped) {
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(base_ni);
	}
	 
	if (rl_write_locked)
		up_write(&ni->runlist.lock);
	else if (rl)
		up_read(&ni->runlist.lock);
	 
	nr_pages = u;
	u = 0;
	end = bh_cpos << vol->cluster_size_bits;
	do {
		page = pages[u];
		bh = head = page_buffers(page);
		do {
			if (u == nr_pages &&
					((s64)page->index << PAGE_SHIFT) +
					bh_offset(bh) >= end)
				break;
			if (!buffer_new(bh))
				continue;
			clear_buffer_new(bh);
			if (!buffer_uptodate(bh)) {
				if (PageUptodate(page))
					set_buffer_uptodate(bh);
				else {
					zero_user(page, bh_offset(bh),
							blocksize);
					set_buffer_uptodate(bh);
				}
			}
			mark_buffer_dirty(bh);
		} while ((bh = bh->b_this_page) != head);
	} while (++u <= nr_pages);
	ntfs_error(vol->sb, "Failed.  Returning error code %i.", err);
	return err;
}

static inline void ntfs_flush_dcache_pages(struct page **pages,
		unsigned nr_pages)
{
	BUG_ON(!nr_pages);
	 
	do {
		--nr_pages;
		flush_dcache_page(pages[nr_pages]);
	} while (nr_pages > 0);
}

 
static inline int ntfs_commit_pages_after_non_resident_write(
		struct page **pages, const unsigned nr_pages,
		s64 pos, size_t bytes)
{
	s64 end, initialized_size;
	struct inode *vi;
	ntfs_inode *ni, *base_ni;
	struct buffer_head *bh, *head;
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	unsigned long flags;
	unsigned blocksize, u;
	int err;

	vi = pages[0]->mapping->host;
	ni = NTFS_I(vi);
	blocksize = vi->i_sb->s_blocksize;
	end = pos + bytes;
	u = 0;
	do {
		s64 bh_pos;
		struct page *page;
		bool partial;

		page = pages[u];
		bh_pos = (s64)page->index << PAGE_SHIFT;
		bh = head = page_buffers(page);
		partial = false;
		do {
			s64 bh_end;

			bh_end = bh_pos + blocksize;
			if (bh_end <= pos || bh_pos >= end) {
				if (!buffer_uptodate(bh))
					partial = true;
			} else {
				set_buffer_uptodate(bh);
				mark_buffer_dirty(bh);
			}
		} while (bh_pos += blocksize, (bh = bh->b_this_page) != head);
		 
		if (!partial && !PageUptodate(page))
			SetPageUptodate(page);
	} while (++u < nr_pages);
	 
	read_lock_irqsave(&ni->size_lock, flags);
	initialized_size = ni->initialized_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	if (end <= initialized_size) {
		ntfs_debug("Done.");
		return 0;
	}
	 
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	 
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		m = NULL;
		ctx = NULL;
		goto err_out;
	}
	BUG_ON(!NInoNonResident(ni));
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto err_out;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT)
			err = -EIO;
		goto err_out;
	}
	a = ctx->attr;
	BUG_ON(!a->non_resident);
	write_lock_irqsave(&ni->size_lock, flags);
	BUG_ON(end > ni->allocated_size);
	ni->initialized_size = end;
	a->data.non_resident.initialized_size = cpu_to_sle64(end);
	if (end > i_size_read(vi)) {
		i_size_write(vi, end);
		a->data.non_resident.data_size =
				a->data.non_resident.initialized_size;
	}
	write_unlock_irqrestore(&ni->size_lock, flags);
	 
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	ntfs_debug("Done.");
	return 0;
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
	ntfs_error(vi->i_sb, "Failed to update initialized_size/i_size (error "
			"code %i).", err);
	if (err != -ENOMEM)
		NVolSetErrors(ni->vol);
	return err;
}

 
static int ntfs_commit_pages_after_write(struct page **pages,
		const unsigned nr_pages, s64 pos, size_t bytes)
{
	s64 end, initialized_size;
	loff_t i_size;
	struct inode *vi;
	ntfs_inode *ni, *base_ni;
	struct page *page;
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	char *kattr, *kaddr;
	unsigned long flags;
	u32 attr_len;
	int err;

	BUG_ON(!nr_pages);
	BUG_ON(!pages);
	page = pages[0];
	BUG_ON(!page);
	vi = page->mapping->host;
	ni = NTFS_I(vi);
	ntfs_debug("Entering for inode 0x%lx, attribute type 0x%x, start page "
			"index 0x%lx, nr_pages 0x%x, pos 0x%llx, bytes 0x%zx.",
			vi->i_ino, ni->type, page->index, nr_pages,
			(long long)pos, bytes);
	if (NInoNonResident(ni))
		return ntfs_commit_pages_after_non_resident_write(pages,
				nr_pages, pos, bytes);
	BUG_ON(nr_pages > 1);
	 
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	BUG_ON(NInoNonResident(ni));
	 
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		m = NULL;
		ctx = NULL;
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto err_out;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT)
			err = -EIO;
		goto err_out;
	}
	a = ctx->attr;
	BUG_ON(a->non_resident);
	 
	attr_len = le32_to_cpu(a->data.resident.value_length);
	i_size = i_size_read(vi);
	BUG_ON(attr_len != i_size);
	BUG_ON(pos > attr_len);
	end = pos + bytes;
	BUG_ON(end > le32_to_cpu(a->length) -
			le16_to_cpu(a->data.resident.value_offset));
	kattr = (u8*)a + le16_to_cpu(a->data.resident.value_offset);
	kaddr = kmap_atomic(page);
	 
	memcpy(kattr + pos, kaddr + pos, bytes);
	 
	if (end > attr_len) {
		attr_len = end;
		a->data.resident.value_length = cpu_to_le32(attr_len);
	}
	 
	if (!PageUptodate(page)) {
		if (pos > 0)
			memcpy(kaddr, kattr, pos);
		if (end < attr_len)
			memcpy(kaddr + end, kattr + end, attr_len - end);
		 
		memset(kaddr + attr_len, 0, PAGE_SIZE - attr_len);
		flush_dcache_page(page);
		SetPageUptodate(page);
	}
	kunmap_atomic(kaddr);
	 
	read_lock_irqsave(&ni->size_lock, flags);
	initialized_size = ni->initialized_size;
	BUG_ON(end > ni->allocated_size);
	read_unlock_irqrestore(&ni->size_lock, flags);
	BUG_ON(initialized_size != i_size);
	if (end > initialized_size) {
		write_lock_irqsave(&ni->size_lock, flags);
		ni->initialized_size = end;
		i_size_write(vi, end);
		write_unlock_irqrestore(&ni->size_lock, flags);
	}
	 
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	ntfs_debug("Done.");
	return 0;
err_out:
	if (err == -ENOMEM) {
		ntfs_warning(vi->i_sb, "Error allocating memory required to "
				"commit the write.");
		if (PageUptodate(page)) {
			ntfs_warning(vi->i_sb, "Page is uptodate, setting "
					"dirty so the write will be retried "
					"later on by the VM.");
			 
			__set_page_dirty_nobuffers(page);
			err = 0;
		} else
			ntfs_error(vi->i_sb, "Page is not uptodate.  Written "
					"data has been lost.");
	} else {
		ntfs_error(vi->i_sb, "Resident attribute commit write failed "
				"with error %i.", err);
		NVolSetErrors(ni->vol);
	}
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
	return err;
}

 
static size_t ntfs_copy_from_user_iter(struct page **pages, unsigned nr_pages,
		unsigned ofs, struct iov_iter *i, size_t bytes)
{
	struct page **last_page = pages + nr_pages;
	size_t total = 0;
	unsigned len, copied;

	do {
		len = PAGE_SIZE - ofs;
		if (len > bytes)
			len = bytes;
		copied = copy_page_from_iter_atomic(*pages, ofs, len, i);
		total += copied;
		bytes -= copied;
		if (!bytes)
			break;
		if (copied < len)
			goto err;
		ofs = 0;
	} while (++pages < last_page);
out:
	return total;
err:
	 
	len = PAGE_SIZE - copied;
	do {
		if (len > bytes)
			len = bytes;
		zero_user(*pages, copied, len);
		bytes -= len;
		copied = 0;
		len = PAGE_SIZE;
	} while (++pages < last_page);
	goto out;
}

 
static ssize_t ntfs_perform_write(struct file *file, struct iov_iter *i,
		loff_t pos)
{
	struct address_space *mapping = file->f_mapping;
	struct inode *vi = mapping->host;
	ntfs_inode *ni = NTFS_I(vi);
	ntfs_volume *vol = ni->vol;
	struct page *pages[NTFS_MAX_PAGES_PER_CLUSTER];
	struct page *cached_page = NULL;
	VCN last_vcn;
	LCN lcn;
	size_t bytes;
	ssize_t status, written = 0;
	unsigned nr_pages;

	ntfs_debug("Entering for i_ino 0x%lx, attribute type 0x%x, pos "
			"0x%llx, count 0x%lx.", vi->i_ino,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned long long)pos,
			(unsigned long)iov_iter_count(i));
	 
	if (unlikely(NInoTruncateFailed(ni))) {
		int err;

		inode_dio_wait(vi);
		err = ntfs_truncate(vi);
		if (err || NInoTruncateFailed(ni)) {
			if (!err)
				err = -EIO;
			ntfs_error(vol->sb, "Cannot perform write to inode "
					"0x%lx, attribute type 0x%x, because "
					"ntfs_truncate() failed (error code "
					"%i).", vi->i_ino,
					(unsigned)le32_to_cpu(ni->type), err);
			return err;
		}
	}
	 
	nr_pages = 1;
	if (vol->cluster_size > PAGE_SIZE && NInoNonResident(ni))
		nr_pages = vol->cluster_size >> PAGE_SHIFT;
	last_vcn = -1;
	do {
		VCN vcn;
		pgoff_t start_idx;
		unsigned ofs, do_pages, u;
		size_t copied;

		start_idx = pos >> PAGE_SHIFT;
		ofs = pos & ~PAGE_MASK;
		bytes = PAGE_SIZE - ofs;
		do_pages = 1;
		if (nr_pages > 1) {
			vcn = pos >> vol->cluster_size_bits;
			if (vcn != last_vcn) {
				last_vcn = vcn;
				 
				down_read(&ni->runlist.lock);
				lcn = ntfs_attr_vcn_to_lcn_nolock(ni, pos >>
						vol->cluster_size_bits, false);
				up_read(&ni->runlist.lock);
				if (unlikely(lcn < LCN_HOLE)) {
					if (lcn == LCN_ENOMEM)
						status = -ENOMEM;
					else {
						status = -EIO;
						ntfs_error(vol->sb, "Cannot "
							"perform write to "
							"inode 0x%lx, "
							"attribute type 0x%x, "
							"because the attribute "
							"is corrupt.",
							vi->i_ino, (unsigned)
							le32_to_cpu(ni->type));
					}
					break;
				}
				if (lcn == LCN_HOLE) {
					start_idx = (pos & ~(s64)
							vol->cluster_size_mask)
							>> PAGE_SHIFT;
					bytes = vol->cluster_size - (pos &
							vol->cluster_size_mask);
					do_pages = nr_pages;
				}
			}
		}
		if (bytes > iov_iter_count(i))
			bytes = iov_iter_count(i);
again:
		 
		if (unlikely(fault_in_iov_iter_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}
		 
		status = __ntfs_grab_cache_pages(mapping, start_idx, do_pages,
				pages, &cached_page);
		if (unlikely(status))
			break;
		 
		if (NInoNonResident(ni)) {
			status = ntfs_prepare_pages_for_non_resident_write(
					pages, do_pages, pos, bytes);
			if (unlikely(status)) {
				do {
					unlock_page(pages[--do_pages]);
					put_page(pages[do_pages]);
				} while (do_pages);
				break;
			}
		}
		u = (pos >> PAGE_SHIFT) - pages[0]->index;
		copied = ntfs_copy_from_user_iter(pages + u, do_pages - u, ofs,
					i, bytes);
		ntfs_flush_dcache_pages(pages + u, do_pages - u);
		status = 0;
		if (likely(copied == bytes)) {
			status = ntfs_commit_pages_after_write(pages, do_pages,
					pos, bytes);
		}
		do {
			unlock_page(pages[--do_pages]);
			put_page(pages[do_pages]);
		} while (do_pages);
		if (unlikely(status < 0)) {
			iov_iter_revert(i, copied);
			break;
		}
		cond_resched();
		if (unlikely(copied < bytes)) {
			iov_iter_revert(i, copied);
			if (copied)
				bytes = copied;
			else if (bytes > PAGE_SIZE - ofs)
				bytes = PAGE_SIZE - ofs;
			goto again;
		}
		pos += copied;
		written += copied;
		balance_dirty_pages_ratelimited(mapping);
		if (fatal_signal_pending(current)) {
			status = -EINTR;
			break;
		}
	} while (iov_iter_count(i));
	if (cached_page)
		put_page(cached_page);
	ntfs_debug("Done.  Returning %s (written 0x%lx, status %li).",
			written ? "written" : "status", (unsigned long)written,
			(long)status);
	return written ? written : status;
}

 
static ssize_t ntfs_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct inode *vi = file_inode(file);
	ssize_t written = 0;
	ssize_t err;

	inode_lock(vi);
	 
	err = ntfs_prepare_file_for_write(iocb, from);
	if (iov_iter_count(from) && !err)
		written = ntfs_perform_write(file, from, iocb->ki_pos);
	inode_unlock(vi);
	iocb->ki_pos += written;
	if (likely(written > 0))
		written = generic_write_sync(iocb, written);
	return written ? written : err;
}

 
static int ntfs_file_fsync(struct file *filp, loff_t start, loff_t end,
			   int datasync)
{
	struct inode *vi = filp->f_mapping->host;
	int err, ret = 0;

	ntfs_debug("Entering for inode 0x%lx.", vi->i_ino);

	err = file_write_and_wait_range(filp, start, end);
	if (err)
		return err;
	inode_lock(vi);

	BUG_ON(S_ISDIR(vi->i_mode));
	if (!datasync || !NInoNonResident(NTFS_I(vi)))
		ret = __ntfs_write_inode(vi, 1);
	write_inode_now(vi, !datasync);
	 
	err = sync_blockdev(vi->i_sb->s_bdev);
	if (unlikely(err && !ret))
		ret = err;
	if (likely(!ret))
		ntfs_debug("Done.");
	else
		ntfs_warning(vi->i_sb, "Failed to f%ssync inode 0x%lx.  Error "
				"%u.", datasync ? "data" : "", vi->i_ino, -ret);
	inode_unlock(vi);
	return ret;
}

#endif  

const struct file_operations ntfs_file_ops = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
#ifdef NTFS_RW
	.write_iter	= ntfs_file_write_iter,
	.fsync		= ntfs_file_fsync,
#endif  
	.mmap		= generic_file_mmap,
	.open		= ntfs_file_open,
	.splice_read	= filemap_splice_read,
};

const struct inode_operations ntfs_file_inode_ops = {
#ifdef NTFS_RW
	.setattr	= ntfs_setattr,
#endif  
};

const struct file_operations ntfs_empty_file_ops = {};

const struct inode_operations ntfs_empty_inode_ops = {};
