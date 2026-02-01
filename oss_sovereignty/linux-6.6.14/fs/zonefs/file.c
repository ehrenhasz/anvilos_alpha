
 
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/iomap.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/statfs.h>
#include <linux/writeback.h>
#include <linux/quotaops.h>
#include <linux/seq_file.h>
#include <linux/parser.h>
#include <linux/uio.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/task_io_accounting_ops.h>

#include "zonefs.h"

#include "trace.h"

static int zonefs_read_iomap_begin(struct inode *inode, loff_t offset,
				   loff_t length, unsigned int flags,
				   struct iomap *iomap, struct iomap *srcmap)
{
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	struct super_block *sb = inode->i_sb;
	loff_t isize;

	 
	mutex_lock(&zi->i_truncate_mutex);
	iomap->bdev = inode->i_sb->s_bdev;
	iomap->offset = ALIGN_DOWN(offset, sb->s_blocksize);
	isize = i_size_read(inode);
	if (iomap->offset >= isize) {
		iomap->type = IOMAP_HOLE;
		iomap->addr = IOMAP_NULL_ADDR;
		iomap->length = length;
	} else {
		iomap->type = IOMAP_MAPPED;
		iomap->addr = (z->z_sector << SECTOR_SHIFT) + iomap->offset;
		iomap->length = isize - iomap->offset;
	}
	mutex_unlock(&zi->i_truncate_mutex);

	trace_zonefs_iomap_begin(inode, iomap);

	return 0;
}

static const struct iomap_ops zonefs_read_iomap_ops = {
	.iomap_begin	= zonefs_read_iomap_begin,
};

static int zonefs_write_iomap_begin(struct inode *inode, loff_t offset,
				    loff_t length, unsigned int flags,
				    struct iomap *iomap, struct iomap *srcmap)
{
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	struct super_block *sb = inode->i_sb;
	loff_t isize;

	 
	if (WARN_ON_ONCE(offset + length > z->z_capacity))
		return -EIO;

	 
	if (WARN_ON_ONCE(zonefs_zone_is_seq(z) && !(flags & IOMAP_DIRECT)))
		return -EIO;

	 
	mutex_lock(&zi->i_truncate_mutex);
	iomap->bdev = inode->i_sb->s_bdev;
	iomap->offset = ALIGN_DOWN(offset, sb->s_blocksize);
	iomap->addr = (z->z_sector << SECTOR_SHIFT) + iomap->offset;
	isize = i_size_read(inode);
	if (iomap->offset >= isize) {
		iomap->type = IOMAP_UNWRITTEN;
		iomap->length = z->z_capacity - iomap->offset;
	} else {
		iomap->type = IOMAP_MAPPED;
		iomap->length = isize - iomap->offset;
	}
	mutex_unlock(&zi->i_truncate_mutex);

	trace_zonefs_iomap_begin(inode, iomap);

	return 0;
}

static const struct iomap_ops zonefs_write_iomap_ops = {
	.iomap_begin	= zonefs_write_iomap_begin,
};

static int zonefs_read_folio(struct file *unused, struct folio *folio)
{
	return iomap_read_folio(folio, &zonefs_read_iomap_ops);
}

static void zonefs_readahead(struct readahead_control *rac)
{
	iomap_readahead(rac, &zonefs_read_iomap_ops);
}

 
static int zonefs_write_map_blocks(struct iomap_writepage_ctx *wpc,
				   struct inode *inode, loff_t offset)
{
	struct zonefs_zone *z = zonefs_inode_zone(inode);

	if (WARN_ON_ONCE(zonefs_zone_is_seq(z)))
		return -EIO;
	if (WARN_ON_ONCE(offset >= i_size_read(inode)))
		return -EIO;

	 
	if (offset >= wpc->iomap.offset &&
	    offset < wpc->iomap.offset + wpc->iomap.length)
		return 0;

	return zonefs_write_iomap_begin(inode, offset,
					z->z_capacity - offset,
					IOMAP_WRITE, &wpc->iomap, NULL);
}

static const struct iomap_writeback_ops zonefs_writeback_ops = {
	.map_blocks		= zonefs_write_map_blocks,
};

static int zonefs_writepages(struct address_space *mapping,
			     struct writeback_control *wbc)
{
	struct iomap_writepage_ctx wpc = { };

	return iomap_writepages(mapping, wbc, &wpc, &zonefs_writeback_ops);
}

static int zonefs_swap_activate(struct swap_info_struct *sis,
				struct file *swap_file, sector_t *span)
{
	struct inode *inode = file_inode(swap_file);

	if (zonefs_inode_is_seq(inode)) {
		zonefs_err(inode->i_sb,
			   "swap file: not a conventional zone file\n");
		return -EINVAL;
	}

	return iomap_swapfile_activate(sis, swap_file, span,
				       &zonefs_read_iomap_ops);
}

const struct address_space_operations zonefs_file_aops = {
	.read_folio		= zonefs_read_folio,
	.readahead		= zonefs_readahead,
	.writepages		= zonefs_writepages,
	.dirty_folio		= iomap_dirty_folio,
	.release_folio		= iomap_release_folio,
	.invalidate_folio	= iomap_invalidate_folio,
	.migrate_folio		= filemap_migrate_folio,
	.is_partially_uptodate	= iomap_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
	.swap_activate		= zonefs_swap_activate,
};

int zonefs_file_truncate(struct inode *inode, loff_t isize)
{
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	loff_t old_isize;
	enum req_op op;
	int ret = 0;

	 
	if (!zonefs_zone_is_seq(z))
		return -EPERM;

	if (!isize)
		op = REQ_OP_ZONE_RESET;
	else if (isize == z->z_capacity)
		op = REQ_OP_ZONE_FINISH;
	else
		return -EPERM;

	inode_dio_wait(inode);

	 
	filemap_invalidate_lock(inode->i_mapping);

	 
	mutex_lock(&zi->i_truncate_mutex);

	old_isize = i_size_read(inode);
	if (isize == old_isize)
		goto unlock;

	ret = zonefs_inode_zone_mgmt(inode, op);
	if (ret)
		goto unlock;

	 
	if (z->z_flags & ZONEFS_ZONE_OPEN) {
		 
		if (!isize)
			ret = zonefs_inode_zone_mgmt(inode, REQ_OP_ZONE_OPEN);
		else
			z->z_flags &= ~ZONEFS_ZONE_OPEN;
	}

	zonefs_update_stats(inode, isize);
	truncate_setsize(inode, isize);
	z->z_wpoffset = isize;
	zonefs_inode_account_active(inode);

unlock:
	mutex_unlock(&zi->i_truncate_mutex);
	filemap_invalidate_unlock(inode->i_mapping);

	return ret;
}

static int zonefs_file_fsync(struct file *file, loff_t start, loff_t end,
			     int datasync)
{
	struct inode *inode = file_inode(file);
	int ret = 0;

	if (unlikely(IS_IMMUTABLE(inode)))
		return -EPERM;

	 
	if (zonefs_inode_is_cnv(inode))
		ret = file_write_and_wait_range(file, start, end);
	if (!ret)
		ret = blkdev_issue_flush(inode->i_sb->s_bdev);

	if (ret)
		zonefs_io_error(inode, true);

	return ret;
}

static vm_fault_t zonefs_filemap_page_mkwrite(struct vm_fault *vmf)
{
	struct inode *inode = file_inode(vmf->vma->vm_file);
	vm_fault_t ret;

	if (unlikely(IS_IMMUTABLE(inode)))
		return VM_FAULT_SIGBUS;

	 
	if (zonefs_inode_is_seq(inode))
		return VM_FAULT_NOPAGE;

	sb_start_pagefault(inode->i_sb);
	file_update_time(vmf->vma->vm_file);

	 
	filemap_invalidate_lock_shared(inode->i_mapping);
	ret = iomap_page_mkwrite(vmf, &zonefs_write_iomap_ops);
	filemap_invalidate_unlock_shared(inode->i_mapping);

	sb_end_pagefault(inode->i_sb);
	return ret;
}

static const struct vm_operations_struct zonefs_file_vm_ops = {
	.fault		= filemap_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite	= zonefs_filemap_page_mkwrite,
};

static int zonefs_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	 
	if (zonefs_inode_is_seq(file_inode(file)) &&
	    (vma->vm_flags & VM_SHARED) && (vma->vm_flags & VM_MAYWRITE))
		return -EINVAL;

	file_accessed(file);
	vma->vm_ops = &zonefs_file_vm_ops;

	return 0;
}

static loff_t zonefs_file_llseek(struct file *file, loff_t offset, int whence)
{
	loff_t isize = i_size_read(file_inode(file));

	 
	return generic_file_llseek_size(file, offset, whence, isize, isize);
}

static int zonefs_file_write_dio_end_io(struct kiocb *iocb, ssize_t size,
					int error, unsigned int flags)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	struct zonefs_inode_info *zi = ZONEFS_I(inode);

	if (error) {
		zonefs_io_error(inode, true);
		return error;
	}

	if (size && zonefs_inode_is_seq(inode)) {
		 
		mutex_lock(&zi->i_truncate_mutex);
		if (i_size_read(inode) < iocb->ki_pos + size) {
			zonefs_update_stats(inode, iocb->ki_pos + size);
			zonefs_i_size_write(inode, iocb->ki_pos + size);
		}
		mutex_unlock(&zi->i_truncate_mutex);
	}

	return 0;
}

static const struct iomap_dio_ops zonefs_write_dio_ops = {
	.end_io		= zonefs_file_write_dio_end_io,
};

 
static loff_t zonefs_write_check_limits(struct file *file, loff_t pos,
					loff_t count)
{
	struct inode *inode = file_inode(file);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	loff_t limit = rlimit(RLIMIT_FSIZE);
	loff_t max_size = z->z_capacity;

	if (limit != RLIM_INFINITY) {
		if (pos >= limit) {
			send_sig(SIGXFSZ, current, 0);
			return -EFBIG;
		}
		count = min(count, limit - pos);
	}

	if (!(file->f_flags & O_LARGEFILE))
		max_size = min_t(loff_t, MAX_NON_LFS, max_size);

	if (unlikely(pos >= max_size))
		return -EFBIG;

	return min(count, max_size - pos);
}

static ssize_t zonefs_write_checks(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file_inode(file);
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	loff_t count;

	if (IS_SWAPFILE(inode))
		return -ETXTBSY;

	if (!iov_iter_count(from))
		return 0;

	if ((iocb->ki_flags & IOCB_NOWAIT) && !(iocb->ki_flags & IOCB_DIRECT))
		return -EINVAL;

	if (iocb->ki_flags & IOCB_APPEND) {
		if (zonefs_zone_is_cnv(z))
			return -EINVAL;
		mutex_lock(&zi->i_truncate_mutex);
		iocb->ki_pos = z->z_wpoffset;
		mutex_unlock(&zi->i_truncate_mutex);
	}

	count = zonefs_write_check_limits(file, iocb->ki_pos,
					  iov_iter_count(from));
	if (count < 0)
		return count;

	iov_iter_truncate(from, count);
	return iov_iter_count(from);
}

 
static ssize_t zonefs_file_dio_write(struct kiocb *iocb, struct iov_iter *from)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	struct super_block *sb = inode->i_sb;
	ssize_t ret, count;

	 
	if (zonefs_zone_is_seq(z) && !is_sync_kiocb(iocb) &&
	    (iocb->ki_flags & IOCB_NOWAIT))
		return -EOPNOTSUPP;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock(inode))
			return -EAGAIN;
	} else {
		inode_lock(inode);
	}

	count = zonefs_write_checks(iocb, from);
	if (count <= 0) {
		ret = count;
		goto inode_unlock;
	}

	if ((iocb->ki_pos | count) & (sb->s_blocksize - 1)) {
		ret = -EINVAL;
		goto inode_unlock;
	}

	 
	if (zonefs_zone_is_seq(z)) {
		mutex_lock(&zi->i_truncate_mutex);
		if (iocb->ki_pos != z->z_wpoffset) {
			mutex_unlock(&zi->i_truncate_mutex);
			ret = -EINVAL;
			goto inode_unlock;
		}
		mutex_unlock(&zi->i_truncate_mutex);
	}

	 
	ret = iomap_dio_rw(iocb, from, &zonefs_write_iomap_ops,
			   &zonefs_write_dio_ops, 0, NULL, 0);
	if (ret == -ENOTBLK)
		ret = -EBUSY;

	if (zonefs_zone_is_seq(z) &&
	    (ret > 0 || ret == -EIOCBQUEUED)) {
		if (ret > 0)
			count = ret;

		 
		mutex_lock(&zi->i_truncate_mutex);
		z->z_wpoffset += count;
		zonefs_inode_account_active(inode);
		mutex_unlock(&zi->i_truncate_mutex);
	}

inode_unlock:
	inode_unlock(inode);

	return ret;
}

static ssize_t zonefs_file_buffered_write(struct kiocb *iocb,
					  struct iov_iter *from)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	ssize_t ret;

	 
	if (zonefs_inode_is_seq(inode))
		return -EIO;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock(inode))
			return -EAGAIN;
	} else {
		inode_lock(inode);
	}

	ret = zonefs_write_checks(iocb, from);
	if (ret <= 0)
		goto inode_unlock;

	ret = iomap_file_buffered_write(iocb, from, &zonefs_write_iomap_ops);
	if (ret == -EIO)
		zonefs_io_error(inode, true);

inode_unlock:
	inode_unlock(inode);
	if (ret > 0)
		ret = generic_write_sync(iocb, ret);

	return ret;
}

static ssize_t zonefs_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	struct zonefs_zone *z = zonefs_inode_zone(inode);

	if (unlikely(IS_IMMUTABLE(inode)))
		return -EPERM;

	if (sb_rdonly(inode->i_sb))
		return -EROFS;

	 
	if (iocb->ki_pos >= z->z_capacity)
		return -EFBIG;

	if (iocb->ki_flags & IOCB_DIRECT) {
		ssize_t ret = zonefs_file_dio_write(iocb, from);

		if (ret != -ENOTBLK)
			return ret;
	}

	return zonefs_file_buffered_write(iocb, from);
}

static int zonefs_file_read_dio_end_io(struct kiocb *iocb, ssize_t size,
				       int error, unsigned int flags)
{
	if (error) {
		zonefs_io_error(file_inode(iocb->ki_filp), false);
		return error;
	}

	return 0;
}

static const struct iomap_dio_ops zonefs_read_dio_ops = {
	.end_io			= zonefs_file_read_dio_end_io,
};

static ssize_t zonefs_file_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct inode *inode = file_inode(iocb->ki_filp);
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	struct super_block *sb = inode->i_sb;
	loff_t isize;
	ssize_t ret;

	 
	if (unlikely(IS_IMMUTABLE(inode) && !(inode->i_mode & 0777)))
		return -EPERM;

	if (iocb->ki_pos >= z->z_capacity)
		return 0;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!inode_trylock_shared(inode))
			return -EAGAIN;
	} else {
		inode_lock_shared(inode);
	}

	 
	mutex_lock(&zi->i_truncate_mutex);
	isize = i_size_read(inode);
	if (iocb->ki_pos >= isize) {
		mutex_unlock(&zi->i_truncate_mutex);
		ret = 0;
		goto inode_unlock;
	}
	iov_iter_truncate(to, isize - iocb->ki_pos);
	mutex_unlock(&zi->i_truncate_mutex);

	if (iocb->ki_flags & IOCB_DIRECT) {
		size_t count = iov_iter_count(to);

		if ((iocb->ki_pos | count) & (sb->s_blocksize - 1)) {
			ret = -EINVAL;
			goto inode_unlock;
		}
		file_accessed(iocb->ki_filp);
		ret = iomap_dio_rw(iocb, to, &zonefs_read_iomap_ops,
				   &zonefs_read_dio_ops, 0, NULL, 0);
	} else {
		ret = generic_file_read_iter(iocb, to);
		if (ret == -EIO)
			zonefs_io_error(inode, false);
	}

inode_unlock:
	inode_unlock_shared(inode);

	return ret;
}

static ssize_t zonefs_file_splice_read(struct file *in, loff_t *ppos,
				       struct pipe_inode_info *pipe,
				       size_t len, unsigned int flags)
{
	struct inode *inode = file_inode(in);
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	loff_t isize;
	ssize_t ret = 0;

	 
	if (unlikely(IS_IMMUTABLE(inode) && !(inode->i_mode & 0777)))
		return -EPERM;

	if (*ppos >= z->z_capacity)
		return 0;

	inode_lock_shared(inode);

	 
	mutex_lock(&zi->i_truncate_mutex);
	isize = i_size_read(inode);
	if (*ppos >= isize)
		len = 0;
	else
		len = min_t(loff_t, len, isize - *ppos);
	mutex_unlock(&zi->i_truncate_mutex);

	if (len > 0) {
		ret = filemap_splice_read(in, ppos, pipe, len, flags);
		if (ret == -EIO)
			zonefs_io_error(inode, false);
	}

	inode_unlock_shared(inode);
	return ret;
}

 
static inline bool zonefs_seq_file_need_wro(struct inode *inode,
					    struct file *file)
{
	if (zonefs_inode_is_cnv(inode))
		return false;

	if (!(file->f_mode & FMODE_WRITE))
		return false;

	return true;
}

static int zonefs_seq_file_write_open(struct inode *inode)
{
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	int ret = 0;

	mutex_lock(&zi->i_truncate_mutex);

	if (!zi->i_wr_refcnt) {
		struct zonefs_sb_info *sbi = ZONEFS_SB(inode->i_sb);
		unsigned int wro = atomic_inc_return(&sbi->s_wro_seq_files);

		if (sbi->s_mount_opts & ZONEFS_MNTOPT_EXPLICIT_OPEN) {

			if (sbi->s_max_wro_seq_files
			    && wro > sbi->s_max_wro_seq_files) {
				atomic_dec(&sbi->s_wro_seq_files);
				ret = -EBUSY;
				goto unlock;
			}

			if (i_size_read(inode) < z->z_capacity) {
				ret = zonefs_inode_zone_mgmt(inode,
							     REQ_OP_ZONE_OPEN);
				if (ret) {
					atomic_dec(&sbi->s_wro_seq_files);
					goto unlock;
				}
				z->z_flags |= ZONEFS_ZONE_OPEN;
				zonefs_inode_account_active(inode);
			}
		}
	}

	zi->i_wr_refcnt++;

unlock:
	mutex_unlock(&zi->i_truncate_mutex);

	return ret;
}

static int zonefs_file_open(struct inode *inode, struct file *file)
{
	int ret;

	file->f_mode |= FMODE_CAN_ODIRECT;
	ret = generic_file_open(inode, file);
	if (ret)
		return ret;

	if (zonefs_seq_file_need_wro(inode, file))
		return zonefs_seq_file_write_open(inode);

	return 0;
}

static void zonefs_seq_file_write_close(struct inode *inode)
{
	struct zonefs_inode_info *zi = ZONEFS_I(inode);
	struct zonefs_zone *z = zonefs_inode_zone(inode);
	struct super_block *sb = inode->i_sb;
	struct zonefs_sb_info *sbi = ZONEFS_SB(sb);
	int ret = 0;

	mutex_lock(&zi->i_truncate_mutex);

	zi->i_wr_refcnt--;
	if (zi->i_wr_refcnt)
		goto unlock;

	 
	if (z->z_flags & ZONEFS_ZONE_OPEN) {
		ret = zonefs_inode_zone_mgmt(inode, REQ_OP_ZONE_CLOSE);
		if (ret) {
			__zonefs_io_error(inode, false);
			 
			if (z->z_flags & ZONEFS_ZONE_OPEN &&
			    !(sb->s_flags & SB_RDONLY)) {
				zonefs_warn(sb,
					"closing zone at %llu failed %d\n",
					z->z_sector, ret);
				zonefs_warn(sb,
					"remounting filesystem read-only\n");
				sb->s_flags |= SB_RDONLY;
			}
			goto unlock;
		}

		z->z_flags &= ~ZONEFS_ZONE_OPEN;
		zonefs_inode_account_active(inode);
	}

	atomic_dec(&sbi->s_wro_seq_files);

unlock:
	mutex_unlock(&zi->i_truncate_mutex);
}

static int zonefs_file_release(struct inode *inode, struct file *file)
{
	 
	if (zonefs_seq_file_need_wro(inode, file))
		zonefs_seq_file_write_close(inode);

	return 0;
}

const struct file_operations zonefs_file_operations = {
	.open		= zonefs_file_open,
	.release	= zonefs_file_release,
	.fsync		= zonefs_file_fsync,
	.mmap		= zonefs_file_mmap,
	.llseek		= zonefs_file_llseek,
	.read_iter	= zonefs_file_read_iter,
	.write_iter	= zonefs_file_write_iter,
	.splice_read	= zonefs_file_splice_read,
	.splice_write	= iter_file_splice_write,
	.iopoll		= iocb_bio_iopoll,
};
