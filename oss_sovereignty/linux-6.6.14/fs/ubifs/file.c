
 

 

#include "ubifs.h"
#include <linux/mount.h>
#include <linux/slab.h>
#include <linux/migrate.h>

static int read_block(struct inode *inode, void *addr, unsigned int block,
		      struct ubifs_data_node *dn)
{
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	int err, len, out_len;
	union ubifs_key key;
	unsigned int dlen;

	data_key_init(c, &key, inode->i_ino, block);
	err = ubifs_tnc_lookup(c, &key, dn);
	if (err) {
		if (err == -ENOENT)
			 
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		return err;
	}

	ubifs_assert(c, le64_to_cpu(dn->ch.sqnum) >
		     ubifs_inode(inode)->creat_sqnum);
	len = le32_to_cpu(dn->size);
	if (len <= 0 || len > UBIFS_BLOCK_SIZE)
		goto dump;

	dlen = le32_to_cpu(dn->ch.len) - UBIFS_DATA_NODE_SZ;

	if (IS_ENCRYPTED(inode)) {
		err = ubifs_decrypt(inode, dn, &dlen, block);
		if (err)
			goto dump;
	}

	out_len = UBIFS_BLOCK_SIZE;
	err = ubifs_decompress(c, &dn->data, dlen, addr, &out_len,
			       le16_to_cpu(dn->compr_type));
	if (err || len != out_len)
		goto dump;

	 
	if (len < UBIFS_BLOCK_SIZE)
		memset(addr + len, 0, UBIFS_BLOCK_SIZE - len);

	return 0;

dump:
	ubifs_err(c, "bad data node (block %u, inode %lu)",
		  block, inode->i_ino);
	ubifs_dump_node(c, dn, UBIFS_MAX_DATA_NODE_SZ);
	return -EINVAL;
}

static int do_readpage(struct page *page)
{
	void *addr;
	int err = 0, i;
	unsigned int block, beyond;
	struct ubifs_data_node *dn;
	struct inode *inode = page->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	loff_t i_size = i_size_read(inode);

	dbg_gen("ino %lu, pg %lu, i_size %lld, flags %#lx",
		inode->i_ino, page->index, i_size, page->flags);
	ubifs_assert(c, !PageChecked(page));
	ubifs_assert(c, !PagePrivate(page));

	addr = kmap(page);

	block = page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	beyond = (i_size + UBIFS_BLOCK_SIZE - 1) >> UBIFS_BLOCK_SHIFT;
	if (block >= beyond) {
		 
		SetPageChecked(page);
		memset(addr, 0, PAGE_SIZE);
		goto out;
	}

	dn = kmalloc(UBIFS_MAX_DATA_NODE_SZ, GFP_NOFS);
	if (!dn) {
		err = -ENOMEM;
		goto error;
	}

	i = 0;
	while (1) {
		int ret;

		if (block >= beyond) {
			 
			err = -ENOENT;
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		} else {
			ret = read_block(inode, addr, block, dn);
			if (ret) {
				err = ret;
				if (err != -ENOENT)
					break;
			} else if (block + 1 == beyond) {
				int dlen = le32_to_cpu(dn->size);
				int ilen = i_size & (UBIFS_BLOCK_SIZE - 1);

				if (ilen && ilen < dlen)
					memset(addr + ilen, 0, dlen - ilen);
			}
		}
		if (++i >= UBIFS_BLOCKS_PER_PAGE)
			break;
		block += 1;
		addr += UBIFS_BLOCK_SIZE;
	}
	if (err) {
		struct ubifs_info *c = inode->i_sb->s_fs_info;
		if (err == -ENOENT) {
			 
			SetPageChecked(page);
			dbg_gen("hole");
			goto out_free;
		}
		ubifs_err(c, "cannot read page %lu of inode %lu, error %d",
			  page->index, inode->i_ino, err);
		goto error;
	}

out_free:
	kfree(dn);
out:
	SetPageUptodate(page);
	ClearPageError(page);
	flush_dcache_page(page);
	kunmap(page);
	return 0;

error:
	kfree(dn);
	ClearPageUptodate(page);
	SetPageError(page);
	flush_dcache_page(page);
	kunmap(page);
	return err;
}

 
static void release_new_page_budget(struct ubifs_info *c)
{
	struct ubifs_budget_req req = { .recalculate = 1, .new_page = 1 };

	ubifs_release_budget(c, &req);
}

 
static void release_existing_page_budget(struct ubifs_info *c)
{
	struct ubifs_budget_req req = { .dd_growth = c->bi.page_budget};

	ubifs_release_budget(c, &req);
}

static int write_begin_slow(struct address_space *mapping,
			    loff_t pos, unsigned len, struct page **pagep)
{
	struct inode *inode = mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	pgoff_t index = pos >> PAGE_SHIFT;
	struct ubifs_budget_req req = { .new_page = 1 };
	int err, appending = !!(pos + len > inode->i_size);
	struct page *page;

	dbg_gen("ino %lu, pos %llu, len %u, i_size %lld",
		inode->i_ino, pos, len, inode->i_size);

	 
	if (appending)
		 
		req.dirtied_ino = 1;

	err = ubifs_budget_space(c, &req);
	if (unlikely(err))
		return err;

	page = grab_cache_page_write_begin(mapping, index);
	if (unlikely(!page)) {
		ubifs_release_budget(c, &req);
		return -ENOMEM;
	}

	if (!PageUptodate(page)) {
		if (!(pos & ~PAGE_MASK) && len == PAGE_SIZE)
			SetPageChecked(page);
		else {
			err = do_readpage(page);
			if (err) {
				unlock_page(page);
				put_page(page);
				ubifs_release_budget(c, &req);
				return err;
			}
		}

		SetPageUptodate(page);
		ClearPageError(page);
	}

	if (PagePrivate(page))
		 
		release_new_page_budget(c);
	else if (!PageChecked(page))
		 
		ubifs_convert_page_budget(c);

	if (appending) {
		struct ubifs_inode *ui = ubifs_inode(inode);

		 
		mutex_lock(&ui->ui_mutex);
		if (ui->dirty)
			 
			ubifs_release_dirty_inode_budget(c, ui);
	}

	*pagep = page;
	return 0;
}

 
static int allocate_budget(struct ubifs_info *c, struct page *page,
			   struct ubifs_inode *ui, int appending)
{
	struct ubifs_budget_req req = { .fast = 1 };

	if (PagePrivate(page)) {
		if (!appending)
			 
			return 0;

		mutex_lock(&ui->ui_mutex);
		if (ui->dirty)
			 
			return 0;

		 
		req.dirtied_ino = 1;
	} else {
		if (PageChecked(page))
			 
			req.new_page = 1;
		else
			 
			req.dirtied_page = 1;

		if (appending) {
			mutex_lock(&ui->ui_mutex);
			if (!ui->dirty)
				 
				req.dirtied_ino = 1;
		}
	}

	return ubifs_budget_space(c, &req);
}

 
static int ubifs_write_begin(struct file *file, struct address_space *mapping,
			     loff_t pos, unsigned len,
			     struct page **pagep, void **fsdata)
{
	struct inode *inode = mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	struct ubifs_inode *ui = ubifs_inode(inode);
	pgoff_t index = pos >> PAGE_SHIFT;
	int err, appending = !!(pos + len > inode->i_size);
	int skipped_read = 0;
	struct page *page;

	ubifs_assert(c, ubifs_inode(inode)->ui_size == inode->i_size);
	ubifs_assert(c, !c->ro_media && !c->ro_mount);

	if (unlikely(c->ro_error))
		return -EROFS;

	 
	page = grab_cache_page_write_begin(mapping, index);
	if (unlikely(!page))
		return -ENOMEM;

	if (!PageUptodate(page)) {
		 
		if (!(pos & ~PAGE_MASK) && len == PAGE_SIZE) {
			 
			SetPageChecked(page);
			skipped_read = 1;
		} else {
			err = do_readpage(page);
			if (err) {
				unlock_page(page);
				put_page(page);
				return err;
			}
		}

		SetPageUptodate(page);
		ClearPageError(page);
	}

	err = allocate_budget(c, page, ui, appending);
	if (unlikely(err)) {
		ubifs_assert(c, err == -ENOSPC);
		 
		if (skipped_read) {
			ClearPageChecked(page);
			ClearPageUptodate(page);
		}
		 
		if (appending) {
			ubifs_assert(c, mutex_is_locked(&ui->ui_mutex));
			mutex_unlock(&ui->ui_mutex);
		}
		unlock_page(page);
		put_page(page);

		return write_begin_slow(mapping, pos, len, pagep);
	}

	 
	*pagep = page;
	return 0;

}

 
static void cancel_budget(struct ubifs_info *c, struct page *page,
			  struct ubifs_inode *ui, int appending)
{
	if (appending) {
		if (!ui->dirty)
			ubifs_release_dirty_inode_budget(c, ui);
		mutex_unlock(&ui->ui_mutex);
	}
	if (!PagePrivate(page)) {
		if (PageChecked(page))
			release_new_page_budget(c);
		else
			release_existing_page_budget(c);
	}
}

static int ubifs_write_end(struct file *file, struct address_space *mapping,
			   loff_t pos, unsigned len, unsigned copied,
			   struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	loff_t end_pos = pos + len;
	int appending = !!(end_pos > inode->i_size);

	dbg_gen("ino %lu, pos %llu, pg %lu, len %u, copied %d, i_size %lld",
		inode->i_ino, pos, page->index, len, copied, inode->i_size);

	if (unlikely(copied < len && len == PAGE_SIZE)) {
		 
		dbg_gen("copied %d instead of %d, read page and repeat",
			copied, len);
		cancel_budget(c, page, ui, appending);
		ClearPageChecked(page);

		 
		copied = do_readpage(page);
		goto out;
	}

	if (!PagePrivate(page)) {
		attach_page_private(page, (void *)1);
		atomic_long_inc(&c->dirty_pg_cnt);
		__set_page_dirty_nobuffers(page);
	}

	if (appending) {
		i_size_write(inode, end_pos);
		ui->ui_size = end_pos;
		 
		__mark_inode_dirty(inode, I_DIRTY_DATASYNC);
		ubifs_assert(c, mutex_is_locked(&ui->ui_mutex));
		mutex_unlock(&ui->ui_mutex);
	}

out:
	unlock_page(page);
	put_page(page);
	return copied;
}

 
static int populate_page(struct ubifs_info *c, struct page *page,
			 struct bu_info *bu, int *n)
{
	int i = 0, nn = *n, offs = bu->zbranch[0].offs, hole = 0, read = 0;
	struct inode *inode = page->mapping->host;
	loff_t i_size = i_size_read(inode);
	unsigned int page_block;
	void *addr, *zaddr;
	pgoff_t end_index;

	dbg_gen("ino %lu, pg %lu, i_size %lld, flags %#lx",
		inode->i_ino, page->index, i_size, page->flags);

	addr = zaddr = kmap(page);

	end_index = (i_size - 1) >> PAGE_SHIFT;
	if (!i_size || page->index > end_index) {
		hole = 1;
		memset(addr, 0, PAGE_SIZE);
		goto out_hole;
	}

	page_block = page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	while (1) {
		int err, len, out_len, dlen;

		if (nn >= bu->cnt) {
			hole = 1;
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		} else if (key_block(c, &bu->zbranch[nn].key) == page_block) {
			struct ubifs_data_node *dn;

			dn = bu->buf + (bu->zbranch[nn].offs - offs);

			ubifs_assert(c, le64_to_cpu(dn->ch.sqnum) >
				     ubifs_inode(inode)->creat_sqnum);

			len = le32_to_cpu(dn->size);
			if (len <= 0 || len > UBIFS_BLOCK_SIZE)
				goto out_err;

			dlen = le32_to_cpu(dn->ch.len) - UBIFS_DATA_NODE_SZ;
			out_len = UBIFS_BLOCK_SIZE;

			if (IS_ENCRYPTED(inode)) {
				err = ubifs_decrypt(inode, dn, &dlen, page_block);
				if (err)
					goto out_err;
			}

			err = ubifs_decompress(c, &dn->data, dlen, addr, &out_len,
					       le16_to_cpu(dn->compr_type));
			if (err || len != out_len)
				goto out_err;

			if (len < UBIFS_BLOCK_SIZE)
				memset(addr + len, 0, UBIFS_BLOCK_SIZE - len);

			nn += 1;
			read = (i << UBIFS_BLOCK_SHIFT) + len;
		} else if (key_block(c, &bu->zbranch[nn].key) < page_block) {
			nn += 1;
			continue;
		} else {
			hole = 1;
			memset(addr, 0, UBIFS_BLOCK_SIZE);
		}
		if (++i >= UBIFS_BLOCKS_PER_PAGE)
			break;
		addr += UBIFS_BLOCK_SIZE;
		page_block += 1;
	}

	if (end_index == page->index) {
		int len = i_size & (PAGE_SIZE - 1);

		if (len && len < read)
			memset(zaddr + len, 0, read - len);
	}

out_hole:
	if (hole) {
		SetPageChecked(page);
		dbg_gen("hole");
	}

	SetPageUptodate(page);
	ClearPageError(page);
	flush_dcache_page(page);
	kunmap(page);
	*n = nn;
	return 0;

out_err:
	ClearPageUptodate(page);
	SetPageError(page);
	flush_dcache_page(page);
	kunmap(page);
	ubifs_err(c, "bad data node (block %u, inode %lu)",
		  page_block, inode->i_ino);
	return -EINVAL;
}

 
static int ubifs_do_bulk_read(struct ubifs_info *c, struct bu_info *bu,
			      struct page *page1)
{
	pgoff_t offset = page1->index, end_index;
	struct address_space *mapping = page1->mapping;
	struct inode *inode = mapping->host;
	struct ubifs_inode *ui = ubifs_inode(inode);
	int err, page_idx, page_cnt, ret = 0, n = 0;
	int allocate = bu->buf ? 0 : 1;
	loff_t isize;
	gfp_t ra_gfp_mask = readahead_gfp_mask(mapping) & ~__GFP_FS;

	err = ubifs_tnc_get_bu_keys(c, bu);
	if (err)
		goto out_warn;

	if (bu->eof) {
		 
		ui->read_in_a_row = 1;
		ui->bulk_read = 0;
	}

	page_cnt = bu->blk_cnt >> UBIFS_BLOCKS_PER_PAGE_SHIFT;
	if (!page_cnt) {
		 
		goto out_bu_off;
	}

	if (bu->cnt) {
		if (allocate) {
			 
			bu->buf_len = bu->zbranch[bu->cnt - 1].offs +
				      bu->zbranch[bu->cnt - 1].len -
				      bu->zbranch[0].offs;
			ubifs_assert(c, bu->buf_len > 0);
			ubifs_assert(c, bu->buf_len <= c->leb_size);
			bu->buf = kmalloc(bu->buf_len, GFP_NOFS | __GFP_NOWARN);
			if (!bu->buf)
				goto out_bu_off;
		}

		err = ubifs_tnc_bulk_read(c, bu);
		if (err)
			goto out_warn;
	}

	err = populate_page(c, page1, bu, &n);
	if (err)
		goto out_warn;

	unlock_page(page1);
	ret = 1;

	isize = i_size_read(inode);
	if (isize == 0)
		goto out_free;
	end_index = ((isize - 1) >> PAGE_SHIFT);

	for (page_idx = 1; page_idx < page_cnt; page_idx++) {
		pgoff_t page_offset = offset + page_idx;
		struct page *page;

		if (page_offset > end_index)
			break;
		page = pagecache_get_page(mapping, page_offset,
				 FGP_LOCK|FGP_ACCESSED|FGP_CREAT|FGP_NOWAIT,
				 ra_gfp_mask);
		if (!page)
			break;
		if (!PageUptodate(page))
			err = populate_page(c, page, bu, &n);
		unlock_page(page);
		put_page(page);
		if (err)
			break;
	}

	ui->last_page_read = offset + page_idx - 1;

out_free:
	if (allocate)
		kfree(bu->buf);
	return ret;

out_warn:
	ubifs_warn(c, "ignoring error %d and skipping bulk-read", err);
	goto out_free;

out_bu_off:
	ui->read_in_a_row = ui->bulk_read = 0;
	goto out_free;
}

 
static int ubifs_bulk_read(struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	struct ubifs_inode *ui = ubifs_inode(inode);
	pgoff_t index = page->index, last_page_read = ui->last_page_read;
	struct bu_info *bu;
	int err = 0, allocated = 0;

	ui->last_page_read = index;
	if (!c->bulk_read)
		return 0;

	 
	if (!mutex_trylock(&ui->ui_mutex))
		return 0;

	if (index != last_page_read + 1) {
		 
		ui->read_in_a_row = 1;
		if (ui->bulk_read)
			ui->bulk_read = 0;
		goto out_unlock;
	}

	if (!ui->bulk_read) {
		ui->read_in_a_row += 1;
		if (ui->read_in_a_row < 3)
			goto out_unlock;
		 
		ui->bulk_read = 1;
	}

	 
	if (mutex_trylock(&c->bu_mutex))
		bu = &c->bu;
	else {
		bu = kmalloc(sizeof(struct bu_info), GFP_NOFS | __GFP_NOWARN);
		if (!bu)
			goto out_unlock;

		bu->buf = NULL;
		allocated = 1;
	}

	bu->buf_len = c->max_bu_buf_len;
	data_key_init(c, &bu->key, inode->i_ino,
		      page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT);
	err = ubifs_do_bulk_read(c, bu, page);

	if (!allocated)
		mutex_unlock(&c->bu_mutex);
	else
		kfree(bu);

out_unlock:
	mutex_unlock(&ui->ui_mutex);
	return err;
}

static int ubifs_read_folio(struct file *file, struct folio *folio)
{
	struct page *page = &folio->page;

	if (ubifs_bulk_read(page))
		return 0;
	do_readpage(page);
	folio_unlock(folio);
	return 0;
}

static int do_writepage(struct page *page, int len)
{
	int err = 0, i, blen;
	unsigned int block;
	void *addr;
	union ubifs_key key;
	struct inode *inode = page->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;

#ifdef UBIFS_DEBUG
	struct ubifs_inode *ui = ubifs_inode(inode);
	spin_lock(&ui->ui_lock);
	ubifs_assert(c, page->index <= ui->synced_i_size >> PAGE_SHIFT);
	spin_unlock(&ui->ui_lock);
#endif

	 
	set_page_writeback(page);

	addr = kmap(page);
	block = page->index << UBIFS_BLOCKS_PER_PAGE_SHIFT;
	i = 0;
	while (len) {
		blen = min_t(int, len, UBIFS_BLOCK_SIZE);
		data_key_init(c, &key, inode->i_ino, block);
		err = ubifs_jnl_write_data(c, inode, &key, addr, blen);
		if (err)
			break;
		if (++i >= UBIFS_BLOCKS_PER_PAGE)
			break;
		block += 1;
		addr += blen;
		len -= blen;
	}
	if (err) {
		SetPageError(page);
		ubifs_err(c, "cannot write page %lu of inode %lu, error %d",
			  page->index, inode->i_ino, err);
		ubifs_ro_mode(c, err);
	}

	ubifs_assert(c, PagePrivate(page));
	if (PageChecked(page))
		release_new_page_budget(c);
	else
		release_existing_page_budget(c);

	atomic_long_dec(&c->dirty_pg_cnt);
	detach_page_private(page);
	ClearPageChecked(page);

	kunmap(page);
	unlock_page(page);
	end_page_writeback(page);
	return err;
}

 
static int ubifs_writepage(struct page *page, struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	struct ubifs_inode *ui = ubifs_inode(inode);
	loff_t i_size =  i_size_read(inode), synced_i_size;
	pgoff_t end_index = i_size >> PAGE_SHIFT;
	int err, len = i_size & (PAGE_SIZE - 1);
	void *kaddr;

	dbg_gen("ino %lu, pg %lu, pg flags %#lx",
		inode->i_ino, page->index, page->flags);
	ubifs_assert(c, PagePrivate(page));

	 
	if (page->index > end_index || (page->index == end_index && !len)) {
		err = 0;
		goto out_unlock;
	}

	spin_lock(&ui->ui_lock);
	synced_i_size = ui->synced_i_size;
	spin_unlock(&ui->ui_lock);

	 
	if (page->index < end_index) {
		if (page->index >= synced_i_size >> PAGE_SHIFT) {
			err = inode->i_sb->s_op->write_inode(inode, NULL);
			if (err)
				goto out_redirty;
			 
		}
		return do_writepage(page, PAGE_SIZE);
	}

	 
	kaddr = kmap_atomic(page);
	memset(kaddr + len, 0, PAGE_SIZE - len);
	flush_dcache_page(page);
	kunmap_atomic(kaddr);

	if (i_size > synced_i_size) {
		err = inode->i_sb->s_op->write_inode(inode, NULL);
		if (err)
			goto out_redirty;
	}

	return do_writepage(page, len);
out_redirty:
	 
	redirty_page_for_writepage(wbc, page);
out_unlock:
	unlock_page(page);
	return err;
}

 
static void do_attr_changes(struct inode *inode, const struct iattr *attr)
{
	if (attr->ia_valid & ATTR_UID)
		inode->i_uid = attr->ia_uid;
	if (attr->ia_valid & ATTR_GID)
		inode->i_gid = attr->ia_gid;
	if (attr->ia_valid & ATTR_ATIME)
		inode->i_atime = attr->ia_atime;
	if (attr->ia_valid & ATTR_MTIME)
		inode->i_mtime = attr->ia_mtime;
	if (attr->ia_valid & ATTR_CTIME)
		inode_set_ctime_to_ts(inode, attr->ia_ctime);
	if (attr->ia_valid & ATTR_MODE) {
		umode_t mode = attr->ia_mode;

		if (!in_group_p(inode->i_gid) && !capable(CAP_FSETID))
			mode &= ~S_ISGID;
		inode->i_mode = mode;
	}
}

 
static int do_truncation(struct ubifs_info *c, struct inode *inode,
			 const struct iattr *attr)
{
	int err;
	struct ubifs_budget_req req;
	loff_t old_size = inode->i_size, new_size = attr->ia_size;
	int offset = new_size & (UBIFS_BLOCK_SIZE - 1), budgeted = 1;
	struct ubifs_inode *ui = ubifs_inode(inode);

	dbg_gen("ino %lu, size %lld -> %lld", inode->i_ino, old_size, new_size);
	memset(&req, 0, sizeof(struct ubifs_budget_req));

	 
	if (new_size & (UBIFS_BLOCK_SIZE - 1))
		req.dirtied_page = 1;

	req.dirtied_ino = 1;
	 
	req.dirtied_ino_d = UBIFS_TRUN_NODE_SZ;
	err = ubifs_budget_space(c, &req);
	if (err) {
		 
		if (new_size || err != -ENOSPC)
			return err;
		budgeted = 0;
	}

	truncate_setsize(inode, new_size);

	if (offset) {
		pgoff_t index = new_size >> PAGE_SHIFT;
		struct page *page;

		page = find_lock_page(inode->i_mapping, index);
		if (page) {
			if (PageDirty(page)) {
				 
				ubifs_assert(c, PagePrivate(page));

				clear_page_dirty_for_io(page);
				if (UBIFS_BLOCKS_PER_PAGE_SHIFT)
					offset = new_size &
						 (PAGE_SIZE - 1);
				err = do_writepage(page, offset);
				put_page(page);
				if (err)
					goto out_budg;
				 
			} else {
				 
				unlock_page(page);
				put_page(page);
			}
		}
	}

	mutex_lock(&ui->ui_mutex);
	ui->ui_size = inode->i_size;
	 
	inode->i_mtime = inode_set_ctime_current(inode);
	 
	do_attr_changes(inode, attr);
	err = ubifs_jnl_truncate(c, inode, old_size, new_size);
	mutex_unlock(&ui->ui_mutex);

out_budg:
	if (budgeted)
		ubifs_release_budget(c, &req);
	else {
		c->bi.nospace = c->bi.nospace_rp = 0;
		smp_wmb();
	}
	return err;
}

 
static int do_setattr(struct ubifs_info *c, struct inode *inode,
		      const struct iattr *attr)
{
	int err, release;
	loff_t new_size = attr->ia_size;
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_budget_req req = { .dirtied_ino = 1,
				.dirtied_ino_d = ALIGN(ui->data_len, 8) };

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	if (attr->ia_valid & ATTR_SIZE) {
		dbg_gen("size %lld -> %lld", inode->i_size, new_size);
		truncate_setsize(inode, new_size);
	}

	mutex_lock(&ui->ui_mutex);
	if (attr->ia_valid & ATTR_SIZE) {
		 
		inode->i_mtime = inode_set_ctime_current(inode);
		 
		ui->ui_size = inode->i_size;
	}

	do_attr_changes(inode, attr);

	release = ui->dirty;
	if (attr->ia_valid & ATTR_SIZE)
		 
		 __mark_inode_dirty(inode, I_DIRTY_DATASYNC);
	else
		mark_inode_dirty_sync(inode);
	mutex_unlock(&ui->ui_mutex);

	if (release)
		ubifs_release_budget(c, &req);
	if (IS_SYNC(inode))
		err = inode->i_sb->s_op->write_inode(inode, NULL);
	return err;
}

int ubifs_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
		  struct iattr *attr)
{
	int err;
	struct inode *inode = d_inode(dentry);
	struct ubifs_info *c = inode->i_sb->s_fs_info;

	dbg_gen("ino %lu, mode %#x, ia_valid %#x",
		inode->i_ino, inode->i_mode, attr->ia_valid);
	err = setattr_prepare(&nop_mnt_idmap, dentry, attr);
	if (err)
		return err;

	err = dbg_check_synced_i_size(c, inode);
	if (err)
		return err;

	err = fscrypt_prepare_setattr(dentry, attr);
	if (err)
		return err;

	if ((attr->ia_valid & ATTR_SIZE) && attr->ia_size < inode->i_size)
		 
		err = do_truncation(c, inode, attr);
	else
		err = do_setattr(c, inode, attr);

	return err;
}

static void ubifs_invalidate_folio(struct folio *folio, size_t offset,
				 size_t length)
{
	struct inode *inode = folio->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;

	ubifs_assert(c, folio_test_private(folio));
	if (offset || length < folio_size(folio))
		 
		return;

	if (folio_test_checked(folio))
		release_new_page_budget(c);
	else
		release_existing_page_budget(c);

	atomic_long_dec(&c->dirty_pg_cnt);
	folio_detach_private(folio);
	folio_clear_checked(folio);
}

int ubifs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	struct inode *inode = file->f_mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	int err;

	dbg_gen("syncing inode %lu", inode->i_ino);

	if (c->ro_mount)
		 
		return 0;

	err = file_write_and_wait_range(file, start, end);
	if (err)
		return err;
	inode_lock(inode);

	 
	if (!datasync || (inode->i_state & I_DIRTY_DATASYNC)) {
		err = inode->i_sb->s_op->write_inode(inode, NULL);
		if (err)
			goto out;
	}

	 
	err = ubifs_sync_wbufs_by_inode(c, inode);
out:
	inode_unlock(inode);
	return err;
}

 
static inline int mctime_update_needed(const struct inode *inode,
				       const struct timespec64 *now)
{
	struct timespec64 ctime = inode_get_ctime(inode);

	if (!timespec64_equal(&inode->i_mtime, now) ||
	    !timespec64_equal(&ctime, now))
		return 1;
	return 0;
}

 
int ubifs_update_time(struct inode *inode, int flags)
{
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	struct ubifs_budget_req req = { .dirtied_ino = 1,
			.dirtied_ino_d = ALIGN(ui->data_len, 8) };
	int err, release;

	if (!IS_ENABLED(CONFIG_UBIFS_ATIME_SUPPORT)) {
		generic_update_time(inode, flags);
		return 0;
	}

	err = ubifs_budget_space(c, &req);
	if (err)
		return err;

	mutex_lock(&ui->ui_mutex);
	inode_update_timestamps(inode, flags);
	release = ui->dirty;
	__mark_inode_dirty(inode, I_DIRTY_SYNC);
	mutex_unlock(&ui->ui_mutex);
	if (release)
		ubifs_release_budget(c, &req);
	return 0;
}

 
static int update_mctime(struct inode *inode)
{
	struct timespec64 now = current_time(inode);
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_info *c = inode->i_sb->s_fs_info;

	if (mctime_update_needed(inode, &now)) {
		int err, release;
		struct ubifs_budget_req req = { .dirtied_ino = 1,
				.dirtied_ino_d = ALIGN(ui->data_len, 8) };

		err = ubifs_budget_space(c, &req);
		if (err)
			return err;

		mutex_lock(&ui->ui_mutex);
		inode->i_mtime = inode_set_ctime_current(inode);
		release = ui->dirty;
		mark_inode_dirty_sync(inode);
		mutex_unlock(&ui->ui_mutex);
		if (release)
			ubifs_release_budget(c, &req);
	}

	return 0;
}

static ssize_t ubifs_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	int err = update_mctime(file_inode(iocb->ki_filp));
	if (err)
		return err;

	return generic_file_write_iter(iocb, from);
}

static bool ubifs_dirty_folio(struct address_space *mapping,
		struct folio *folio)
{
	bool ret;
	struct ubifs_info *c = mapping->host->i_sb->s_fs_info;

	ret = filemap_dirty_folio(mapping, folio);
	 
	ubifs_assert(c, ret == false);
	return ret;
}

static bool ubifs_release_folio(struct folio *folio, gfp_t unused_gfp_flags)
{
	struct inode *inode = folio->mapping->host;
	struct ubifs_info *c = inode->i_sb->s_fs_info;

	if (folio_test_writeback(folio))
		return false;

	 
	ubifs_assert(c, folio_test_private(folio));
	if (folio_test_checked(folio))
		release_new_page_budget(c);
	else
		release_existing_page_budget(c);

	atomic_long_dec(&c->dirty_pg_cnt);
	folio_detach_private(folio);
	folio_clear_checked(folio);
	return true;
}

 
static vm_fault_t ubifs_vm_page_mkwrite(struct vm_fault *vmf)
{
	struct page *page = vmf->page;
	struct inode *inode = file_inode(vmf->vma->vm_file);
	struct ubifs_info *c = inode->i_sb->s_fs_info;
	struct timespec64 now = current_time(inode);
	struct ubifs_budget_req req = { .new_page = 1 };
	int err, update_time;

	dbg_gen("ino %lu, pg %lu, i_size %lld",	inode->i_ino, page->index,
		i_size_read(inode));
	ubifs_assert(c, !c->ro_media && !c->ro_mount);

	if (unlikely(c->ro_error))
		return VM_FAULT_SIGBUS;  

	 
	update_time = mctime_update_needed(inode, &now);
	if (update_time)
		 
		req.dirtied_ino = 1;

	err = ubifs_budget_space(c, &req);
	if (unlikely(err)) {
		if (err == -ENOSPC)
			ubifs_warn(c, "out of space for mmapped file (inode number %lu)",
				   inode->i_ino);
		return VM_FAULT_SIGBUS;
	}

	lock_page(page);
	if (unlikely(page->mapping != inode->i_mapping ||
		     page_offset(page) > i_size_read(inode))) {
		 
		goto sigbus;
	}

	if (PagePrivate(page))
		release_new_page_budget(c);
	else {
		if (!PageChecked(page))
			ubifs_convert_page_budget(c);
		attach_page_private(page, (void *)1);
		atomic_long_inc(&c->dirty_pg_cnt);
		__set_page_dirty_nobuffers(page);
	}

	if (update_time) {
		int release;
		struct ubifs_inode *ui = ubifs_inode(inode);

		mutex_lock(&ui->ui_mutex);
		inode->i_mtime = inode_set_ctime_current(inode);
		release = ui->dirty;
		mark_inode_dirty_sync(inode);
		mutex_unlock(&ui->ui_mutex);
		if (release)
			ubifs_release_dirty_inode_budget(c, ui);
	}

	wait_for_stable_page(page);
	return VM_FAULT_LOCKED;

sigbus:
	unlock_page(page);
	ubifs_release_budget(c, &req);
	return VM_FAULT_SIGBUS;
}

static const struct vm_operations_struct ubifs_file_vm_ops = {
	.fault        = filemap_fault,
	.map_pages = filemap_map_pages,
	.page_mkwrite = ubifs_vm_page_mkwrite,
};

static int ubifs_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err;

	err = generic_file_mmap(file, vma);
	if (err)
		return err;
	vma->vm_ops = &ubifs_file_vm_ops;

	if (IS_ENABLED(CONFIG_UBIFS_ATIME_SUPPORT))
		file_accessed(file);

	return 0;
}

static const char *ubifs_get_link(struct dentry *dentry,
					    struct inode *inode,
					    struct delayed_call *done)
{
	struct ubifs_inode *ui = ubifs_inode(inode);

	if (!IS_ENCRYPTED(inode))
		return ui->data;

	if (!dentry)
		return ERR_PTR(-ECHILD);

	return fscrypt_get_symlink(inode, ui->data, ui->data_len, done);
}

static int ubifs_symlink_getattr(struct mnt_idmap *idmap,
				 const struct path *path, struct kstat *stat,
				 u32 request_mask, unsigned int query_flags)
{
	ubifs_getattr(idmap, path, stat, request_mask, query_flags);

	if (IS_ENCRYPTED(d_inode(path->dentry)))
		return fscrypt_symlink_getattr(path, stat);
	return 0;
}

const struct address_space_operations ubifs_file_address_operations = {
	.read_folio     = ubifs_read_folio,
	.writepage      = ubifs_writepage,
	.write_begin    = ubifs_write_begin,
	.write_end      = ubifs_write_end,
	.invalidate_folio = ubifs_invalidate_folio,
	.dirty_folio	= ubifs_dirty_folio,
	.migrate_folio	= filemap_migrate_folio,
	.release_folio	= ubifs_release_folio,
};

const struct inode_operations ubifs_file_inode_operations = {
	.setattr     = ubifs_setattr,
	.getattr     = ubifs_getattr,
	.listxattr   = ubifs_listxattr,
	.update_time = ubifs_update_time,
	.fileattr_get = ubifs_fileattr_get,
	.fileattr_set = ubifs_fileattr_set,
};

const struct inode_operations ubifs_symlink_inode_operations = {
	.get_link    = ubifs_get_link,
	.setattr     = ubifs_setattr,
	.getattr     = ubifs_symlink_getattr,
	.listxattr   = ubifs_listxattr,
	.update_time = ubifs_update_time,
};

const struct file_operations ubifs_file_operations = {
	.llseek         = generic_file_llseek,
	.read_iter      = generic_file_read_iter,
	.write_iter     = ubifs_write_iter,
	.mmap           = ubifs_file_mmap,
	.fsync          = ubifs_fsync,
	.unlocked_ioctl = ubifs_ioctl,
	.splice_read	= filemap_splice_read,
	.splice_write	= iter_file_splice_write,
	.open		= fscrypt_file_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ubifs_compat_ioctl,
#endif
};
