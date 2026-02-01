
 

 

#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/iomap.h>
#include <linux/mm.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/capability.h>
#include <linux/blkdev.h>
#include <linux/file.h>
#include <linux/quotaops.h>
#include <linux/highmem.h>
#include <linux/export.h>
#include <linux/backing-dev.h>
#include <linux/writeback.h>
#include <linux/hash.h>
#include <linux/suspend.h>
#include <linux/buffer_head.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/bio.h>
#include <linux/cpu.h>
#include <linux/bitops.h>
#include <linux/mpage.h>
#include <linux/bit_spinlock.h>
#include <linux/pagevec.h>
#include <linux/sched/mm.h>
#include <trace/events/block.h>
#include <linux/fscrypt.h>
#include <linux/fsverity.h>
#include <linux/sched/isolation.h>

#include "internal.h"

static int fsync_buffers_list(spinlock_t *lock, struct list_head *list);
static void submit_bh_wbc(blk_opf_t opf, struct buffer_head *bh,
			  struct writeback_control *wbc);

#define BH_ENTRY(list) list_entry((list), struct buffer_head, b_assoc_buffers)

inline void touch_buffer(struct buffer_head *bh)
{
	trace_block_touch_buffer(bh);
	folio_mark_accessed(bh->b_folio);
}
EXPORT_SYMBOL(touch_buffer);

void __lock_buffer(struct buffer_head *bh)
{
	wait_on_bit_lock_io(&bh->b_state, BH_Lock, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(__lock_buffer);

void unlock_buffer(struct buffer_head *bh)
{
	clear_bit_unlock(BH_Lock, &bh->b_state);
	smp_mb__after_atomic();
	wake_up_bit(&bh->b_state, BH_Lock);
}
EXPORT_SYMBOL(unlock_buffer);

 
void buffer_check_dirty_writeback(struct folio *folio,
				     bool *dirty, bool *writeback)
{
	struct buffer_head *head, *bh;
	*dirty = false;
	*writeback = false;

	BUG_ON(!folio_test_locked(folio));

	head = folio_buffers(folio);
	if (!head)
		return;

	if (folio_test_writeback(folio))
		*writeback = true;

	bh = head;
	do {
		if (buffer_locked(bh))
			*writeback = true;

		if (buffer_dirty(bh))
			*dirty = true;

		bh = bh->b_this_page;
	} while (bh != head);
}

 
void __wait_on_buffer(struct buffer_head * bh)
{
	wait_on_bit_io(&bh->b_state, BH_Lock, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(__wait_on_buffer);

static void buffer_io_error(struct buffer_head *bh, char *msg)
{
	if (!test_bit(BH_Quiet, &bh->b_state))
		printk_ratelimited(KERN_ERR
			"Buffer I/O error on dev %pg, logical block %llu%s\n",
			bh->b_bdev, (unsigned long long)bh->b_blocknr, msg);
}

 
static void __end_buffer_read_notouch(struct buffer_head *bh, int uptodate)
{
	if (uptodate) {
		set_buffer_uptodate(bh);
	} else {
		 
		clear_buffer_uptodate(bh);
	}
	unlock_buffer(bh);
}

 
void end_buffer_read_sync(struct buffer_head *bh, int uptodate)
{
	__end_buffer_read_notouch(bh, uptodate);
	put_bh(bh);
}
EXPORT_SYMBOL(end_buffer_read_sync);

void end_buffer_write_sync(struct buffer_head *bh, int uptodate)
{
	if (uptodate) {
		set_buffer_uptodate(bh);
	} else {
		buffer_io_error(bh, ", lost sync page write");
		mark_buffer_write_io_error(bh);
		clear_buffer_uptodate(bh);
	}
	unlock_buffer(bh);
	put_bh(bh);
}
EXPORT_SYMBOL(end_buffer_write_sync);

 
static struct buffer_head *
__find_get_block_slow(struct block_device *bdev, sector_t block)
{
	struct inode *bd_inode = bdev->bd_inode;
	struct address_space *bd_mapping = bd_inode->i_mapping;
	struct buffer_head *ret = NULL;
	pgoff_t index;
	struct buffer_head *bh;
	struct buffer_head *head;
	struct folio *folio;
	int all_mapped = 1;
	static DEFINE_RATELIMIT_STATE(last_warned, HZ, 1);

	index = block >> (PAGE_SHIFT - bd_inode->i_blkbits);
	folio = __filemap_get_folio(bd_mapping, index, FGP_ACCESSED, 0);
	if (IS_ERR(folio))
		goto out;

	spin_lock(&bd_mapping->private_lock);
	head = folio_buffers(folio);
	if (!head)
		goto out_unlock;
	bh = head;
	do {
		if (!buffer_mapped(bh))
			all_mapped = 0;
		else if (bh->b_blocknr == block) {
			ret = bh;
			get_bh(bh);
			goto out_unlock;
		}
		bh = bh->b_this_page;
	} while (bh != head);

	 
	ratelimit_set_flags(&last_warned, RATELIMIT_MSG_ON_RELEASE);
	if (all_mapped && __ratelimit(&last_warned)) {
		printk("__find_get_block_slow() failed. block=%llu, "
		       "b_blocknr=%llu, b_state=0x%08lx, b_size=%zu, "
		       "device %pg blocksize: %d\n",
		       (unsigned long long)block,
		       (unsigned long long)bh->b_blocknr,
		       bh->b_state, bh->b_size, bdev,
		       1 << bd_inode->i_blkbits);
	}
out_unlock:
	spin_unlock(&bd_mapping->private_lock);
	folio_put(folio);
out:
	return ret;
}

static void end_buffer_async_read(struct buffer_head *bh, int uptodate)
{
	unsigned long flags;
	struct buffer_head *first;
	struct buffer_head *tmp;
	struct folio *folio;
	int folio_uptodate = 1;

	BUG_ON(!buffer_async_read(bh));

	folio = bh->b_folio;
	if (uptodate) {
		set_buffer_uptodate(bh);
	} else {
		clear_buffer_uptodate(bh);
		buffer_io_error(bh, ", async page read");
		folio_set_error(folio);
	}

	 
	first = folio_buffers(folio);
	spin_lock_irqsave(&first->b_uptodate_lock, flags);
	clear_buffer_async_read(bh);
	unlock_buffer(bh);
	tmp = bh;
	do {
		if (!buffer_uptodate(tmp))
			folio_uptodate = 0;
		if (buffer_async_read(tmp)) {
			BUG_ON(!buffer_locked(tmp));
			goto still_busy;
		}
		tmp = tmp->b_this_page;
	} while (tmp != bh);
	spin_unlock_irqrestore(&first->b_uptodate_lock, flags);

	 
	if (folio_uptodate)
		folio_mark_uptodate(folio);
	folio_unlock(folio);
	return;

still_busy:
	spin_unlock_irqrestore(&first->b_uptodate_lock, flags);
	return;
}

struct postprocess_bh_ctx {
	struct work_struct work;
	struct buffer_head *bh;
};

static void verify_bh(struct work_struct *work)
{
	struct postprocess_bh_ctx *ctx =
		container_of(work, struct postprocess_bh_ctx, work);
	struct buffer_head *bh = ctx->bh;
	bool valid;

	valid = fsverity_verify_blocks(bh->b_folio, bh->b_size, bh_offset(bh));
	end_buffer_async_read(bh, valid);
	kfree(ctx);
}

static bool need_fsverity(struct buffer_head *bh)
{
	struct folio *folio = bh->b_folio;
	struct inode *inode = folio->mapping->host;

	return fsverity_active(inode) &&
		 
		folio->index < DIV_ROUND_UP(inode->i_size, PAGE_SIZE);
}

static void decrypt_bh(struct work_struct *work)
{
	struct postprocess_bh_ctx *ctx =
		container_of(work, struct postprocess_bh_ctx, work);
	struct buffer_head *bh = ctx->bh;
	int err;

	err = fscrypt_decrypt_pagecache_blocks(bh->b_folio, bh->b_size,
					       bh_offset(bh));
	if (err == 0 && need_fsverity(bh)) {
		 
		INIT_WORK(&ctx->work, verify_bh);
		fsverity_enqueue_verify_work(&ctx->work);
		return;
	}
	end_buffer_async_read(bh, err == 0);
	kfree(ctx);
}

 
static void end_buffer_async_read_io(struct buffer_head *bh, int uptodate)
{
	struct inode *inode = bh->b_folio->mapping->host;
	bool decrypt = fscrypt_inode_uses_fs_layer_crypto(inode);
	bool verify = need_fsverity(bh);

	 
	if (uptodate && (decrypt || verify)) {
		struct postprocess_bh_ctx *ctx =
			kmalloc(sizeof(*ctx), GFP_ATOMIC);

		if (ctx) {
			ctx->bh = bh;
			if (decrypt) {
				INIT_WORK(&ctx->work, decrypt_bh);
				fscrypt_enqueue_decrypt_work(&ctx->work);
			} else {
				INIT_WORK(&ctx->work, verify_bh);
				fsverity_enqueue_verify_work(&ctx->work);
			}
			return;
		}
		uptodate = 0;
	}
	end_buffer_async_read(bh, uptodate);
}

 
void end_buffer_async_write(struct buffer_head *bh, int uptodate)
{
	unsigned long flags;
	struct buffer_head *first;
	struct buffer_head *tmp;
	struct folio *folio;

	BUG_ON(!buffer_async_write(bh));

	folio = bh->b_folio;
	if (uptodate) {
		set_buffer_uptodate(bh);
	} else {
		buffer_io_error(bh, ", lost async page write");
		mark_buffer_write_io_error(bh);
		clear_buffer_uptodate(bh);
		folio_set_error(folio);
	}

	first = folio_buffers(folio);
	spin_lock_irqsave(&first->b_uptodate_lock, flags);

	clear_buffer_async_write(bh);
	unlock_buffer(bh);
	tmp = bh->b_this_page;
	while (tmp != bh) {
		if (buffer_async_write(tmp)) {
			BUG_ON(!buffer_locked(tmp));
			goto still_busy;
		}
		tmp = tmp->b_this_page;
	}
	spin_unlock_irqrestore(&first->b_uptodate_lock, flags);
	folio_end_writeback(folio);
	return;

still_busy:
	spin_unlock_irqrestore(&first->b_uptodate_lock, flags);
	return;
}
EXPORT_SYMBOL(end_buffer_async_write);

 
static void mark_buffer_async_read(struct buffer_head *bh)
{
	bh->b_end_io = end_buffer_async_read_io;
	set_buffer_async_read(bh);
}

static void mark_buffer_async_write_endio(struct buffer_head *bh,
					  bh_end_io_t *handler)
{
	bh->b_end_io = handler;
	set_buffer_async_write(bh);
}

void mark_buffer_async_write(struct buffer_head *bh)
{
	mark_buffer_async_write_endio(bh, end_buffer_async_write);
}
EXPORT_SYMBOL(mark_buffer_async_write);


 

 
static void __remove_assoc_queue(struct buffer_head *bh)
{
	list_del_init(&bh->b_assoc_buffers);
	WARN_ON(!bh->b_assoc_map);
	bh->b_assoc_map = NULL;
}

int inode_has_buffers(struct inode *inode)
{
	return !list_empty(&inode->i_data.private_list);
}

 
static int osync_buffers_list(spinlock_t *lock, struct list_head *list)
{
	struct buffer_head *bh;
	struct list_head *p;
	int err = 0;

	spin_lock(lock);
repeat:
	list_for_each_prev(p, list) {
		bh = BH_ENTRY(p);
		if (buffer_locked(bh)) {
			get_bh(bh);
			spin_unlock(lock);
			wait_on_buffer(bh);
			if (!buffer_uptodate(bh))
				err = -EIO;
			brelse(bh);
			spin_lock(lock);
			goto repeat;
		}
	}
	spin_unlock(lock);
	return err;
}

 
int sync_mapping_buffers(struct address_space *mapping)
{
	struct address_space *buffer_mapping = mapping->private_data;

	if (buffer_mapping == NULL || list_empty(&mapping->private_list))
		return 0;

	return fsync_buffers_list(&buffer_mapping->private_lock,
					&mapping->private_list);
}
EXPORT_SYMBOL(sync_mapping_buffers);

 
int generic_buffers_fsync_noflush(struct file *file, loff_t start, loff_t end,
				  bool datasync)
{
	struct inode *inode = file->f_mapping->host;
	int err;
	int ret;

	err = file_write_and_wait_range(file, start, end);
	if (err)
		return err;

	ret = sync_mapping_buffers(inode->i_mapping);
	if (!(inode->i_state & I_DIRTY_ALL))
		goto out;
	if (datasync && !(inode->i_state & I_DIRTY_DATASYNC))
		goto out;

	err = sync_inode_metadata(inode, 1);
	if (ret == 0)
		ret = err;

out:
	 
	err = file_check_and_advance_wb_err(file);
	if (ret == 0)
		ret = err;
	return ret;
}
EXPORT_SYMBOL(generic_buffers_fsync_noflush);

 
int generic_buffers_fsync(struct file *file, loff_t start, loff_t end,
			  bool datasync)
{
	struct inode *inode = file->f_mapping->host;
	int ret;

	ret = generic_buffers_fsync_noflush(file, start, end, datasync);
	if (!ret)
		ret = blkdev_issue_flush(inode->i_sb->s_bdev);
	return ret;
}
EXPORT_SYMBOL(generic_buffers_fsync);

 
void write_boundary_block(struct block_device *bdev,
			sector_t bblock, unsigned blocksize)
{
	struct buffer_head *bh = __find_get_block(bdev, bblock + 1, blocksize);
	if (bh) {
		if (buffer_dirty(bh))
			write_dirty_buffer(bh, 0);
		put_bh(bh);
	}
}

void mark_buffer_dirty_inode(struct buffer_head *bh, struct inode *inode)
{
	struct address_space *mapping = inode->i_mapping;
	struct address_space *buffer_mapping = bh->b_folio->mapping;

	mark_buffer_dirty(bh);
	if (!mapping->private_data) {
		mapping->private_data = buffer_mapping;
	} else {
		BUG_ON(mapping->private_data != buffer_mapping);
	}
	if (!bh->b_assoc_map) {
		spin_lock(&buffer_mapping->private_lock);
		list_move_tail(&bh->b_assoc_buffers,
				&mapping->private_list);
		bh->b_assoc_map = mapping;
		spin_unlock(&buffer_mapping->private_lock);
	}
}
EXPORT_SYMBOL(mark_buffer_dirty_inode);

 
bool block_dirty_folio(struct address_space *mapping, struct folio *folio)
{
	struct buffer_head *head;
	bool newly_dirty;

	spin_lock(&mapping->private_lock);
	head = folio_buffers(folio);
	if (head) {
		struct buffer_head *bh = head;

		do {
			set_buffer_dirty(bh);
			bh = bh->b_this_page;
		} while (bh != head);
	}
	 
	folio_memcg_lock(folio);
	newly_dirty = !folio_test_set_dirty(folio);
	spin_unlock(&mapping->private_lock);

	if (newly_dirty)
		__folio_mark_dirty(folio, mapping, 1);

	folio_memcg_unlock(folio);

	if (newly_dirty)
		__mark_inode_dirty(mapping->host, I_DIRTY_PAGES);

	return newly_dirty;
}
EXPORT_SYMBOL(block_dirty_folio);

 
static int fsync_buffers_list(spinlock_t *lock, struct list_head *list)
{
	struct buffer_head *bh;
	struct list_head tmp;
	struct address_space *mapping;
	int err = 0, err2;
	struct blk_plug plug;

	INIT_LIST_HEAD(&tmp);
	blk_start_plug(&plug);

	spin_lock(lock);
	while (!list_empty(list)) {
		bh = BH_ENTRY(list->next);
		mapping = bh->b_assoc_map;
		__remove_assoc_queue(bh);
		 
		smp_mb();
		if (buffer_dirty(bh) || buffer_locked(bh)) {
			list_add(&bh->b_assoc_buffers, &tmp);
			bh->b_assoc_map = mapping;
			if (buffer_dirty(bh)) {
				get_bh(bh);
				spin_unlock(lock);
				 
				write_dirty_buffer(bh, REQ_SYNC);

				 
				brelse(bh);
				spin_lock(lock);
			}
		}
	}

	spin_unlock(lock);
	blk_finish_plug(&plug);
	spin_lock(lock);

	while (!list_empty(&tmp)) {
		bh = BH_ENTRY(tmp.prev);
		get_bh(bh);
		mapping = bh->b_assoc_map;
		__remove_assoc_queue(bh);
		 
		smp_mb();
		if (buffer_dirty(bh)) {
			list_add(&bh->b_assoc_buffers,
				 &mapping->private_list);
			bh->b_assoc_map = mapping;
		}
		spin_unlock(lock);
		wait_on_buffer(bh);
		if (!buffer_uptodate(bh))
			err = -EIO;
		brelse(bh);
		spin_lock(lock);
	}
	
	spin_unlock(lock);
	err2 = osync_buffers_list(lock, list);
	if (err)
		return err;
	else
		return err2;
}

 
void invalidate_inode_buffers(struct inode *inode)
{
	if (inode_has_buffers(inode)) {
		struct address_space *mapping = &inode->i_data;
		struct list_head *list = &mapping->private_list;
		struct address_space *buffer_mapping = mapping->private_data;

		spin_lock(&buffer_mapping->private_lock);
		while (!list_empty(list))
			__remove_assoc_queue(BH_ENTRY(list->next));
		spin_unlock(&buffer_mapping->private_lock);
	}
}
EXPORT_SYMBOL(invalidate_inode_buffers);

 
int remove_inode_buffers(struct inode *inode)
{
	int ret = 1;

	if (inode_has_buffers(inode)) {
		struct address_space *mapping = &inode->i_data;
		struct list_head *list = &mapping->private_list;
		struct address_space *buffer_mapping = mapping->private_data;

		spin_lock(&buffer_mapping->private_lock);
		while (!list_empty(list)) {
			struct buffer_head *bh = BH_ENTRY(list->next);
			if (buffer_dirty(bh)) {
				ret = 0;
				break;
			}
			__remove_assoc_queue(bh);
		}
		spin_unlock(&buffer_mapping->private_lock);
	}
	return ret;
}

 
struct buffer_head *folio_alloc_buffers(struct folio *folio, unsigned long size,
					bool retry)
{
	struct buffer_head *bh, *head;
	gfp_t gfp = GFP_NOFS | __GFP_ACCOUNT;
	long offset;
	struct mem_cgroup *memcg, *old_memcg;

	if (retry)
		gfp |= __GFP_NOFAIL;

	 
	memcg = folio_memcg(folio);
	old_memcg = set_active_memcg(memcg);

	head = NULL;
	offset = folio_size(folio);
	while ((offset -= size) >= 0) {
		bh = alloc_buffer_head(gfp);
		if (!bh)
			goto no_grow;

		bh->b_this_page = head;
		bh->b_blocknr = -1;
		head = bh;

		bh->b_size = size;

		 
		folio_set_bh(bh, folio, offset);
	}
out:
	set_active_memcg(old_memcg);
	return head;
 
no_grow:
	if (head) {
		do {
			bh = head;
			head = head->b_this_page;
			free_buffer_head(bh);
		} while (head);
	}

	goto out;
}
EXPORT_SYMBOL_GPL(folio_alloc_buffers);

struct buffer_head *alloc_page_buffers(struct page *page, unsigned long size,
				       bool retry)
{
	return folio_alloc_buffers(page_folio(page), size, retry);
}
EXPORT_SYMBOL_GPL(alloc_page_buffers);

static inline void link_dev_buffers(struct folio *folio,
		struct buffer_head *head)
{
	struct buffer_head *bh, *tail;

	bh = head;
	do {
		tail = bh;
		bh = bh->b_this_page;
	} while (bh);
	tail->b_this_page = head;
	folio_attach_private(folio, head);
}

static sector_t blkdev_max_block(struct block_device *bdev, unsigned int size)
{
	sector_t retval = ~((sector_t)0);
	loff_t sz = bdev_nr_bytes(bdev);

	if (sz) {
		unsigned int sizebits = blksize_bits(size);
		retval = (sz >> sizebits);
	}
	return retval;
}

  
static sector_t folio_init_buffers(struct folio *folio,
		struct block_device *bdev, sector_t block, int size)
{
	struct buffer_head *head = folio_buffers(folio);
	struct buffer_head *bh = head;
	bool uptodate = folio_test_uptodate(folio);
	sector_t end_block = blkdev_max_block(bdev, size);

	do {
		if (!buffer_mapped(bh)) {
			bh->b_end_io = NULL;
			bh->b_private = NULL;
			bh->b_bdev = bdev;
			bh->b_blocknr = block;
			if (uptodate)
				set_buffer_uptodate(bh);
			if (block < end_block)
				set_buffer_mapped(bh);
		}
		block++;
		bh = bh->b_this_page;
	} while (bh != head);

	 
	return end_block;
}

 
static int
grow_dev_page(struct block_device *bdev, sector_t block,
	      pgoff_t index, int size, int sizebits, gfp_t gfp)
{
	struct inode *inode = bdev->bd_inode;
	struct folio *folio;
	struct buffer_head *bh;
	sector_t end_block;
	int ret = 0;
	gfp_t gfp_mask;

	gfp_mask = mapping_gfp_constraint(inode->i_mapping, ~__GFP_FS) | gfp;

	 
	gfp_mask |= __GFP_NOFAIL;

	folio = __filemap_get_folio(inode->i_mapping, index,
			FGP_LOCK | FGP_ACCESSED | FGP_CREAT, gfp_mask);

	bh = folio_buffers(folio);
	if (bh) {
		if (bh->b_size == size) {
			end_block = folio_init_buffers(folio, bdev,
					(sector_t)index << sizebits, size);
			goto done;
		}
		if (!try_to_free_buffers(folio))
			goto failed;
	}

	bh = folio_alloc_buffers(folio, size, true);

	 
	spin_lock(&inode->i_mapping->private_lock);
	link_dev_buffers(folio, bh);
	end_block = folio_init_buffers(folio, bdev,
			(sector_t)index << sizebits, size);
	spin_unlock(&inode->i_mapping->private_lock);
done:
	ret = (block < end_block) ? 1 : -ENXIO;
failed:
	folio_unlock(folio);
	folio_put(folio);
	return ret;
}

 
static int
grow_buffers(struct block_device *bdev, sector_t block, int size, gfp_t gfp)
{
	pgoff_t index;
	int sizebits;

	sizebits = PAGE_SHIFT - __ffs(size);
	index = block >> sizebits;

	 
	if (unlikely(index != block >> sizebits)) {
		printk(KERN_ERR "%s: requested out-of-range block %llu for "
			"device %pg\n",
			__func__, (unsigned long long)block,
			bdev);
		return -EIO;
	}

	 
	return grow_dev_page(bdev, block, index, size, sizebits, gfp);
}

static struct buffer_head *
__getblk_slow(struct block_device *bdev, sector_t block,
	     unsigned size, gfp_t gfp)
{
	 
	if (unlikely(size & (bdev_logical_block_size(bdev)-1) ||
			(size < 512 || size > PAGE_SIZE))) {
		printk(KERN_ERR "getblk(): invalid block size %d requested\n",
					size);
		printk(KERN_ERR "logical block size: %d\n",
					bdev_logical_block_size(bdev));

		dump_stack();
		return NULL;
	}

	for (;;) {
		struct buffer_head *bh;
		int ret;

		bh = __find_get_block(bdev, block, size);
		if (bh)
			return bh;

		ret = grow_buffers(bdev, block, size, gfp);
		if (ret < 0)
			return NULL;
	}
}

 

 
void mark_buffer_dirty(struct buffer_head *bh)
{
	WARN_ON_ONCE(!buffer_uptodate(bh));

	trace_block_dirty_buffer(bh);

	 
	if (buffer_dirty(bh)) {
		smp_mb();
		if (buffer_dirty(bh))
			return;
	}

	if (!test_set_buffer_dirty(bh)) {
		struct folio *folio = bh->b_folio;
		struct address_space *mapping = NULL;

		folio_memcg_lock(folio);
		if (!folio_test_set_dirty(folio)) {
			mapping = folio->mapping;
			if (mapping)
				__folio_mark_dirty(folio, mapping, 0);
		}
		folio_memcg_unlock(folio);
		if (mapping)
			__mark_inode_dirty(mapping->host, I_DIRTY_PAGES);
	}
}
EXPORT_SYMBOL(mark_buffer_dirty);

void mark_buffer_write_io_error(struct buffer_head *bh)
{
	set_buffer_write_io_error(bh);
	 
	if (bh->b_folio && bh->b_folio->mapping)
		mapping_set_error(bh->b_folio->mapping, -EIO);
	if (bh->b_assoc_map) {
		mapping_set_error(bh->b_assoc_map, -EIO);
		errseq_set(&bh->b_assoc_map->host->i_sb->s_wb_err, -EIO);
	}
}
EXPORT_SYMBOL(mark_buffer_write_io_error);

 
void __brelse(struct buffer_head * buf)
{
	if (atomic_read(&buf->b_count)) {
		put_bh(buf);
		return;
	}
	WARN(1, KERN_ERR "VFS: brelse: Trying to free free buffer\n");
}
EXPORT_SYMBOL(__brelse);

 
void __bforget(struct buffer_head *bh)
{
	clear_buffer_dirty(bh);
	if (bh->b_assoc_map) {
		struct address_space *buffer_mapping = bh->b_folio->mapping;

		spin_lock(&buffer_mapping->private_lock);
		list_del_init(&bh->b_assoc_buffers);
		bh->b_assoc_map = NULL;
		spin_unlock(&buffer_mapping->private_lock);
	}
	__brelse(bh);
}
EXPORT_SYMBOL(__bforget);

static struct buffer_head *__bread_slow(struct buffer_head *bh)
{
	lock_buffer(bh);
	if (buffer_uptodate(bh)) {
		unlock_buffer(bh);
		return bh;
	} else {
		get_bh(bh);
		bh->b_end_io = end_buffer_read_sync;
		submit_bh(REQ_OP_READ, bh);
		wait_on_buffer(bh);
		if (buffer_uptodate(bh))
			return bh;
	}
	brelse(bh);
	return NULL;
}

 

#define BH_LRU_SIZE	16

struct bh_lru {
	struct buffer_head *bhs[BH_LRU_SIZE];
};

static DEFINE_PER_CPU(struct bh_lru, bh_lrus) = {{ NULL }};

#ifdef CONFIG_SMP
#define bh_lru_lock()	local_irq_disable()
#define bh_lru_unlock()	local_irq_enable()
#else
#define bh_lru_lock()	preempt_disable()
#define bh_lru_unlock()	preempt_enable()
#endif

static inline void check_irqs_on(void)
{
#ifdef irqs_disabled
	BUG_ON(irqs_disabled());
#endif
}

 
static void bh_lru_install(struct buffer_head *bh)
{
	struct buffer_head *evictee = bh;
	struct bh_lru *b;
	int i;

	check_irqs_on();
	bh_lru_lock();

	 
	if (lru_cache_disabled() || cpu_is_isolated(smp_processor_id())) {
		bh_lru_unlock();
		return;
	}

	b = this_cpu_ptr(&bh_lrus);
	for (i = 0; i < BH_LRU_SIZE; i++) {
		swap(evictee, b->bhs[i]);
		if (evictee == bh) {
			bh_lru_unlock();
			return;
		}
	}

	get_bh(bh);
	bh_lru_unlock();
	brelse(evictee);
}

 
static struct buffer_head *
lookup_bh_lru(struct block_device *bdev, sector_t block, unsigned size)
{
	struct buffer_head *ret = NULL;
	unsigned int i;

	check_irqs_on();
	bh_lru_lock();
	if (cpu_is_isolated(smp_processor_id())) {
		bh_lru_unlock();
		return NULL;
	}
	for (i = 0; i < BH_LRU_SIZE; i++) {
		struct buffer_head *bh = __this_cpu_read(bh_lrus.bhs[i]);

		if (bh && bh->b_blocknr == block && bh->b_bdev == bdev &&
		    bh->b_size == size) {
			if (i) {
				while (i) {
					__this_cpu_write(bh_lrus.bhs[i],
						__this_cpu_read(bh_lrus.bhs[i - 1]));
					i--;
				}
				__this_cpu_write(bh_lrus.bhs[0], bh);
			}
			get_bh(bh);
			ret = bh;
			break;
		}
	}
	bh_lru_unlock();
	return ret;
}

 
struct buffer_head *
__find_get_block(struct block_device *bdev, sector_t block, unsigned size)
{
	struct buffer_head *bh = lookup_bh_lru(bdev, block, size);

	if (bh == NULL) {
		 
		bh = __find_get_block_slow(bdev, block);
		if (bh)
			bh_lru_install(bh);
	} else
		touch_buffer(bh);

	return bh;
}
EXPORT_SYMBOL(__find_get_block);

 
struct buffer_head *
__getblk_gfp(struct block_device *bdev, sector_t block,
	     unsigned size, gfp_t gfp)
{
	struct buffer_head *bh = __find_get_block(bdev, block, size);

	might_sleep();
	if (bh == NULL)
		bh = __getblk_slow(bdev, block, size, gfp);
	return bh;
}
EXPORT_SYMBOL(__getblk_gfp);

 
void __breadahead(struct block_device *bdev, sector_t block, unsigned size)
{
	struct buffer_head *bh = __getblk(bdev, block, size);
	if (likely(bh)) {
		bh_readahead(bh, REQ_RAHEAD);
		brelse(bh);
	}
}
EXPORT_SYMBOL(__breadahead);

 
struct buffer_head *
__bread_gfp(struct block_device *bdev, sector_t block,
		   unsigned size, gfp_t gfp)
{
	struct buffer_head *bh = __getblk_gfp(bdev, block, size, gfp);

	if (likely(bh) && !buffer_uptodate(bh))
		bh = __bread_slow(bh);
	return bh;
}
EXPORT_SYMBOL(__bread_gfp);

static void __invalidate_bh_lrus(struct bh_lru *b)
{
	int i;

	for (i = 0; i < BH_LRU_SIZE; i++) {
		brelse(b->bhs[i]);
		b->bhs[i] = NULL;
	}
}
 
static void invalidate_bh_lru(void *arg)
{
	struct bh_lru *b = &get_cpu_var(bh_lrus);

	__invalidate_bh_lrus(b);
	put_cpu_var(bh_lrus);
}

bool has_bh_in_lru(int cpu, void *dummy)
{
	struct bh_lru *b = per_cpu_ptr(&bh_lrus, cpu);
	int i;
	
	for (i = 0; i < BH_LRU_SIZE; i++) {
		if (b->bhs[i])
			return true;
	}

	return false;
}

void invalidate_bh_lrus(void)
{
	on_each_cpu_cond(has_bh_in_lru, invalidate_bh_lru, NULL, 1);
}
EXPORT_SYMBOL_GPL(invalidate_bh_lrus);

 
void invalidate_bh_lrus_cpu(void)
{
	struct bh_lru *b;

	bh_lru_lock();
	b = this_cpu_ptr(&bh_lrus);
	__invalidate_bh_lrus(b);
	bh_lru_unlock();
}

void folio_set_bh(struct buffer_head *bh, struct folio *folio,
		  unsigned long offset)
{
	bh->b_folio = folio;
	BUG_ON(offset >= folio_size(folio));
	if (folio_test_highmem(folio))
		 
		bh->b_data = (char *)(0 + offset);
	else
		bh->b_data = folio_address(folio) + offset;
}
EXPORT_SYMBOL(folio_set_bh);

 

 
#define BUFFER_FLAGS_DISCARD \
	(1 << BH_Mapped | 1 << BH_New | 1 << BH_Req | \
	 1 << BH_Delay | 1 << BH_Unwritten)

static void discard_buffer(struct buffer_head * bh)
{
	unsigned long b_state;

	lock_buffer(bh);
	clear_buffer_dirty(bh);
	bh->b_bdev = NULL;
	b_state = READ_ONCE(bh->b_state);
	do {
	} while (!try_cmpxchg(&bh->b_state, &b_state,
			      b_state & ~BUFFER_FLAGS_DISCARD));
	unlock_buffer(bh);
}

 
void block_invalidate_folio(struct folio *folio, size_t offset, size_t length)
{
	struct buffer_head *head, *bh, *next;
	size_t curr_off = 0;
	size_t stop = length + offset;

	BUG_ON(!folio_test_locked(folio));

	 
	BUG_ON(stop > folio_size(folio) || stop < length);

	head = folio_buffers(folio);
	if (!head)
		return;

	bh = head;
	do {
		size_t next_off = curr_off + bh->b_size;
		next = bh->b_this_page;

		 
		if (next_off > stop)
			goto out;

		 
		if (offset <= curr_off)
			discard_buffer(bh);
		curr_off = next_off;
		bh = next;
	} while (bh != head);

	 
	if (length == folio_size(folio))
		filemap_release_folio(folio, 0);
out:
	return;
}
EXPORT_SYMBOL(block_invalidate_folio);

 
void folio_create_empty_buffers(struct folio *folio, unsigned long blocksize,
				unsigned long b_state)
{
	struct buffer_head *bh, *head, *tail;

	head = folio_alloc_buffers(folio, blocksize, true);
	bh = head;
	do {
		bh->b_state |= b_state;
		tail = bh;
		bh = bh->b_this_page;
	} while (bh);
	tail->b_this_page = head;

	spin_lock(&folio->mapping->private_lock);
	if (folio_test_uptodate(folio) || folio_test_dirty(folio)) {
		bh = head;
		do {
			if (folio_test_dirty(folio))
				set_buffer_dirty(bh);
			if (folio_test_uptodate(folio))
				set_buffer_uptodate(bh);
			bh = bh->b_this_page;
		} while (bh != head);
	}
	folio_attach_private(folio, head);
	spin_unlock(&folio->mapping->private_lock);
}
EXPORT_SYMBOL(folio_create_empty_buffers);

void create_empty_buffers(struct page *page,
			unsigned long blocksize, unsigned long b_state)
{
	folio_create_empty_buffers(page_folio(page), blocksize, b_state);
}
EXPORT_SYMBOL(create_empty_buffers);

 
void clean_bdev_aliases(struct block_device *bdev, sector_t block, sector_t len)
{
	struct inode *bd_inode = bdev->bd_inode;
	struct address_space *bd_mapping = bd_inode->i_mapping;
	struct folio_batch fbatch;
	pgoff_t index = block >> (PAGE_SHIFT - bd_inode->i_blkbits);
	pgoff_t end;
	int i, count;
	struct buffer_head *bh;
	struct buffer_head *head;

	end = (block + len - 1) >> (PAGE_SHIFT - bd_inode->i_blkbits);
	folio_batch_init(&fbatch);
	while (filemap_get_folios(bd_mapping, &index, end, &fbatch)) {
		count = folio_batch_count(&fbatch);
		for (i = 0; i < count; i++) {
			struct folio *folio = fbatch.folios[i];

			if (!folio_buffers(folio))
				continue;
			 
			folio_lock(folio);
			 
			head = folio_buffers(folio);
			if (!head)
				goto unlock_page;
			bh = head;
			do {
				if (!buffer_mapped(bh) || (bh->b_blocknr < block))
					goto next;
				if (bh->b_blocknr >= block + len)
					break;
				clear_buffer_dirty(bh);
				wait_on_buffer(bh);
				clear_buffer_req(bh);
next:
				bh = bh->b_this_page;
			} while (bh != head);
unlock_page:
			folio_unlock(folio);
		}
		folio_batch_release(&fbatch);
		cond_resched();
		 
		if (index > end || !index)
			break;
	}
}
EXPORT_SYMBOL(clean_bdev_aliases);

 
static inline int block_size_bits(unsigned int blocksize)
{
	return ilog2(blocksize);
}

static struct buffer_head *folio_create_buffers(struct folio *folio,
						struct inode *inode,
						unsigned int b_state)
{
	BUG_ON(!folio_test_locked(folio));

	if (!folio_buffers(folio))
		folio_create_empty_buffers(folio,
					   1 << READ_ONCE(inode->i_blkbits),
					   b_state);
	return folio_buffers(folio);
}

 

 
int __block_write_full_folio(struct inode *inode, struct folio *folio,
			get_block_t *get_block, struct writeback_control *wbc,
			bh_end_io_t *handler)
{
	int err;
	sector_t block;
	sector_t last_block;
	struct buffer_head *bh, *head;
	unsigned int blocksize, bbits;
	int nr_underway = 0;
	blk_opf_t write_flags = wbc_to_write_flags(wbc);

	head = folio_create_buffers(folio, inode,
				    (1 << BH_Dirty) | (1 << BH_Uptodate));

	 

	bh = head;
	blocksize = bh->b_size;
	bbits = block_size_bits(blocksize);

	block = (sector_t)folio->index << (PAGE_SHIFT - bbits);
	last_block = (i_size_read(inode) - 1) >> bbits;

	 
	do {
		if (block > last_block) {
			 
			 
			clear_buffer_dirty(bh);
			set_buffer_uptodate(bh);
		} else if ((!buffer_mapped(bh) || buffer_delay(bh)) &&
			   buffer_dirty(bh)) {
			WARN_ON(bh->b_size != blocksize);
			err = get_block(inode, block, bh, 1);
			if (err)
				goto recover;
			clear_buffer_delay(bh);
			if (buffer_new(bh)) {
				 
				clear_buffer_new(bh);
				clean_bdev_bh_alias(bh);
			}
		}
		bh = bh->b_this_page;
		block++;
	} while (bh != head);

	do {
		if (!buffer_mapped(bh))
			continue;
		 
		if (wbc->sync_mode != WB_SYNC_NONE) {
			lock_buffer(bh);
		} else if (!trylock_buffer(bh)) {
			folio_redirty_for_writepage(wbc, folio);
			continue;
		}
		if (test_clear_buffer_dirty(bh)) {
			mark_buffer_async_write_endio(bh, handler);
		} else {
			unlock_buffer(bh);
		}
	} while ((bh = bh->b_this_page) != head);

	 
	BUG_ON(folio_test_writeback(folio));
	folio_start_writeback(folio);

	do {
		struct buffer_head *next = bh->b_this_page;
		if (buffer_async_write(bh)) {
			submit_bh_wbc(REQ_OP_WRITE | write_flags, bh, wbc);
			nr_underway++;
		}
		bh = next;
	} while (bh != head);
	folio_unlock(folio);

	err = 0;
done:
	if (nr_underway == 0) {
		 
		folio_end_writeback(folio);

		 
	}
	return err;

recover:
	 
	bh = head;
	 
	do {
		if (buffer_mapped(bh) && buffer_dirty(bh) &&
		    !buffer_delay(bh)) {
			lock_buffer(bh);
			mark_buffer_async_write_endio(bh, handler);
		} else {
			 
			clear_buffer_dirty(bh);
		}
	} while ((bh = bh->b_this_page) != head);
	folio_set_error(folio);
	BUG_ON(folio_test_writeback(folio));
	mapping_set_error(folio->mapping, err);
	folio_start_writeback(folio);
	do {
		struct buffer_head *next = bh->b_this_page;
		if (buffer_async_write(bh)) {
			clear_buffer_dirty(bh);
			submit_bh_wbc(REQ_OP_WRITE | write_flags, bh, wbc);
			nr_underway++;
		}
		bh = next;
	} while (bh != head);
	folio_unlock(folio);
	goto done;
}
EXPORT_SYMBOL(__block_write_full_folio);

 
void folio_zero_new_buffers(struct folio *folio, size_t from, size_t to)
{
	size_t block_start, block_end;
	struct buffer_head *head, *bh;

	BUG_ON(!folio_test_locked(folio));
	head = folio_buffers(folio);
	if (!head)
		return;

	bh = head;
	block_start = 0;
	do {
		block_end = block_start + bh->b_size;

		if (buffer_new(bh)) {
			if (block_end > from && block_start < to) {
				if (!folio_test_uptodate(folio)) {
					size_t start, xend;

					start = max(from, block_start);
					xend = min(to, block_end);

					folio_zero_segment(folio, start, xend);
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
EXPORT_SYMBOL(folio_zero_new_buffers);

static int
iomap_to_bh(struct inode *inode, sector_t block, struct buffer_head *bh,
		const struct iomap *iomap)
{
	loff_t offset = block << inode->i_blkbits;

	bh->b_bdev = iomap->bdev;

	 
	if (offset >= iomap->offset + iomap->length)
		return -EIO;

	switch (iomap->type) {
	case IOMAP_HOLE:
		 
		if (!buffer_uptodate(bh) ||
		    (offset >= i_size_read(inode)))
			set_buffer_new(bh);
		return 0;
	case IOMAP_DELALLOC:
		if (!buffer_uptodate(bh) ||
		    (offset >= i_size_read(inode)))
			set_buffer_new(bh);
		set_buffer_uptodate(bh);
		set_buffer_mapped(bh);
		set_buffer_delay(bh);
		return 0;
	case IOMAP_UNWRITTEN:
		 
		set_buffer_new(bh);
		set_buffer_unwritten(bh);
		fallthrough;
	case IOMAP_MAPPED:
		if ((iomap->flags & IOMAP_F_NEW) ||
		    offset >= i_size_read(inode)) {
			 
			if (S_ISBLK(inode->i_mode))
				return -EIO;
			set_buffer_new(bh);
		}
		bh->b_blocknr = (iomap->addr + offset - iomap->offset) >>
				inode->i_blkbits;
		set_buffer_mapped(bh);
		return 0;
	default:
		WARN_ON_ONCE(1);
		return -EIO;
	}
}

int __block_write_begin_int(struct folio *folio, loff_t pos, unsigned len,
		get_block_t *get_block, const struct iomap *iomap)
{
	unsigned from = pos & (PAGE_SIZE - 1);
	unsigned to = from + len;
	struct inode *inode = folio->mapping->host;
	unsigned block_start, block_end;
	sector_t block;
	int err = 0;
	unsigned blocksize, bbits;
	struct buffer_head *bh, *head, *wait[2], **wait_bh=wait;

	BUG_ON(!folio_test_locked(folio));
	BUG_ON(from > PAGE_SIZE);
	BUG_ON(to > PAGE_SIZE);
	BUG_ON(from > to);

	head = folio_create_buffers(folio, inode, 0);
	blocksize = head->b_size;
	bbits = block_size_bits(blocksize);

	block = (sector_t)folio->index << (PAGE_SHIFT - bbits);

	for(bh = head, block_start = 0; bh != head || !block_start;
	    block++, block_start=block_end, bh = bh->b_this_page) {
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (folio_test_uptodate(folio)) {
				if (!buffer_uptodate(bh))
					set_buffer_uptodate(bh);
			}
			continue;
		}
		if (buffer_new(bh))
			clear_buffer_new(bh);
		if (!buffer_mapped(bh)) {
			WARN_ON(bh->b_size != blocksize);
			if (get_block)
				err = get_block(inode, block, bh, 1);
			else
				err = iomap_to_bh(inode, block, bh, iomap);
			if (err)
				break;

			if (buffer_new(bh)) {
				clean_bdev_bh_alias(bh);
				if (folio_test_uptodate(folio)) {
					clear_buffer_new(bh);
					set_buffer_uptodate(bh);
					mark_buffer_dirty(bh);
					continue;
				}
				if (block_end > to || block_start < from)
					folio_zero_segments(folio,
						to, block_end,
						block_start, from);
				continue;
			}
		}
		if (folio_test_uptodate(folio)) {
			if (!buffer_uptodate(bh))
				set_buffer_uptodate(bh);
			continue; 
		}
		if (!buffer_uptodate(bh) && !buffer_delay(bh) &&
		    !buffer_unwritten(bh) &&
		     (block_start < from || block_end > to)) {
			bh_read_nowait(bh, 0);
			*wait_bh++=bh;
		}
	}
	 
	while(wait_bh > wait) {
		wait_on_buffer(*--wait_bh);
		if (!buffer_uptodate(*wait_bh))
			err = -EIO;
	}
	if (unlikely(err))
		folio_zero_new_buffers(folio, from, to);
	return err;
}

int __block_write_begin(struct page *page, loff_t pos, unsigned len,
		get_block_t *get_block)
{
	return __block_write_begin_int(page_folio(page), pos, len, get_block,
				       NULL);
}
EXPORT_SYMBOL(__block_write_begin);

static void __block_commit_write(struct folio *folio, size_t from, size_t to)
{
	size_t block_start, block_end;
	bool partial = false;
	unsigned blocksize;
	struct buffer_head *bh, *head;

	bh = head = folio_buffers(folio);
	blocksize = bh->b_size;

	block_start = 0;
	do {
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (!buffer_uptodate(bh))
				partial = true;
		} else {
			set_buffer_uptodate(bh);
			mark_buffer_dirty(bh);
		}
		if (buffer_new(bh))
			clear_buffer_new(bh);

		block_start = block_end;
		bh = bh->b_this_page;
	} while (bh != head);

	 
	if (!partial)
		folio_mark_uptodate(folio);
}

 
int block_write_begin(struct address_space *mapping, loff_t pos, unsigned len,
		struct page **pagep, get_block_t *get_block)
{
	pgoff_t index = pos >> PAGE_SHIFT;
	struct page *page;
	int status;

	page = grab_cache_page_write_begin(mapping, index);
	if (!page)
		return -ENOMEM;

	status = __block_write_begin(page, pos, len, get_block);
	if (unlikely(status)) {
		unlock_page(page);
		put_page(page);
		page = NULL;
	}

	*pagep = page;
	return status;
}
EXPORT_SYMBOL(block_write_begin);

int block_write_end(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata)
{
	struct folio *folio = page_folio(page);
	size_t start = pos - folio_pos(folio);

	if (unlikely(copied < len)) {
		 
		if (!folio_test_uptodate(folio))
			copied = 0;

		folio_zero_new_buffers(folio, start+copied, start+len);
	}
	flush_dcache_folio(folio);

	 
	__block_commit_write(folio, start, start + copied);

	return copied;
}
EXPORT_SYMBOL(block_write_end);

int generic_write_end(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	loff_t old_size = inode->i_size;
	bool i_size_changed = false;

	copied = block_write_end(file, mapping, pos, len, copied, page, fsdata);

	 
	if (pos + copied > inode->i_size) {
		i_size_write(inode, pos + copied);
		i_size_changed = true;
	}

	unlock_page(page);
	put_page(page);

	if (old_size < pos)
		pagecache_isize_extended(inode, old_size, pos);
	 
	if (i_size_changed)
		mark_inode_dirty(inode);
	return copied;
}
EXPORT_SYMBOL(generic_write_end);

 
bool block_is_partially_uptodate(struct folio *folio, size_t from, size_t count)
{
	unsigned block_start, block_end, blocksize;
	unsigned to;
	struct buffer_head *bh, *head;
	bool ret = true;

	head = folio_buffers(folio);
	if (!head)
		return false;
	blocksize = head->b_size;
	to = min_t(unsigned, folio_size(folio) - from, count);
	to = from + to;
	if (from < blocksize && to > folio_size(folio) - blocksize)
		return false;

	bh = head;
	block_start = 0;
	do {
		block_end = block_start + blocksize;
		if (block_end > from && block_start < to) {
			if (!buffer_uptodate(bh)) {
				ret = false;
				break;
			}
			if (block_end >= to)
				break;
		}
		block_start = block_end;
		bh = bh->b_this_page;
	} while (bh != head);

	return ret;
}
EXPORT_SYMBOL(block_is_partially_uptodate);

 
int block_read_full_folio(struct folio *folio, get_block_t *get_block)
{
	struct inode *inode = folio->mapping->host;
	sector_t iblock, lblock;
	struct buffer_head *bh, *head, *arr[MAX_BUF_PER_PAGE];
	unsigned int blocksize, bbits;
	int nr, i;
	int fully_mapped = 1;
	bool page_error = false;
	loff_t limit = i_size_read(inode);

	 
	if (IS_ENABLED(CONFIG_FS_VERITY) && IS_VERITY(inode))
		limit = inode->i_sb->s_maxbytes;

	VM_BUG_ON_FOLIO(folio_test_large(folio), folio);

	head = folio_create_buffers(folio, inode, 0);
	blocksize = head->b_size;
	bbits = block_size_bits(blocksize);

	iblock = (sector_t)folio->index << (PAGE_SHIFT - bbits);
	lblock = (limit+blocksize-1) >> bbits;
	bh = head;
	nr = 0;
	i = 0;

	do {
		if (buffer_uptodate(bh))
			continue;

		if (!buffer_mapped(bh)) {
			int err = 0;

			fully_mapped = 0;
			if (iblock < lblock) {
				WARN_ON(bh->b_size != blocksize);
				err = get_block(inode, iblock, bh, 0);
				if (err) {
					folio_set_error(folio);
					page_error = true;
				}
			}
			if (!buffer_mapped(bh)) {
				folio_zero_range(folio, i * blocksize,
						blocksize);
				if (!err)
					set_buffer_uptodate(bh);
				continue;
			}
			 
			if (buffer_uptodate(bh))
				continue;
		}
		arr[nr++] = bh;
	} while (i++, iblock++, (bh = bh->b_this_page) != head);

	if (fully_mapped)
		folio_set_mappedtodisk(folio);

	if (!nr) {
		 
		if (!page_error)
			folio_mark_uptodate(folio);
		folio_unlock(folio);
		return 0;
	}

	 
	for (i = 0; i < nr; i++) {
		bh = arr[i];
		lock_buffer(bh);
		mark_buffer_async_read(bh);
	}

	 
	for (i = 0; i < nr; i++) {
		bh = arr[i];
		if (buffer_uptodate(bh))
			end_buffer_async_read(bh, 1);
		else
			submit_bh(REQ_OP_READ, bh);
	}
	return 0;
}
EXPORT_SYMBOL(block_read_full_folio);

 
int generic_cont_expand_simple(struct inode *inode, loff_t size)
{
	struct address_space *mapping = inode->i_mapping;
	const struct address_space_operations *aops = mapping->a_ops;
	struct page *page;
	void *fsdata = NULL;
	int err;

	err = inode_newsize_ok(inode, size);
	if (err)
		goto out;

	err = aops->write_begin(NULL, mapping, size, 0, &page, &fsdata);
	if (err)
		goto out;

	err = aops->write_end(NULL, mapping, size, 0, 0, page, fsdata);
	BUG_ON(err > 0);

out:
	return err;
}
EXPORT_SYMBOL(generic_cont_expand_simple);

static int cont_expand_zero(struct file *file, struct address_space *mapping,
			    loff_t pos, loff_t *bytes)
{
	struct inode *inode = mapping->host;
	const struct address_space_operations *aops = mapping->a_ops;
	unsigned int blocksize = i_blocksize(inode);
	struct page *page;
	void *fsdata = NULL;
	pgoff_t index, curidx;
	loff_t curpos;
	unsigned zerofrom, offset, len;
	int err = 0;

	index = pos >> PAGE_SHIFT;
	offset = pos & ~PAGE_MASK;

	while (index > (curidx = (curpos = *bytes)>>PAGE_SHIFT)) {
		zerofrom = curpos & ~PAGE_MASK;
		if (zerofrom & (blocksize-1)) {
			*bytes |= (blocksize-1);
			(*bytes)++;
		}
		len = PAGE_SIZE - zerofrom;

		err = aops->write_begin(file, mapping, curpos, len,
					    &page, &fsdata);
		if (err)
			goto out;
		zero_user(page, zerofrom, len);
		err = aops->write_end(file, mapping, curpos, len, len,
						page, fsdata);
		if (err < 0)
			goto out;
		BUG_ON(err != len);
		err = 0;

		balance_dirty_pages_ratelimited(mapping);

		if (fatal_signal_pending(current)) {
			err = -EINTR;
			goto out;
		}
	}

	 
	if (index == curidx) {
		zerofrom = curpos & ~PAGE_MASK;
		 
		if (offset <= zerofrom) {
			goto out;
		}
		if (zerofrom & (blocksize-1)) {
			*bytes |= (blocksize-1);
			(*bytes)++;
		}
		len = offset - zerofrom;

		err = aops->write_begin(file, mapping, curpos, len,
					    &page, &fsdata);
		if (err)
			goto out;
		zero_user(page, zerofrom, len);
		err = aops->write_end(file, mapping, curpos, len, len,
						page, fsdata);
		if (err < 0)
			goto out;
		BUG_ON(err != len);
		err = 0;
	}
out:
	return err;
}

 
int cont_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len,
			struct page **pagep, void **fsdata,
			get_block_t *get_block, loff_t *bytes)
{
	struct inode *inode = mapping->host;
	unsigned int blocksize = i_blocksize(inode);
	unsigned int zerofrom;
	int err;

	err = cont_expand_zero(file, mapping, pos, bytes);
	if (err)
		return err;

	zerofrom = *bytes & ~PAGE_MASK;
	if (pos+len > *bytes && zerofrom & (blocksize-1)) {
		*bytes |= (blocksize-1);
		(*bytes)++;
	}

	return block_write_begin(mapping, pos, len, pagep, get_block);
}
EXPORT_SYMBOL(cont_write_begin);

void block_commit_write(struct page *page, unsigned from, unsigned to)
{
	struct folio *folio = page_folio(page);
	__block_commit_write(folio, from, to);
}
EXPORT_SYMBOL(block_commit_write);

 
int block_page_mkwrite(struct vm_area_struct *vma, struct vm_fault *vmf,
			 get_block_t get_block)
{
	struct folio *folio = page_folio(vmf->page);
	struct inode *inode = file_inode(vma->vm_file);
	unsigned long end;
	loff_t size;
	int ret;

	folio_lock(folio);
	size = i_size_read(inode);
	if ((folio->mapping != inode->i_mapping) ||
	    (folio_pos(folio) >= size)) {
		 
		ret = -EFAULT;
		goto out_unlock;
	}

	end = folio_size(folio);
	 
	if (folio_pos(folio) + end > size)
		end = size - folio_pos(folio);

	ret = __block_write_begin_int(folio, 0, end, get_block, NULL);
	if (unlikely(ret))
		goto out_unlock;

	__block_commit_write(folio, 0, end);

	folio_mark_dirty(folio);
	folio_wait_stable(folio);
	return 0;
out_unlock:
	folio_unlock(folio);
	return ret;
}
EXPORT_SYMBOL(block_page_mkwrite);

int block_truncate_page(struct address_space *mapping,
			loff_t from, get_block_t *get_block)
{
	pgoff_t index = from >> PAGE_SHIFT;
	unsigned blocksize;
	sector_t iblock;
	size_t offset, length, pos;
	struct inode *inode = mapping->host;
	struct folio *folio;
	struct buffer_head *bh;
	int err = 0;

	blocksize = i_blocksize(inode);
	length = from & (blocksize - 1);

	 
	if (!length)
		return 0;

	length = blocksize - length;
	iblock = (sector_t)index << (PAGE_SHIFT - inode->i_blkbits);
	
	folio = filemap_grab_folio(mapping, index);
	if (IS_ERR(folio))
		return PTR_ERR(folio);

	bh = folio_buffers(folio);
	if (!bh) {
		folio_create_empty_buffers(folio, blocksize, 0);
		bh = folio_buffers(folio);
	}

	 
	offset = offset_in_folio(folio, from);
	pos = blocksize;
	while (offset >= pos) {
		bh = bh->b_this_page;
		iblock++;
		pos += blocksize;
	}

	if (!buffer_mapped(bh)) {
		WARN_ON(bh->b_size != blocksize);
		err = get_block(inode, iblock, bh, 0);
		if (err)
			goto unlock;
		 
		if (!buffer_mapped(bh))
			goto unlock;
	}

	 
	if (folio_test_uptodate(folio))
		set_buffer_uptodate(bh);

	if (!buffer_uptodate(bh) && !buffer_delay(bh) && !buffer_unwritten(bh)) {
		err = bh_read(bh, 0);
		 
		if (err < 0)
			goto unlock;
	}

	folio_zero_range(folio, offset, length);
	mark_buffer_dirty(bh);

unlock:
	folio_unlock(folio);
	folio_put(folio);

	return err;
}
EXPORT_SYMBOL(block_truncate_page);

 
int block_write_full_page(struct page *page, get_block_t *get_block,
			struct writeback_control *wbc)
{
	struct folio *folio = page_folio(page);
	struct inode * const inode = folio->mapping->host;
	loff_t i_size = i_size_read(inode);

	 
	if (folio_pos(folio) + folio_size(folio) <= i_size)
		return __block_write_full_folio(inode, folio, get_block, wbc,
					       end_buffer_async_write);

	 
	if (folio_pos(folio) >= i_size) {
		folio_unlock(folio);
		return 0;  
	}

	 
	folio_zero_segment(folio, offset_in_folio(folio, i_size),
			folio_size(folio));
	return __block_write_full_folio(inode, folio, get_block, wbc,
			end_buffer_async_write);
}
EXPORT_SYMBOL(block_write_full_page);

sector_t generic_block_bmap(struct address_space *mapping, sector_t block,
			    get_block_t *get_block)
{
	struct inode *inode = mapping->host;
	struct buffer_head tmp = {
		.b_size = i_blocksize(inode),
	};

	get_block(inode, block, &tmp, 0);
	return tmp.b_blocknr;
}
EXPORT_SYMBOL(generic_block_bmap);

static void end_bio_bh_io_sync(struct bio *bio)
{
	struct buffer_head *bh = bio->bi_private;

	if (unlikely(bio_flagged(bio, BIO_QUIET)))
		set_bit(BH_Quiet, &bh->b_state);

	bh->b_end_io(bh, !bio->bi_status);
	bio_put(bio);
}

static void submit_bh_wbc(blk_opf_t opf, struct buffer_head *bh,
			  struct writeback_control *wbc)
{
	const enum req_op op = opf & REQ_OP_MASK;
	struct bio *bio;

	BUG_ON(!buffer_locked(bh));
	BUG_ON(!buffer_mapped(bh));
	BUG_ON(!bh->b_end_io);
	BUG_ON(buffer_delay(bh));
	BUG_ON(buffer_unwritten(bh));

	 
	if (test_set_buffer_req(bh) && (op == REQ_OP_WRITE))
		clear_buffer_write_io_error(bh);

	if (buffer_meta(bh))
		opf |= REQ_META;
	if (buffer_prio(bh))
		opf |= REQ_PRIO;

	bio = bio_alloc(bh->b_bdev, 1, opf, GFP_NOIO);

	fscrypt_set_bio_crypt_ctx_bh(bio, bh, GFP_NOIO);

	bio->bi_iter.bi_sector = bh->b_blocknr * (bh->b_size >> 9);

	__bio_add_page(bio, bh->b_page, bh->b_size, bh_offset(bh));

	bio->bi_end_io = end_bio_bh_io_sync;
	bio->bi_private = bh;

	 
	guard_bio_eod(bio);

	if (wbc) {
		wbc_init_bio(wbc, bio);
		wbc_account_cgroup_owner(wbc, bh->b_page, bh->b_size);
	}

	submit_bio(bio);
}

void submit_bh(blk_opf_t opf, struct buffer_head *bh)
{
	submit_bh_wbc(opf, bh, NULL);
}
EXPORT_SYMBOL(submit_bh);

void write_dirty_buffer(struct buffer_head *bh, blk_opf_t op_flags)
{
	lock_buffer(bh);
	if (!test_clear_buffer_dirty(bh)) {
		unlock_buffer(bh);
		return;
	}
	bh->b_end_io = end_buffer_write_sync;
	get_bh(bh);
	submit_bh(REQ_OP_WRITE | op_flags, bh);
}
EXPORT_SYMBOL(write_dirty_buffer);

 
int __sync_dirty_buffer(struct buffer_head *bh, blk_opf_t op_flags)
{
	WARN_ON(atomic_read(&bh->b_count) < 1);
	lock_buffer(bh);
	if (test_clear_buffer_dirty(bh)) {
		 
		if (!buffer_mapped(bh)) {
			unlock_buffer(bh);
			return -EIO;
		}

		get_bh(bh);
		bh->b_end_io = end_buffer_write_sync;
		submit_bh(REQ_OP_WRITE | op_flags, bh);
		wait_on_buffer(bh);
		if (!buffer_uptodate(bh))
			return -EIO;
	} else {
		unlock_buffer(bh);
	}
	return 0;
}
EXPORT_SYMBOL(__sync_dirty_buffer);

int sync_dirty_buffer(struct buffer_head *bh)
{
	return __sync_dirty_buffer(bh, REQ_SYNC);
}
EXPORT_SYMBOL(sync_dirty_buffer);

 
static inline int buffer_busy(struct buffer_head *bh)
{
	return atomic_read(&bh->b_count) |
		(bh->b_state & ((1 << BH_Dirty) | (1 << BH_Lock)));
}

static bool
drop_buffers(struct folio *folio, struct buffer_head **buffers_to_free)
{
	struct buffer_head *head = folio_buffers(folio);
	struct buffer_head *bh;

	bh = head;
	do {
		if (buffer_busy(bh))
			goto failed;
		bh = bh->b_this_page;
	} while (bh != head);

	do {
		struct buffer_head *next = bh->b_this_page;

		if (bh->b_assoc_map)
			__remove_assoc_queue(bh);
		bh = next;
	} while (bh != head);
	*buffers_to_free = head;
	folio_detach_private(folio);
	return true;
failed:
	return false;
}

bool try_to_free_buffers(struct folio *folio)
{
	struct address_space * const mapping = folio->mapping;
	struct buffer_head *buffers_to_free = NULL;
	bool ret = 0;

	BUG_ON(!folio_test_locked(folio));
	if (folio_test_writeback(folio))
		return false;

	if (mapping == NULL) {		 
		ret = drop_buffers(folio, &buffers_to_free);
		goto out;
	}

	spin_lock(&mapping->private_lock);
	ret = drop_buffers(folio, &buffers_to_free);

	 
	if (ret)
		folio_cancel_dirty(folio);
	spin_unlock(&mapping->private_lock);
out:
	if (buffers_to_free) {
		struct buffer_head *bh = buffers_to_free;

		do {
			struct buffer_head *next = bh->b_this_page;
			free_buffer_head(bh);
			bh = next;
		} while (bh != buffers_to_free);
	}
	return ret;
}
EXPORT_SYMBOL(try_to_free_buffers);

 
static struct kmem_cache *bh_cachep __read_mostly;

 
static unsigned long max_buffer_heads;

int buffer_heads_over_limit;

struct bh_accounting {
	int nr;			 
	int ratelimit;		 
};

static DEFINE_PER_CPU(struct bh_accounting, bh_accounting) = {0, 0};

static void recalc_bh_state(void)
{
	int i;
	int tot = 0;

	if (__this_cpu_inc_return(bh_accounting.ratelimit) - 1 < 4096)
		return;
	__this_cpu_write(bh_accounting.ratelimit, 0);
	for_each_online_cpu(i)
		tot += per_cpu(bh_accounting, i).nr;
	buffer_heads_over_limit = (tot > max_buffer_heads);
}

struct buffer_head *alloc_buffer_head(gfp_t gfp_flags)
{
	struct buffer_head *ret = kmem_cache_zalloc(bh_cachep, gfp_flags);
	if (ret) {
		INIT_LIST_HEAD(&ret->b_assoc_buffers);
		spin_lock_init(&ret->b_uptodate_lock);
		preempt_disable();
		__this_cpu_inc(bh_accounting.nr);
		recalc_bh_state();
		preempt_enable();
	}
	return ret;
}
EXPORT_SYMBOL(alloc_buffer_head);

void free_buffer_head(struct buffer_head *bh)
{
	BUG_ON(!list_empty(&bh->b_assoc_buffers));
	kmem_cache_free(bh_cachep, bh);
	preempt_disable();
	__this_cpu_dec(bh_accounting.nr);
	recalc_bh_state();
	preempt_enable();
}
EXPORT_SYMBOL(free_buffer_head);

static int buffer_exit_cpu_dead(unsigned int cpu)
{
	int i;
	struct bh_lru *b = &per_cpu(bh_lrus, cpu);

	for (i = 0; i < BH_LRU_SIZE; i++) {
		brelse(b->bhs[i]);
		b->bhs[i] = NULL;
	}
	this_cpu_add(bh_accounting.nr, per_cpu(bh_accounting, cpu).nr);
	per_cpu(bh_accounting, cpu).nr = 0;
	return 0;
}

 
int bh_uptodate_or_lock(struct buffer_head *bh)
{
	if (!buffer_uptodate(bh)) {
		lock_buffer(bh);
		if (!buffer_uptodate(bh))
			return 0;
		unlock_buffer(bh);
	}
	return 1;
}
EXPORT_SYMBOL(bh_uptodate_or_lock);

 
int __bh_read(struct buffer_head *bh, blk_opf_t op_flags, bool wait)
{
	int ret = 0;

	BUG_ON(!buffer_locked(bh));

	get_bh(bh);
	bh->b_end_io = end_buffer_read_sync;
	submit_bh(REQ_OP_READ | op_flags, bh);
	if (wait) {
		wait_on_buffer(bh);
		if (!buffer_uptodate(bh))
			ret = -EIO;
	}
	return ret;
}
EXPORT_SYMBOL(__bh_read);

 
void __bh_read_batch(int nr, struct buffer_head *bhs[],
		     blk_opf_t op_flags, bool force_lock)
{
	int i;

	for (i = 0; i < nr; i++) {
		struct buffer_head *bh = bhs[i];

		if (buffer_uptodate(bh))
			continue;

		if (force_lock)
			lock_buffer(bh);
		else
			if (!trylock_buffer(bh))
				continue;

		if (buffer_uptodate(bh)) {
			unlock_buffer(bh);
			continue;
		}

		bh->b_end_io = end_buffer_read_sync;
		get_bh(bh);
		submit_bh(REQ_OP_READ | op_flags, bh);
	}
}
EXPORT_SYMBOL(__bh_read_batch);

void __init buffer_init(void)
{
	unsigned long nrpages;
	int ret;

	bh_cachep = kmem_cache_create("buffer_head",
			sizeof(struct buffer_head), 0,
				(SLAB_RECLAIM_ACCOUNT|SLAB_PANIC|
				SLAB_MEM_SPREAD),
				NULL);

	 
	nrpages = (nr_free_buffer_pages() * 10) / 100;
	max_buffer_heads = nrpages * (PAGE_SIZE / sizeof(struct buffer_head));
	ret = cpuhp_setup_state_nocalls(CPUHP_FS_BUFF_DEAD, "fs/buffer:dead",
					NULL, buffer_exit_cpu_dead);
	WARN_ON(ret < 0);
}
